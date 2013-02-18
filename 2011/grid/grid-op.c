// grid op

// TODO
// led intensity (normal, scaling?)
// intensity map exponential
// sys commands


#define F_CPU 20000000UL

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include "twi.h"
#include "button.h"

// i2c addr, can be overwritten by makefile
#ifndef ADDR
#define ADDR 100
#endif

// pins

#define C0_KROW_A 0x01
#define C1_KROW_B 0x02
#define C2_KROW_C 0x04

#define B0_KCOL_Q 0x01
#define B1_KCOL_CLK 0x02
#define B2_KCOL_SH 0x04

#define B3_LROW_SER 0x08
#define B4_LROW_SCK 0x10
#define B5_LROW_RCK 0x20


// tuning
#define LED_REFRESH_RATE 80
#define KEYPAD_REFRESH_RATE 10
#define INPUT_BUFFER_LENGTH 256


// i2c output state
#define _REPLY_QUERY 0
#define _REPLY_KEYPAD 1



// i2c protocol
// input
#define _SLEEP 1
#define _WAKE 2
#define _QUERY 0

#define _LED_SET0 0x10
#define _LED_SET1 0x11
#define _LED_ALL0 0x12
#define _LED_ALL1 0x13
#define _LED_MAP 0x14
#define _LED_ROW 0x15
#define _LED_COL 0x16
#define _LED_INT 0x17
#define _LED_SETI 0x18
#define _LED_ALLI 0x19
#define _LED_MAPI 0x1A
#define _LED_ROWI 0x1B
#define _LED_COLI 0x1C

#define _KEYPAD_REQUEST 0x20


const uint8_t i2c_packet_length[32] = {
	1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
	2,2,1,1,9,3,3,2, 3,2,65,10,10,0,0,0
};

const uint8_t ints[16] = { 4, 5, 6, 7, 8, 9, 11, 13, 16, 20, 26, 33, 43, 54, 66, 81 };
const uint8_t d1ints[16] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 20, 24 };
const uint8_t d2ints[16] = { 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8 };


// globals
uint8_t n1, n2, n3, n4;
static volatile uint8_t rx[INPUT_BUFFER_LENGTH];
static volatile uint8_t rx_read, rx_write;
volatile uint8_t reply_type;

volatile uint8_t refresh_row;
volatile uint8_t refresh_cycle;
volatile uint8_t intensity_index;

volatile uint8_t scan_keypad;

static uint8_t key_changes[128], key_send[1];
volatile uint8_t key_read, key_write;

static uint8_t lights[8];// = {127,2,3,4,5,6,7,8};
static uint8_t dim1[8];// = {15,255,255,255,255,255,0,0};
static uint8_t dim2[8];// = {3,0,0,255,255,255,255,255};



// trigger a keypad scan
ISR(TIMER1_COMPA_vect)
{
	scan_keypad++;
	TCNT1 = 0;
}

// cycle led row refresh
ISR(TIMER0_COMPA_vect)
{
	if(refresh_row > 7) {
		refresh_row = 0;
		PORTB |= B3_LROW_SER;
		PORTB |= B4_LROW_SCK;
		PORTB &= ~B4_LROW_SCK;
		OCR0B = ints[intensity_index];
		OCR2A = d1ints[intensity_index];
		OCR2B = d2ints[intensity_index];
	}
	else {
		PORTB &= ~B3_LROW_SER;
		PORTB |= B4_LROW_SCK;
		PORTB &= ~B4_LROW_SCK;
	}

	PORTD = 0;
	PORTB |= B5_LROW_RCK;
	PORTB &= ~B5_LROW_RCK;
	PORTD = lights[7-refresh_row];

	refresh_row++;
	TCNT0 = 0;
	TCNT2 = 0;
}

// master intensity
ISR(TIMER0_COMPB_vect)
{
	PORTD = 0;
}

// turn off dimmed leds
ISR(TIMER2_COMPB_vect)
{
	PORTD = lights[8-refresh_row] & ~dim2[8-refresh_row];
}

ISR(TIMER2_COMPA_vect)
{
	PORTD = lights[8-refresh_row] & ~dim1[8-refresh_row];
}



// BUS RECEIVE
// ===============================================================
// ===============================================================
void processRx(uint8_t *data, int index)
{
	for(n1=0; n1 < index; n1++) {
		rx[(n1 + rx_write)%128] = data[n1];
	}
	rx_write += index;
	rx_write %= 128;
}

// BUS TRANSMIT
// ===============================================================
// ===============================================================
void processTx(void)
{
	key_send[0] = 255;
	if(key_read != key_write) {
		key_send[0] = key_changes[key_read];
		key_read++;
		key_read %= 128;
	} 

	twi_transmit(key_send, 1);
}


// main
// ===============================================================
// ===============================================================
// ===============================================================
// ===============================================================
int main(void)
{
	uint8_t i1, i2, i3, i4;
	
	uint8_t bus_address;
	uint8_t rx_type;
	uint8_t k_row;

	k_row = 0;
	refresh_row = 0;
	scan_keypad = 0;
	key_read = key_write = 0;
	rx_read = rx_write = 0;
	
	intensity_index = 15;

	reply_type = _REPLY_KEYPAD;

	DDRB = 0xfe;	// input pin 1, outputs else
	DDRC = 0xff;	// output
	DDRD = 0xff;	// output

	bus_address = ADDR;	// initialized value, default 100 or from makefile
						// we will overwrite this with the value stored in eeprom

	// led refresh interrupt
	TCCR0A = 0;
	TCCR0B = (1<<CS02);// | (1<<CS00);	// prescale io/256
	TIMSK0 = (1 << OCIE0A) | (1 << OCIE0B);// | (1 << TOIE0);
	OCR0A = LED_REFRESH_RATE;
	OCR0B = ints[intensity_index];
	
	TCCR2A = 0;
	TCCR2B = (1<<CS22) | (1<<CS21);		// prescale io/256
	TIMSK2 = (1 << OCIE2A) | (1 << OCIE2B);// | (1 << OCIE0B);// | (1 << TOIE0);
	OCR2A = d1ints[intensity_index];
	OCR2B = d2ints[intensity_index];
	
	// keypad interrupt
	TCCR1A = 0;
	TCCR1B = (1<<CS12) | (1<<CS10);
	TIMSK1 = (1<<OCIE1A);
	OCR1A = KEYPAD_REFRESH_RATE;

	buttonInit();

	sei();

	twi_init();
	twi_setAddress(bus_address);
	twi_attachSlaveRxEvent(processRx);
	twi_attachSlaveTxEvent(processTx);


	// loop
	// ===============================================================
	// ===============================================================
	while(1) {
		if(scan_keypad) {
			//for(k_row=0;k_row<8;k_row++) {
				button_last[k_row] = button_current[k_row];
				//_delay_us(5);
				PORTB |= B2_KCOL_SH;
				_delay_us(5);

				for(i1=0;i1<8;i1++) {
					i2 = !(PINB & B0_KCOL_Q);
					PORTB |= B1_KCOL_CLK;
					//_delay_us(2);
					PORTB &= ~B1_KCOL_CLK;
					if (i2)
			            button_current[k_row] |= (1 << i1);
			        else
			            button_current[k_row] &= ~(1 << i1);
					buttonCheck(k_row,i1);
					if(button_event[k_row] & (1<<i1)) {
						button_event[k_row] &= ~(1<<i1);
						key_changes[key_write]= ((!i2)<<7) + ((7-i1)*8) + (k_row);
						key_write++;
						key_write %= 128;
					}
				}

			PORTB &= ~B2_KCOL_SH;
			
			k_row++;
			k_row %= 8;
			PORTC = k_row;//(PINC | 0x07) | k_row;

			scan_keypad = 0;
		}

		if(rx_read != rx_write) {
			rx_type = rx[rx_read];

			if(rx_type == _QUERY) {
				i1 = 0;
			} else if(rx_type == _SLEEP) {
				TIMSK0 = 0;
				TIMSK1 = 0;
				TIMSK2 = 0;
				PORTD = 0;
			} else if(rx_type == _WAKE) {
				TIMSK0 = (1 << OCIE0A) | (1 << OCIE0B);
				TIMSK1 = (1<<OCIE1A);
			} else if(rx_type == _KEYPAD_REQUEST) {
				reply_type = _REPLY_KEYPAD;
			} else if(rx_type == _LED_SET0) {
				// _LED_SET0 //////////////////////////////////////////////
				i3 = rx[(rx_read + 1)%128];
				i1 = i3 & 0x0f;
				i2 = i3 >> 4;
				lights[i1] &= ~(1<<i2);
			} else if(rx_type == _LED_SET1) {
				// _LED_SET1 //////////////////////////////////////////////
				i3 = rx[(rx_read + 1)%128];
				i1 = i3 & 0x0f;
				i2 = i3 >> 4;
				lights[i1] |= (1<<i2);
			} else if(rx_type == _LED_ALL0) {
				// _LED_ALL0 //////////////////////////////////////////////
				for(i1=0;i1<8;i1++) lights[i1] = 0;
			} else if(rx_type == _LED_ALL1) {
				// _LED_ALL1 //////////////////////////////////////////////
				for(i1=0;i1<8;i1++) lights[i1] = 255;
			} else if(rx_type == _LED_MAP) {
				// _LED_MAP ///////////////////////////////////////////////
				for(i1=0;i1<8;i1++) {
					lights[i1] = rx[(1 + rx_read + i1)%128];
				}
			} else if(rx_type == _LED_ROW) {
				// _LED_ROW ///////////////////////////////////////////////
				lights[rx[(rx_read + 1)%128]] = rx[(rx_read + 2)%128];
			} else if(rx_type == _LED_COL) {
				// _LED_COL ///////////////////////////////////////////////
				i1 = rx[(rx_read + 1)%128];
				i2 = rx[(rx_read + 2)%128];
				for(i3=0;i3<8;i3++) {
					if(i2 & (1<<i3)) lights[i3] |= (1<<i1);
					else lights[i3] &= ~(1<<i1);
				}
			} else if(rx_type == _LED_INT) {
				// _LED_INT ///////////////////////////////////////////////
				i1 = (rx_read + 1) % 128;
				intensity_index = rx[i1];
			 
			} else if(rx_type == _LED_SETI) {
				// _LED_SETI //////////////////////////////////////////////
				i3 = rx[(rx_read + 1)%128];
				i1 = i3 & 0x0f;
				i2 = (1 << (i3 >> 4));
				i3 = rx[(rx_read + 2)%128];
				
				if(i3 == 0) {
					lights[i1] &= ~i2;
					dim1[i1] &= ~i2;
					dim2[i1] &= ~i2;
				} else if(i3 == 1) {
					lights[i1] |= i2;
					dim1[i1] |= i2;
					dim2[i1] |= i2;
				} else if(i3 == 2) {
					lights[i1] |= i2;
					dim1[i1] |= i2;
					dim2[i1] &= ~i2;
				} else if(i3 == 3) {
					lights[i1] |= i2;
					dim1[i1] &= ~i2;
					dim2[i1] &= ~i2;
				}
			} else if(rx_type == _LED_ALLI) {
				// _LED_ALLI //////////////////////////////////////////////
				i1 = rx[(rx_read + 1)%128];
				
				if(i1 == 0) {
					for(i2=0;i2<8;i2++) {
						lights[i2] = 0;
						dim1[i2] = 0;
						dim2[i2] = 0;
					}
				} else if(i1 == 1) {
					for(i2=0;i2<8;i2++) {
						lights[i2] = 255;
						dim1[i2] = 255;
						dim2[i2] = 255;
					}
				} else if(i1 == 2) {
					for(i2=0;i2<8;i2++) {
						lights[i2] = 255;
						dim1[i2] = 255;
						dim2[i2] = 0;
					}
				} else if(i1 == 3) {
					for(i2=0;i2<8;i2++) {
						lights[i2] = 255;
						dim1[i2] = 0;
						dim2[i2] = 0;
					}
				}
			} else if(rx_type == _LED_MAPI) {
				// _LED_MAPI //////////////////////////////////////////////
				for(i1=0;i1<8;i1++) {
					for(i2=0;i2<8;i2++) {
						i4 = 1 << i2;
						i3 = rx[(rx_read + (i1*8 + i2) + 1)%128];
						
						if(i3 == 0) {
							lights[i1] &= ~i4;
							dim1[i1] &= ~i4;
							dim2[i1] &= ~i4;
						} else if(i3 == 1) {
							lights[i1] |= i4;
							dim1[i1] |= i4;
							dim2[i1] |= i4;
						} else if(i3 == 2) {
							lights[i1] |= i4;
							dim1[i1] |= i4;
							dim2[i1] &= ~i4;
						} else if(i3 == 3) {
							lights[i1] |= i4;
							dim1[i1] &= ~i4;
							dim2[i1] &= ~i4;
						}
					}
				}
			} else if(rx_type == _LED_ROWI) {
				// _LED_ROWI //////////////////////////////////////////////
				i1 = rx[(rx_read + 1)%128];
				
				for(i2=0;i2<8;i2++) {
					i4 = 1 << i2;
					i3 = rx[(rx_read + i2 + 2)%128];
					
					if(i3 == 0) {
						lights[i1] &= ~i4;
						dim1[i1] &= ~i4;
						dim2[i1] &= ~i4;
					} else if(i3 == 1) {
						lights[i1] |= i4;
						dim1[i1] |= i4;
						dim2[i1] |= i4;
					} else if(i3 == 2) {
						lights[i1] |= i4;
						dim1[i1] |= i4;
						dim2[i1] &= ~i4;
					} else if(i3 == 3) {
						lights[i1] |= i4;
						dim1[i1] &= ~i4;
						dim2[i1] &= ~i4;
					}
				}
					
			} else if(rx_type == _LED_COLI) {
				// _LED_COLI //////////////////////////////////////////////
				i1 = rx[(rx_read + 1) % 128];
				i4 = 1 << i1;
				
				for(i2=0;i2<8;i2++) {
					i3 = rx[(rx_read + i2 + 2)%128];
					
					if(i3 == 0) {
						lights[i2] &= ~i4;
						dim1[i2] &= ~i4;
						dim2[i2] &= ~i4;
					} else if(i3 == 1) {
						lights[i2] |= i4;
						dim1[i2] |= i4;
						dim2[i2] |= i4;
					} else if(i3 == 2) {
						lights[i2] |= i4;
						dim1[i2] |= i4;
						dim2[i2] &= ~i4;
					} else if(i3 == 3) {
						lights[i2] |= i4;
						dim1[i2] &= ~i4;
						dim2[i2] &= ~i4;
					}
				}
				
			
			} else {
				rx_read = rx_write;		// kill the buffer if weird data arrived, reset
			}

			if(rx_read != rx_write) {
				rx_read += i2c_packet_length[rx_type];
				rx_read %= 128;
			}
		}
	}

	return 0;
}