#define F_CPU 16000000UL
#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include "message.h"
#include "button.h"

// protocol
#define KEYDOWN 0
#define KEYUP 1
#define LEDON 2
#define LEDOFF 3
#define LEDROW1 4
#define LEDCOL1 5
#define LEDROW2 6
#define LEDCOL2 7
#define FRAME 8
#define CLEAR 9
#define INTENSITY 10
#define MODE 11

// led pins
#define E0_CLK 0x01
#define E1_LD 0x02   
#define E2_SER4 0x04 
#define E3_SER3 0x08 
#define E4_SER2 0x10 
#define E5_SER1 0x20 

// key in pins
#define E6_CLK 0x40
#define E7_LD 0x80
#define B0_SER4 0x01
#define B1_SER3 0x02
#define B2_SER2 0x04
#define B3_SER1 0x08

// key sel pins
#define B4_A0 0x10
#define B5_A1 0x20
#define B6_A2 0x40

// usb pins
#define C0_TXE 0x01
#define C1_RXF 0x02
#define C2_WR 0x04
#define C3_RD 0x08
#define C4_PWREN 0x10
#define B7_USB 0x80

#define OUTPUT_BUFFER_LENGTH 80
#define KEY_REFRESH_RATE 25
#define RX_STARVE 20

uint8 i1, i2, i3, i4;
volatile uint8 output_buffer[OUTPUT_BUFFER_LENGTH];
volatile uint8 output_write;
volatile uint8 output_read;

// remember to turn off ints if usbsleep!!
ISR(TIMER0_COMP_vect)
{
	for(i1=0;i1<8;i1++) {
		// sel row
		i2 = i1 << 4;				// to prevent momentary flicker to row 0
		PORTB = i2;
		
		button_last[i1] = button_current[i1];
		button_last[i1+8] = button_current[i1+8];
		button_last[i1+16] = button_current[i1+16];
		button_last[i1+24] = button_current[i1+24];
		
		_delay_us(5);				// wait for voltage fall! due to high resistance pullup
		PORTE |= (E7_LD);	
		_delay_us(1);

		for(i2=0;i2<8;i2++) {
			
			// =================================================
			i3 = i1;
			i4 = (PINB & B3_SER1)!=0;
			
			if (!i4) 
                button_current[i3] |= (1 << i2);
            else
                button_current[i3] &= ~(1 << i2);

			buttonCheck(i3, i2);

			if (button_event[i3] & (1 << i2)) {
                button_event[i3] &= ~(1 << i2);	

				output_buffer[output_write] = i4 << 4;
				output_buffer[output_write+1] = ((7-i1)<<4) | i2;
				output_write = (output_write + 2) % OUTPUT_BUFFER_LENGTH;
			}
			
			// =================================================
			i3 = i1 + 8;
			i4 = (PINB & B2_SER2)!=0;
			
			if (!i4) 
                button_current[i3] |= (1 << i2);
            else
                button_current[i3] &= ~(1 << i2);

			buttonCheck(i3, i2);

			if (button_event[i3] & (1 << i2)) {
                button_event[i3] &= ~(1 << i2);	

				output_buffer[output_write] = i4 << 4;
				output_buffer[output_write+1] = ((15-i1)<<4) | i2;
				output_write = (output_write + 2) % OUTPUT_BUFFER_LENGTH;
			}
			
			// =================================================
			i3 = i1 + 16;
			i4 = (PINB & B1_SER3)!=0;
			
			if (!i4) 
                button_current[i3] |= (1 << i2);
            else
                button_current[i3] &= ~(1 << i2);

			buttonCheck(i3, i2);

			if (button_event[i3] & (1 << i2)) {
                button_event[i3] &= ~(1 << i2);	

				output_buffer[output_write] = i4 << 4;
				output_buffer[output_write+1] = ((7-i1)<<4) | (i2+8);
				output_write = (output_write + 2) % OUTPUT_BUFFER_LENGTH;
			}
			
			// =================================================
			i3 = i1 + 24;
			i4 = (PINB & B0_SER4)!=0;
			
			if (!i4) 
                button_current[i3] |= (1 << i2);
            else
                button_current[i3] &= ~(1 << i2);

			buttonCheck(i3, i2);

			if (button_event[i3] & (1 << i2)) {
                button_event[i3] &= ~(1 << i2);	

				output_buffer[output_write] = i4 << 4;
				output_buffer[output_write+1] = ((15-i1)<<4) | (i2+8);
				output_write = (output_write + 2) % OUTPUT_BUFFER_LENGTH;
			}

			PORTE |= (E6_CLK);
			_delay_us(1);
			PORTE &= ~(E6_CLK);		
			_delay_us(1);
		}

		PORTE &= ~(E7_LD);	
	}

	TCNT0 = 0;
}


void to_all_led(char data1, char data2)
{
	uint8 i;

	PORTE &= ~(E1_LD);

	for(i=0;i<8;i++) {
		if(data1 & (1<<(7-i))) {
			PORTE |= (E2_SER4);
			PORTE |= (E3_SER3);
			PORTE |= (E4_SER2);
			PORTE |= (E5_SER1);
		}
		else {
			PORTE &= ~(E2_SER4);
			PORTE &= ~(E3_SER3);
			PORTE &= ~(E4_SER2);
			PORTE &= ~(E5_SER1);
		}

		PORTE |= (E0_CLK);
		_delay_us(1);
		PORTE &= ~(E0_CLK);
	}

	for(i=0;i<8;i++) {
		if(data2 & (1<<(7-i))) {
			PORTE |= (E2_SER4);
			PORTE |= (E3_SER3);
			PORTE |= (E4_SER2);
			PORTE |= (E5_SER1);
		}
		else {
			PORTE &= ~(E2_SER4);
			PORTE &= ~(E3_SER3);
			PORTE &= ~(E4_SER2);
			PORTE &= ~(E5_SER1);
		}

		PORTE |= (E0_CLK);
		_delay_us(1);
		PORTE &= ~(E0_CLK);
	}

	PORTE |= (E1_LD); 
}

void to_led(char data_all, char data1, char data2, char data3, char data4)
{
	uint8 i;

	PORTE &= ~(E1_LD);

	for(i=0;i<8;i++) {
		if(data_all & (1<<(7-i))) {
			PORTE |= (E2_SER4);
			PORTE |= (E3_SER3);
			PORTE |= (E4_SER2);
			PORTE |= (E5_SER1);
		}
		else {
			PORTE &= ~(E2_SER4);
			PORTE &= ~(E3_SER3);
			PORTE &= ~(E4_SER2);
			PORTE &= ~(E5_SER1);
		}

		PORTE |= (E0_CLK);
		_delay_us(1);
		PORTE &= ~(E0_CLK);
	}

	for(i=0;i<8;i++) {
		if(data1 & (1<<(7-i))) PORTE |= (E5_SER1);
		else PORTE &= ~(E5_SER1);
		
		if(data2 & (1<<(7-i))) PORTE |= (E4_SER2);
		else PORTE &= ~(E4_SER2);
		
		if(data3 & (1<<(7-i))) PORTE |= (E3_SER3);
		else PORTE &= ~(E3_SER3);
		
		if(data4 & (1<<(7-i))) PORTE |= (E2_SER4);
		else PORTE &= ~(E2_SER4);				

		PORTE |= (E0_CLK);
		_delay_us(1);
		PORTE &= ~(E0_CLK);
	}

	PORTE |= (E1_LD); 
}

int main(void)
{
	uint8 i1,i2,i3,i4;
	uint8 starve;
	uint8 rx_count;
	uint8 rx_length;
	uint8 rx_type;
	uint8 rx_timeout;
	uint8 rx[10];	// input buffer
	uint8 usb_state, sleep_state;
	
	uint8 display[4][8];
	
	uint8 packet_length[12];
	
	// setup packet length
	packet_length[KEYDOWN] = 2; 
	packet_length[KEYUP] = 2;
	packet_length[LEDON] = 2;
	packet_length[LEDOFF] = 2;
	packet_length[LEDROW1] = 2;
	packet_length[LEDCOL1] = 2;
	packet_length[LEDROW2] = 3;
	packet_length[LEDCOL2] = 3;
	packet_length[FRAME] = 9;
	packet_length[CLEAR] = 1;
	packet_length[INTENSITY] = 1;
	packet_length[MODE] = 1;
	

	// pin assignments
	DDRE = 0xff;	// all output
	DDRB = B4_A0 | B5_A1 | B6_A2; 

	PORTE = 0;
	PORTB = 0; 

	DDRC = C2_WR | C3_RD;
	
	PORTD = 0;                                			// setup PORTD for input
	DDRD = 0;                                 			// input w/ tristate
	


	// init led drivers
	to_all_led(11, 7);                                    	// set scan limit to full range
	to_all_led(10, 15);                               		// set to max intensity
	for(i1 = 1; i1 < 9; i1++) to_all_led(i1, 0);         	// clear pattern 
	
	to_all_led(12, 1);                                    	// come out of shutdown mode 
	to_all_led(15, 0);                                    	// test mode off


	to_led(1,6,0,0,0);
	for(i1=0;i1<8;i1++) _delay_ms(8);
	to_all_led(10,7);
	to_led(1,9,0,0,0);
	to_led(2,6,0,0,0);
	for(i1=0;i1<8;i1++) _delay_ms(8);
	to_all_led(10,3);
	to_led(1,16,0,0,0);
	to_led(2,9,0,0,0);
	to_led(3,6,0,0,0);
	for(i1=0;i1<8;i1++) _delay_ms(8);
	to_all_led(10,0);
	to_led(1,16,0,0,0);
	to_led(2,16,0,0,0);
	to_led(3,8,0,0,0);
	to_led(4,7,0,0,0);
	for(i1=0;i1<8;i1++) _delay_ms(8);

	for(i1 = 1; i1 < 9; i1++) to_all_led(i1, 0);			// clear pattern
	to_all_led(10, 15);                            			// set to max intensity

	for(i1=0;i1<255;i1++) _delay_ms(8);

	// init data
	for(i1=0;i1<10;i1++) rx[i1] = 0;
			
	for(i1=0;i1<4;i1++)
		for(i2=0;i2<8;i2++)
			display[i1][i2] = 0;

	i1 = i2 = i3 = i4 = 0;
	rx_count = rx_type = rx_timeout = 0;
	rx_length = 1;
	usb_state = 0;
	sleep_state = 1;

	buttonInit();
	output_read = 0;
	output_write = 0;
	
	TCCR0A |= (1<<CS02) | (1<<CS00); // timer0 on, prescale clk/1024 (p95)
	TIMSK0 |= (1 << OCIE0A);// | (1<< TOIE0);  // enable timer0 interrupts
	OCR0A = KEY_REFRESH_RATE;
	sei();
	

	while(true) {
		
		if(sleep_state) {	 			
			if(!(PINC & C4_PWREN)) {
				sleep_state = 0;				

				to_all_led(12, 1);		// out of shutdown
				for(i1=0;i1<64;i1++) {	// fade in
					to_all_led(10, (i1)/4);
					_delay_ms(8);
				}
			} 

			for(i1=0;i1<64;i1++) _delay_ms(8);
			
			TIMSK0 |= (1 << OCIE0A);// | (1<< TOIE0);  // enable timer0 interrupts
			
		}
		
		// ========================== NORMAL:
		else {
			
			// ====================== check/read incoming serial	
			PORTD = 0;                                // setup PORTD for input
			DDRD = 0;                                 // input w/ tristate

			if(rx_timeout > 40 ) {
				rx_count = 0;
			}
			else rx_timeout++;

			starve = 0;
			
			while((PINC & C1_RXF) == 0 && starve < RX_STARVE) {
				starve++;				// make sure we process keypad data...
				
				PORTC &= ~(C3_RD);
				_delay_us(1);			// wait for valid data
				rx[rx_count] = PIND;
				
				if(rx_count == 0) {		// get packet length if reading first byte
					rx_type = rx[0] >> 4;
					if(rx_type < 12) {	
						rx_length = packet_length[rx_type];
						rx_count++;
						rx_timeout = 0;
					}
				}
				else rx_count++;

				if(rx_count == rx_length) {
					rx_count = 0;
					
					if(rx_type == INTENSITY) {
						i1 = rx[0] & 0x0f;
						to_all_led(10,i1);
					}
					else if(rx_type == MODE) {
						i1 = rx[0] & 0x03;
						
						if(i1 == 0) {							// NORMAL
							to_all_led(12, 1);					// shutdown mode off
							to_all_led(15, 0);					// test mode off
						}
						else if(i1 == 1) {						
							to_all_led(12, 1);					// shutdown mode off
							to_all_led(15, 1);					// test mode on
						}
						else if(i1 == 2) {
							to_all_led(12, 0);					// shutdown mode on
							to_all_led(15, 0);					// test mode off
						}
					}
					else if(rx_type == LEDCOL1) {			
						i1 = rx[0] & 0x0F;
						i2 = i1 > 7;
						i1 = i1 & 0x07;
					
						display[i2][i1] = rx[1];
						
						to_led(i1+1,display[0][i1],display[1][i1],display[2][i1],display[3][i1]);
					}
					else if(rx_type == LEDCOL2) {			
						i1 = rx[0] & 0x0F;
						i2 = i1 > 7;
						i1 = i1 & 0x07;
					
						display[i2][i1] = rx[1];
						display[i2+2][i1] = rx[2];
					
						to_led(i1+1,display[0][i1],display[1][i1],display[2][i1],display[3][i1]);
					}
					else if(rx_type == LEDROW1) {			
						i1 = 1 << (rx[0] & 0x07);
						i2 = (rx[0] & 0x08) >> 2;
						
						for(i3=0;i3<8;i3++) {
							i4 = 1 << i3;
							// OPTIMIZE??
							if(rx[1] & i4) display[i2][i3] |= i1;
							else display[i2][i3] &= ~i1;												
						}
						
						for(i3=0;i3<8;i3++)
							to_led(i3+1,display[0][i3],display[1][i3],display[2][i3],display[3][i3]);
					}
					else if(rx_type == LEDROW2) {			
						i1 = 1 << (rx[0] & 0x07);
						i2 = (rx[0] & 0x08) >> 2;
						
						for(i3=0;i3<8;i3++) {
							i4 = 1 << i3;
							// OPTIMIZE??
							if(rx[1] & i4) display[i2][i3] |= i1;
							else display[i2][i3] &= ~i1;
							
							if(rx[2] & i4) display[i2+1][i3] |= i1;
							else display[i2+1][i3] &= ~i1;							
						}
						
						for(i3=0;i3<8;i3++)
							to_led(i3+1,display[0][i3],display[1][i3],display[2][i3],display[3][i3]);
					}
					else if(rx_type == FRAME) {
						i1 = rx[0] & 0x03;
						
						for(i2=0;i2<8;i2++) {
							i4 = 1 << i2;
							for(i3=0;i3<8;i3++) {
								if(rx[i2+1] & (1 << i3)) display[i1][i3] |= i4;
								else display[i1][i3] &= ~i4;												
							}
						}
						
						for(i3=0;i3<8;i3++)
							to_led(i3+1,display[0][i3],display[1][i3],display[2][i3],display[3][i3]);
					}
					else if(rx_type == LEDON) {
						i1 = rx[1] & 0x07;
						i2 = (rx[1] >> 4) & 0x07;
						i3 = ((rx[1] & 0x80) >> 7) + ((rx[1] & 0x08) >> 2);
						
						display[i3][i2] |= (1<<i1);
						
						to_led(i2+1,display[0][i2],display[1][i2],display[2][i2],display[3][i2]);					
					}
					else if(rx_type == LEDOFF) {
						i1 = rx[1] & 0x07;
						i2 = (rx[1] >> 4) & 0x07;
						i3 = ((rx[1] & 0x80) >> 7) + ((rx[1] & 0x08) >> 2);
						
						display[i3][i2] &= ~(1<<i1);
						
						to_led(i2+1,display[0][i2],display[1][i2],display[2][i2],display[3][i2]);					
					}
					else if(rx_type == CLEAR) {
						i1 = rx[0] & 0x01;
						if(i1) i1 = 255;
						
						for(i2=0;i2<8;i2++) {
							display[0][i2] = i1;
							display[1][i2] = i1;
							display[2][i2] = i1;
							display[3][i2] = i1;
						}
				
						for(i3=0;i3<8;i3++)
							to_led(i3+1,display[0][i3],display[1][i3],display[2][i3],display[3][i3]);					
					}
				}

				PORTC |= C3_RD;
				_delay_us(1);			// wait for valid data
			}

			// ====================== check/send keypad data
			
			PORTD = 0;                      // setup PORTD for output
			DDRD = 0xFF;

			while(output_read != output_write) {
				
				PORTC |= C2_WR;
				PORTD = output_buffer[output_read];
				//_delay_us(1);
				PORTC &= ~(C2_WR);

				PORTC |= C2_WR;
				PORTD = output_buffer[output_read+1];
				//_delay_us(1);
				PORTC &= ~(C2_WR);
				
				output_read = (output_read + 2) % OUTPUT_BUFFER_LENGTH;
			}
			
			// ====================== check usb/sleep status
			if(PINC & C4_PWREN) {
				sleep_state = 1;
				TIMSK0 = 0; // turn off keypad checking int

				for(i1=0;i1<16;i1++) {	// fade out
					to_all_led(10, 15-i1);
					_delay_ms(8);
					_delay_ms(8);
					_delay_ms(8);
					_delay_ms(8);
				}
				to_all_led(12, 0);		// shutdown
			} 
		} 		
	}

	return 0;
}
