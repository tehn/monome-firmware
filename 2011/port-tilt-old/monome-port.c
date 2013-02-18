// TODO
//
// eeprom storage -- size, id, offsets(?)
// serilosc sys responses

#define FIRMWARE_VERSION 0

#define F_CPU 20000000UL

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include "twi.h"

// usb pins
#define C0_TXE 0x01
#define C1_RXF 0x02
#define C2_RD 0x04
#define C3_WR 0x08
#define B0_PWREN 0x01

// tuning
#define OUTPUT_BUFFER_LENGTH 256
#define INPUT_BUFFER_LENGTH 32
#define RX_STARVE_THRESH 20
#define GATHER_RATE 100			// 40 = 2ms


// i2c protocol
#define _QUERY 0
#define _SLEEP 1
#define _WAKE 2

#define _KEYPAD_REQUEST 0x20

	
// usb protocol incoming

#define _SYS_QUERY 0x00
#define _SYS_QUERY_ID 0x01
#define _SYS_WRITE_ID 0x02
#define _SYS_GET_GRID_OFFSET 0x03
#define _SYS_SET_GRID_OFFSET 0x04
#define _SYS_GET_GRID_SIZE 0x05
#define _SYS_SET_GRID_SIZE 0x06
#define _SYS_SCAN_ADDR 0x07
#define _SYS_SET_ADDR 0x08
#define _SYS_QUERY_VERSION 0x0F

#define _LED_SET0 0x10
#define _LED_SET1 0x11
#define _LED_ALL0 0x12
#define _LED_ALL1 0x13
#define _LED_MAP 0x14
#define _LED_ROW 0x15
#define _LED_COL 0x16
#define _LED_INT 0x17

#define _LED_SETX 0x18
#define _LED_ALLX 0x19
#define _LED_MAPX 0x1A
#define _LED_ROWX 0x1B
#define _LED_COLX 0x1C



// usb protocol outgoing

#define _SYS_QUERY_RESPONSE 0x00
#define _SYS_ID 0x01
#define _SYS_REPORT_GRID_OFFSET 0x02
#define _SYS_REPORT_GRID_SIZE 0x03
#define _SYS_FOUND_ADDR 0x04
#define _SYS_REPORT_VERSION 0x05

#define _KEY 32

const uint8_t usb_packet_length[256] = {
	1,1,33,1,4,1,3,1,3,0,0,0,0,0,0,1,
	3,3,1,1,11,4,4,2, 4,2,35,7,7,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

// globals
uint8_t n1, n2, n3, n4;
volatile uint8_t input_check;



// ISR(TIMER1_OVF_vect)
// {
// 	TCNT1 = 0;
// }

ISR(TIMER0_COMPA_vect)
{
	// PORTB ^= 0xff;
	input_check = 1;
	TCNT0 = 0;
}

// void processRx(uint8_t *data, int index)
// {
// 	//PORTB ^= 0xff;
// }


int main(void)
{
	uint8_t i1, i2, i3, i4, i5;
	int16_t x, y, z;
	uint8_t rx_count, rx_length, rx_type, rx_timeout, rx_starve;
	uint8_t addr;
	uint8_t rx[80];	// usb input buffer
	uint8_t d[80];
	uint8_t awake;
	uint8_t found_sub[4] = {0,0,0,0};
	uint8_t offset_x[4] = {0,8,0,8};
	uint8_t offset_y[4] = {0,0,8,8};
	uint8_t lookup_sub[2][2] = { {100,102} , {104,106} };
	uint8_t size_x, size_y;
	
	char id[32];
	
	uint8_t output_buffer[OUTPUT_BUFFER_LENGTH];
	uint8_t output_write;
	uint8_t output_read;
	uint8_t input_buffer[INPUT_BUFFER_LENGTH];

	i1 = i2 = i3 = i4 = 0;
	rx_count = rx_type = rx_timeout = 0;
	rx_length = 1;
	input_check = 0;
	awake = 1;

	output_read = output_write = 0;

	DDRB = 0;
	DDRC = C3_WR | C2_RD;
	DDRD = 0;

	TCCR0A = 0;
	TCCR0B = (1<<CS02) | (1<<CS00);
	TIMSK0 = (1 << OCIE0A);
	OCR0A = GATHER_RATE;

	// TCCR1A = 0;
	// TCCR1B = (1<<CS10) | (1<<CS12);
	// TIMSK1 = (1<<TOIE1);

	sei();

	twi_init();
	//twi_setAddress(0);
	// twi_attachSlaveRxEvent(processRx);

	//// scan i2c bus //////////////////////////////////////////////////////
	
	// for(i1=0;i1<4;i1++) {
	// 	addr = 100 + (i1*2);
	// 	d[0] = _LED_ALL1;
	// 	twi_writeTo(addr, d, 1, 1);
	// }
	
	/*
	
	_delay_ms(50);	

	for(i1=0;i1<4;i1++) {
		addr = 100 + (i1*2);
		d[0] = _QUERY;
		i2 = twi_writeTo(addr, d, 1, 1);
		if(!i2) {
			found_sub[i1] = addr;
			// lookup_sub[offset_x[i1]>>8][offset_y[i1>>8]] = addr;
			//d[0] = _LED_ALL0;
			//twi_writeTo(addr, d, 1, 1);
		}
	}
	
	*/
	
	for(i1=0;i1<32;i1++) id[i1]=0;

	i1 = 0;
	for(i2=0;i2<4;i2++) {
		if(found_sub[i2]) i1++;
	}
	
	size_x = 0; size_y = 0;
	
	if(i1==0) {
		strcpy(id,"monome");
	} else if(i1==1) {
		size_x = 8; size_y = 8;
		strcpy(id,"monome 64");
	} else if(i1==2) {
		size_x = 16; size_y = 8;
		strcpy(id,"monome 128");
	} else if(i1==4) {
		size_x = 16; size_y = 16;
		strcpy(id,"monome 256");
	}


	// tilt setup
	
	addr = 0x53;

	d[0] = 0x2D; 	// write to power ctl
	d[1] = (1<<3);	// set measure bit on d3

	twi_writeTo(addr, d, 2, 1);
	
	
	/*

	// startup animation
	
	addr = 100;
	
	d[0] = _LED_COL;
	d[1] = 0;
	d[2] = 6;
	twi_writeTo(addr, d, 3, 1);
	_delay_ms(50);

	d[0] = _LED_COL;	
	d[2] = 9;
	twi_writeTo(addr, d, 3, 1);
	d[1] = 1;
	d[2] = 6;
	twi_writeTo(addr, d, 3, 1);
	d[0] = _LED_INT;
	d[1] = 12;
	twi_writeTo(addr, d, 2, 1);
	_delay_ms(50);

	d[0] = _LED_COL;
	d[1] = 0;
	d[2] = 16;
	twi_writeTo(addr, d, 3, 1);
	d[1] = 1;
	d[2] = 9;
	twi_writeTo(addr, d, 3, 1);
	d[1] = 2;
	d[2] = 6;
	twi_writeTo(addr, d, 3, 1);
	d[0] = _LED_INT;
	d[1] = 7;
	twi_writeTo(addr, d, 2, 1);
	_delay_ms(50);

	d[0] = _LED_COL;
	d[1] = 1;
	d[2] = 16;
	twi_writeTo(addr, d, 3, 1);
	d[1] = 2;
	d[2] = 9;
	twi_writeTo(addr, d, 3, 1);
	d[1] = 3;
	d[2] = 6;
	twi_writeTo(addr, d, 3, 1);
	d[0] = _LED_INT;
	d[1] = 2;
	twi_writeTo(addr, d, 2, 1);
	_delay_ms(50);
	
	d[0] = _LED_ALL0;
	twi_writeTo(addr, d, 1, 1);
	d[0] = _LED_INT;
	d[1] = 15;
	twi_writeTo(addr, d, 2, 1);

	*/


	//// main loop /////////////////////////////////////////////////////////
	while(1)
	{
		i1 = (PINB & B0_PWREN);
		if(i1 != awake) {
			if(i1) {
				addr = 100;
				d[0] = _SLEEP;
				twi_writeTo(addr, d, 1, 1);
			} else {
				addr = 100;
				d[0] = _WAKE;
				twi_writeTo(addr, d, 1, 1);
			}

			awake = i1;
		}

		// ====================== check/read incoming serial
		PORTD = 0;                  // setup PORTD for input
		DDRD = 0;                   // input w/ tristate

		if(rx_timeout > 40 ) {
			rx_count = 0;
		}
		else rx_timeout++;

		rx_starve = 0;

		while((PINC & C1_RXF) == 0 && rx_starve < RX_STARVE_THRESH) {
			rx_starve++;			// make sure we process keypad data...
									// if we process more input bytes than RX_STARVE
									// we'll jump to sending out waiting keypad bytes
									// and then continue
			PORTC &= ~(C2_RD);
			_delay_us(1);			// wait for valid data
			rx[rx_count] = PIND;
			PORTC |= C2_RD;

			if(rx_count == 0) {		// get packet length if reading first byte
				rx_type = rx[0];
				rx_length = usb_packet_length[rx_type];
				rx_count++;
				rx_timeout = 0;
			}
			else rx_count++;

			if(rx_count == rx_length) {
				rx_count = 0;
				
				if(rx_type == _SYS_QUERY) {
					i1 = 0;
					for(i2=0;i2<4;i2++) {
						if(found_sub[i2]) i1++;
					}
					
					output_buffer[output_write] = _SYS_QUERY_RESPONSE;
					output_write++;
					output_buffer[output_write] = 1;
					output_write++;
					output_buffer[output_write] = i1;
					output_write++;
					
					output_buffer[output_write] = _SYS_QUERY_RESPONSE;
					output_write++;
					output_buffer[output_write] = 2;
					output_write++;
					output_buffer[output_write] = i1;
					output_write++;
				}
				else if(rx_type == _SYS_QUERY_ID) {
					output_buffer[output_write] = _SYS_ID;
					output_write++;
					
					for(i1=0;i1<32;i1++) {
						output_buffer[output_write] = id[i1];
						output_write++;
					}
				}
				else if(rx_type == _SYS_WRITE_ID) {
				}
				else if(rx_type == _SYS_GET_GRID_OFFSET) {
				}
				else if(rx_type == _SYS_SET_GRID_OFFSET) {
				}
				else if(rx_type == _SYS_GET_GRID_SIZE) {
					output_buffer[output_write] = _SYS_REPORT_GRID_SIZE;
					output_write++;
					output_buffer[output_write] = size_x;
					output_write++;
					output_buffer[output_write] = size_y;
					output_write++;
				}
				else if(rx_type == _SYS_SET_GRID_SIZE) {
				}
				else if(rx_type == _SYS_SCAN_ADDR) {
				}
				else if(rx_type == _SYS_SET_ADDR) {
				}
				else if(rx_type == _SYS_QUERY_VERSION) {
					output_buffer[output_write] = _SYS_REPORT_VERSION;
					output_write++;
					output_buffer[output_write] = FIRMWARE_VERSION;
					output_write++;
				}
				
				else if(rx_type == _LED_SET0) {
					// _LED_SET0 //////////////////////////////////////////////
					i1 = rx[1] >> 3;
					i2 = rx[2] >> 3;
					i3 = rx[1] & 0x07;
					i4 = rx[2] & 0x07;
					
					addr = lookup_sub[i2][i1];

					d[0] = _LED_SET0;
					d[1] = (i3 << 4) + i4;
					twi_writeTo(addr, d, 2, 1);
				}
				else if(rx_type == _LED_SET1) {
					// _LED_SET1 //////////////////////////////////////////////
					i1 = rx[1] >> 3;
					i2 = rx[2] >> 3;
					i3 = rx[1] & 0x07;
					i4 = rx[2] & 0x07;
					
					addr = lookup_sub[i2][i1];

					d[0] = _LED_SET1;
					d[1] = (i3 << 4) + i4;					
					twi_writeTo(addr, d, 2, 1);
				}
				else if(rx_type == _LED_ALL0) {
					// _LED_ALL0 //////////////////////////////////////////////
					for(i1=0;i1<4;i1++) {
						if(found_sub[i1]) {
							addr = 100 + (i1*2);
							d[0] = _LED_ALL0;
							twi_writeTo(addr, d, 1, 1);
						}
					}
				}
				else if(rx_type == _LED_ALL1) {
					// _LED_ALL1 //////////////////////////////////////////////
					for(i1=0;i1<4;i1++) {
						if(found_sub[i1]) {
							addr = 100 + (i1*2);
							d[0] = _LED_ALL1;
							twi_writeTo(addr, d, 1, 1);
						}
					}
				}
				else if(rx_type == _LED_MAP) {
					// _LED_MAP ///////////////////////////////////////////////
					i1 = rx[1] >> 3;
					i2 = rx[2] >> 3;
					addr = lookup_sub[i2][i1];
					
					d[0] = _LED_MAP;
					d[1] = rx[3];
					d[2] = rx[4];
					d[3] = rx[5];
					d[4] = rx[6];
					d[5] = rx[7];
					d[6] = rx[8];
					d[7] = rx[9];
					d[8] = rx[10];
					twi_writeTo(addr, d, 9, 1);
				}
				else if(rx_type == _LED_ROW) {
					// _LED_ROW ///////////////////////////////////////////////
					// x offset is rx[1]
					i1 = rx[1] >> 3;
					i2 = rx[2] >> 3;
					addr = lookup_sub[i2][i1];

					d[0] = _LED_ROW;
					d[1] = rx[2] & 0x07;
					d[2] = rx[3];
					twi_writeTo(addr, d, 3, 1);
				}
				else if(rx_type == _LED_COL) {
					// _LED_COL ///////////////////////////////////////////////
					// y offset is rx[2]
					i1 = rx[1] >> 3;
					i2 = rx[2] >> 3;
					addr = lookup_sub[i2][i1];
					
					d[0] = _LED_COL;
					d[1] = rx[1] & 0x07;
					d[2] = rx[3];
					twi_writeTo(addr, d, 3, 1);
				}
				else if(rx_type == _LED_INT) {
					// _LED_INT ///////////////////////////////////////////////
					for(i1=0;i1<4;i1++) {
						if(found_sub[i1]) {
							addr = 100 + (i1*2);
							d[0] = _LED_INT;
							d[1] = rx[1] % 16;
							twi_writeTo(addr, d, 2, 1);
						}
					}
				}
				
				else if(rx_type == _LED_SETX) {
					// _LED_SETX //////////////////////////////////////////////
					i1 = rx[1] >> 3;
					i2 = rx[2] >> 3;
					i3 = rx[1] & 0x07;
					i4 = rx[2] & 0x07;
					addr = lookup_sub[i2][i1];

					d[0] = _LED_SETX;
					d[1] = (i3 << 4) + i4;
					d[2] = (rx[3] >> 2) & 3;
					twi_writeTo(addr, d, 3, 1);
				}
				else if(rx_type == _LED_ALLX) {
					// _LED_ALLX //////////////////////////////////////////////
					for(i1=0;i1<4;i1++) {
						if(found_sub[i1]) {
							addr = 100 + (i1*2);
							d[0] = _LED_ALLX;
							d[1] = (rx[1] >> 2) & 3;
							twi_writeTo(addr, d, 2, 1);
						}
					}
				}
				else if(rx_type == _LED_MAPX) {
					// _LED_MAPX //////////////////////////////////////////////
					i1 = rx[1] >> 3;
					i2 = rx[2] >> 3;
					addr = lookup_sub[i2][i1];
					
					// d[0] = _LED_MAPX;
					// for(i3 = 0; i3<64; i3++) {
					// 	d[i3+1] = rx[i3+3] & 3;
					// }
					// 
					// twi_writeTo(addr, d, 65, 1);
					
					for(i4=0;i4<8;i4++) {
						d[0] = _LED_ROWX;
						d[1] = i4;
						for(i3 = 0; i3<4; i3++) {
							d[(i3*2)+2] = (rx[i3+3+(i4*4)] >> 6) & 3;
							d[(i3*2)+3] = (rx[i3+3+(i4*4)] >> 2) & 3;
						}

						twi_writeTo(addr, d, 10, 1);
					}
				}
				else if(rx_type == _LED_ROWX) {
					// _LED_ROWX //////////////////////////////////////////////
					// x offset is rx[1]
					i1 = rx[1] >> 3;
					i2 = rx[2] >> 3;
					addr = lookup_sub[i2][i1];

					d[0] = _LED_ROWX;
					d[1] = rx[2] & 0x07;
					for(i3 = 0; i3<4; i3++) {
						d[(i3*2)+2] = (rx[i3+3] >> 6) & 3;
						d[(i3*2)+3] = (rx[i3+3] >> 2) & 3;
					}
					
					twi_writeTo(addr, d, 10, 1);
				}
				else if(rx_type == _LED_COLX) {
					// _LED_COLX //////////////////////////////////////////////
					// y offset is rx[2]
					i1 = rx[1] >> 3;
					i2 = rx[2] >> 3;
					addr = lookup_sub[i2][i1];
					
					d[0] = _LED_COLX;
					d[1] = rx[1] & 0x07;
					for(i3 = 0; i3<4; i3++) {
						d[(i3*2)+2] = (rx[i3+3] >> 6) & 3;
						d[(i3*2)+3] = (rx[i3+3] >> 2) & 3;
						// d[i3+2] = rx[i3+3] & 3;
					}
					twi_writeTo(addr, d, 10, 1);
				}
			}
		}

		//// check/send output  ////////////////////////////////////////////
		if(input_check) {
			input_check = 0;

			for(i5=0;i5<4;i5++) {
				if(found_sub[i5]) {
					input_buffer[0] = 255;
					twi_readFrom(100 + (i5*2), input_buffer, 1);
					i1 = input_buffer[0];
					if(i1 != 255) {
						i2 = (i1 & 0x80) == 0;
						i3 = (i1 & 0x07) + offset_y[i5];
						i4 = ((i1 >> 3) & 0x07) + offset_x[i5];

						output_buffer[output_write] = _KEY + i2;
						output_write++;
						output_buffer[output_write] = i4;
						output_write++;
						output_buffer[output_write] = i3;
						output_write++;
					}
				}
			}
			
			
			tilt_thin++;
			
			if(tilt_thin == TTHIN && tilt) {
				addr = 0x53;
			
				d[0] = 0x32;
				twi_writeTo(addr,d,1,1);
				twi_readFrom(addr, input_buffer, 2);
				x = input_buffer[0] | (input_buffer[1] << 8);
								
				tilt_accum[0][3] = tilt_accum[0][2];
				tilt_accum[0][2] = tilt_accum[0][1];
				tilt_accum[0][1] = tilt_accum[0][0];
				tilt_accum[0][0] = x;
				x = (tilt_accum[0][0] + tilt_accum[0][1] + tilt_accum[0][2] + tilt_accum[0][3]) / 4;
			
				d[0] = 0x34;
				twi_writeTo(addr,d,1,1);
				twi_readFrom(addr, input_buffer, 2);
				y = input_buffer[0] | (input_buffer[1] << 8);
				
				tilt_accum[1][3] = tilt_accum[1][2];
				tilt_accum[1][2] = tilt_accum[1][1];
				tilt_accum[1][1] = tilt_accum[1][0];
				tilt_accum[1][0] = x;
				x = (tilt_accum[1][0] + tilt_accum[1][1] + tilt_accum[1][2] + tilt_accum[1][3]) / 4;
				
				d[0] = 0x36;
				twi_writeTo(addr,d,1,1);
				twi_readFrom(addr, input_buffer, 2);
				z = input_buffer[0] | (input_buffer[1] << 8);
				
				tilt_accum[2][3] = tilt_accum[2][2];
				tilt_accum[2][2] = tilt_accum[2][1];
				tilt_accum[2][1] = tilt_accum[2][0];
				tilt_accum[2][0] = x;
				x = (tilt_accum[2][0] + tilt_accum[2][1] + tilt_accum[2][2] + tilt_accum[2][3]) / 4;
			
				output_buffer[output_write] = 0x81;
				output_write++;
				output_buffer[output_write] = 0;
				output_write++;
				output_buffer[output_write] = x & 0xff;
				output_write++;
				output_buffer[output_write] = x >> 8;
				output_write++;
				output_buffer[output_write] = y & 0xff;
				output_write++;
				output_buffer[output_write] = y >> 8;
				output_write++;
				output_buffer[output_write] = z & 0xff;
				output_write++;
				output_buffer[output_write] = z >> 8;
				output_write++;
				
				tilt_thin = 0;
			}
			
		}
					

		// does this need to be protected from ints?
		if(output_read != output_write) {
			PORTD = 0;                      // setup PORTD for output
			DDRD = 0xFF;

			while(output_read != output_write) {

				PORTC |= C3_WR;
				PORTD = output_buffer[output_read];
				PORTC &= ~(C3_WR);

				output_read++;
			}
		}
	}

	return 0;
}