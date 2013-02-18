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

static const unsigned char rbit[256] = 
{
  0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 
  0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 
  0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 
  0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC, 
  0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 
  0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
  0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, 
  0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
  0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
  0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9, 
  0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
  0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
  0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 
  0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
  0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7, 
  0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

void to_all_led(char data1, char data2)
{
	uint8 i;

	PORTE &= ~(E1_LD);

	for(i=0;i<8;i++) {
		if(data1 & (1<<(7-i))) {
			PORTE |= (E4_SER2);
			PORTE |= (E5_SER1);
		}
		else {
			PORTE &= ~(E4_SER2);
			PORTE &= ~(E5_SER1);
		}

		PORTE |= (E0_CLK);
		_delay_us(1);
		PORTE &= ~(E0_CLK);
	}

	for(i=0;i<8;i++) {
		if(data2 & (1<<(7-i))) {
			PORTE |= (E4_SER2);
			PORTE |= (E5_SER1);
		}
		else {
			PORTE &= ~(E4_SER2);
			PORTE &= ~(E5_SER1);
		}

		PORTE |= (E0_CLK);
		_delay_us(1);
		PORTE &= ~(E0_CLK);
	}

	PORTE |= (E1_LD); 
}

void to_led(char data_all, char data1, char data2)
{
	uint8 i;

	PORTE &= ~(E1_LD);

	for(i=0;i<8;i++) {
		if(data_all & (1<<(7-i))) {
			PORTE |= (E4_SER2);
			PORTE |= (E5_SER1);
		}
		else {
			PORTE &= ~(E4_SER2);
			PORTE &= ~(E5_SER1);
		}

		PORTE |= (E0_CLK);
		_delay_us(1);
		PORTE &= ~(E0_CLK);
	}

	// compared to 256.c -- data1 and data2 are reversed for proper hardware cabling
	for(i=0;i<8;i++) {
		if(data2 & (1<<(7-i))) PORTE |= (E5_SER1);
		else PORTE &= ~(E5_SER1);
		
		if(data1 & (1<<(7-i))) PORTE |= (E4_SER2);
		else PORTE &= ~(E4_SER2);

		PORTE |= (E0_CLK);
		_delay_us(1);
		PORTE &= ~(E0_CLK);
	}

	PORTE |= (E1_LD); 
}

int main(void)
{
	uint8 i1,i2,i3,i4;
	uint8 rx_count;
	uint8 rx_length;
	uint8 rx_type;
	uint8 rx_timeout;
	uint8 rx[10];	// input buffer
	uint8 usb_state, sleep_state;
	
	uint8 display[4][8];
	
	uint8 packet_length[12];
	
	// sleep variables
	uint8 pattern[2][9];
	uint8 world[8][16][2];
	uint8 neighbors, population, grow, x, y;
	
	grow = 0;
	population = 0;

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


	to_led(2,0,128);
	to_led(3,0,128);
	for(i1=0;i1<8;i1++) _delay_ms(8);
	to_all_led(10,7);
	to_led(1,0,128);
	to_led(2,0,64);
	to_led(3,0,64);
	to_led(4,0,128);
	for(i1=0;i1<8;i1++) _delay_ms(8);
	to_all_led(10,3);
	to_led(1,0,64);
	to_led(2,0,32);
	to_led(3,0,32);
	to_led(4,0,64);
	to_led(5,0,128);
	for(i1=0;i1<8;i1++) _delay_ms(8);
	to_all_led(10,0);
	to_led(1,0,32);
	to_led(2,0,16);
	to_led(3,0,16);
	to_led(4,0,32);
	to_led(5,0,192);
	for(i1=0;i1<8;i1++) _delay_ms(8);

	for(i1 = 1; i1 < 9; i1++) to_all_led(i1, 0);			// clear pattern
	to_all_led(10, 15);                            			// set to max intensity

	for(i1=0;i1<255;i1++) _delay_ms(8);

	// init data
	for(i1=0;i1<10;i1++) rx[i1] = 0;
			
	for(i1=0;i1<4;i1++)
		for(i2=0;i2<8;i2++)
			display[i1][i2] = 0;
			
	for(i1=0;i1<8;i1++)
		for(i2=0;i2<16;i2++) {
			world[i1][i2][0] = 0;
			world[i1][i2][1] = 0;
		}
			
	world[2][12][1] = 1;

	i1 = i2 = i3 = i4 = 0;
	rx_count = rx_type = rx_timeout = 0;
	rx_length = 1;
	usb_state = 0;
	sleep_state = 1;

	buttonInit();
	

	while(true) {
		
		// ========================== NO USB:
		if(!usb_state) {
			/******* DO SOMETHING BETTER HERE ********/

			for(x = 0; x < 8; x++) {
			  	for(y = 0; y < 16; y++) { 
					if (world[x][y][1] == 1 ) {
			    	   	world[x][y][0] = 1;
			
						i1 = x & 0x07;
						i2 = y & 0x07;
						i3 = !((y & 0x08) >> 3);
						
						display[i3][i2] |= (1<<i1);
			        } 
			      	if (world[x][y][1] == 255) {
			  			world[x][y][0] = 0;
			
			  			//i1 = rx[1] & 0x07;
						//i2 = (rx[1] >> 4) & 0x07;
						//i3 = ((rx[1] & 0x80) >> 7) + ((rx[1] & 0x08) >> 2);
						i1 = x & 0x07;
						i2 = y & 0x07;
						i3 = !((y & 0x08) >> 3);
						
						display[i3][i2] &= ~(1<<i1);     	
					}

					world[x][y][1] = 0; 
				}
			}	

			// Birth and death cycle

			if(population < 4 && grow==0) {
				pattern[0][0] = 0;
				pattern[0][1] = 3;
				pattern[0][2] = 2;
				pattern[0][3] = 2;
				pattern[0][4] = 1;
				pattern[0][5] = 1;
				pattern[0][6] = 1;
				pattern[0][7] = 1;
				pattern[0][8] = 1;

				pattern[1][0] = 0;
				pattern[1][1] = 4;
				pattern[1][2] = 2;
				pattern[1][3] = 1;
				pattern[1][4] = 1;
				pattern[1][5] = 0;
				pattern[1][6] = 0;
				pattern[1][7] = 0;
				pattern[1][8] = 0;

				grow = 1;
			}
			else if(population > 20 && grow==1) {
				pattern[0][0] = 0;
				pattern[0][1] = 1;
				pattern[0][2] = 1;
				pattern[0][3] = 1;
				pattern[0][4] = 1;
				pattern[0][5] = 1;
				pattern[0][6] = 0;
				pattern[0][7] = 0;
				pattern[0][8] = 0;

				pattern[1][0] = 64;
				pattern[1][1] = 8;
				pattern[1][2] = 6;
				pattern[1][3] = 4;
				pattern[1][4] = 2;
				pattern[1][5] = 1;
				pattern[1][6] = 1;
				pattern[1][7] = 0;
				pattern[1][8] = 0;

				grow=0;
			}

			population = 0;
			for (x = 0; x < 8; x++) { 
		    	for (y = 0; y < 16; y++) {
					neighbors =  world[(x + 1) % 8][y][0] + 
					         world[x][(y + 1) % 8][0] + 
					         world[(x + 8 - 1) % 8][y][0] + 
					         world[x][(y + 16 - 1) % 16][0] + 
					         world[(x + 1) % 8][(y + 1) % 16][0] + 
					         world[(x + 8 - 1) % 8][(y + 1) % 16][0] + 
					         world[(x + 8 - 1) % 8][(y + 16 - 1) % 16][0] + 
					         world[(x + 1) % 8][(y + 16 - 1) % 16][0]; 

					if(world[x][y][0]==1)
						population++;

					if(world[x][y][0]==0 && (rand()%255) < pattern[0][neighbors])
							world[x][y][1] = 1;

					if(world[x][y][0]==1 && (rand()%255) < pattern[1][neighbors]) 
							world[x][y][1] = 255;  
		    	} 
		  	} 


			// refresh full frame
			for(i3=0;i3<8;i3++)
				to_led(i3+1,display[0][i3],display[1][i3]);


			/*****************************************/
			
			if(PINB & B7_USB) {
				usb_state = 1;
				for(i1=0;i1<16;i1++) {	// fade out
					to_all_led(10, 15-i1);
					_delay_ms(8);
					_delay_ms(8);
					_delay_ms(8);
					_delay_ms(8);
				}
				
				for(i1 = 1; i1 < 9; i1++) to_all_led(i1, 0);				// clear pattern
				to_all_led(10,15);											// full brightness

				for(i1=0;i1<4;i1++)			// erase display buffer
					for(i2=0;i2<8;i2++)
						display[i1][i2] = 0;
				sleep_state = 1;
			}

			for(i1=0;i1<3;i1++) _delay_ms(8);
		}
		
		// ========================== ASLEEP:
		else if(sleep_state) {	 			
			// check if it's time to wake up, or if usb disconnected
			if(!(PINB & B7_USB)) {
				usb_state = 0; 
				for(i1 = 1; i1 < 9; i1++) to_all_led(i1, 0);				// clear pattern
				to_all_led(10,15);											// full brightness			
			} 
			else if(!(PINC & C4_PWREN)) {
				sleep_state = 0;

				to_all_led(12, 1);		// out of shutdown
				for(i1=0;i1<64;i1++) {	// fade in
					to_all_led(10, (i1)/4);
					_delay_ms(8);
				}
			} 

			for(i1=0;i1<64;i1++) _delay_ms(8);
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

			while((PINC & C1_RXF) == 0) {
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
						i1 = 7 - (i1 & 0x07);
					
						display[i2][i1] = rbit[rx[1]];
						
						to_led(i1+1,display[0][i1],display[1][i1]);
					}
					else if(rx_type == LEDCOL2) {			
						i1 = rx[0] & 0x0F;
						i2 = i1 > 7;
						i1 = 7 - (i1 & 0x07);
					
						display[i2][i1] = rbit[rx[1]];
						//display[i2+2][i1] = rbit[rx[2]];			// does not exist on 8x16
					
						to_led(i1+1,display[0][i1],display[1][i1]);
					}
					else if(rx_type == LEDROW1) {			
						i1 =  1 << (7-(rx[0] & 0x07));
						i2 = rx[0] & 0x08;
						
						if(i2<8) {
							for(i3=0;i3<8;i3++) {
								i4 = 1 << (7-i3);
								// OPTIMIZE??
								if(rx[1] & i4) display[0][i3] |= i1;
								else display[0][i3] &= ~i1;												
							}
						
							for(i3=0;i3<8;i3++)
								to_led(i3+1,display[0][i3],display[1][i3]);
						}
					}
					else if(rx_type == LEDROW2) {			
						i1 = 1 << (7-(rx[0] & 0x07));
						i2 = rx[0] & 0x08;
						
						if(i2<8) {
							for(i3=0;i3<8;i3++) {
								i4 = 1 << (7-i3);
								// OPTIMIZE??
								if(rx[1] & i4) display[0][i3] |= i1;
								else display[0][i3] &= ~i1;
							
								if(rx[2] & i4) display[1][i3] |= i1;
								else display[1][i3] &= ~i1;							
							}
						
							for(i3=0;i3<8;i3++)
								to_led(i3+1,display[0][i3],display[1][i3]);
						}
					}
					else if(rx_type == FRAME) {
						i1 = rx[0] & 0x03;
						
						if(i1<2) {
							for(i2=0;i2<8;i2++) {
								i4 = 1 << i2;
								for(i3=0;i3<8;i3++) {
									if(rx[8-i2] & (1 << (7-i3))) display[i1][i3] |= i4;
									else display[i1][i3] &= ~i4;												
								}
							}
						
							for(i3=0;i3<8;i3++)
								to_led(i3+1,display[0][i3],display[1][i3]);
						}
					}
					else if(rx_type == LEDON) {
						i1 = rx[1] & 0x07;
						i2 = (rx[1] >> 4) & 0x07;
						i3 = ((rx[1] & 0x80) >> 7) + ((rx[1] & 0x08) >> 2);
						
						if(i3<2) {
							i1 = 7-i1;
							i2 = 7-i2;
							
							display[i3][i2] |= (1<<(i1));
						
							to_led(i2+1,display[0][i2],display[1][i2]);					
						}
					}
					else if(rx_type == LEDOFF) {
						i1 = rx[1] & 0x07;
						i2 = (rx[1] >> 4) & 0x07;
						i3 = ((rx[1] & 0x80) >> 7) + ((rx[1] & 0x08) >> 2);
						
						if(i3<2) {
							i1 = 7-i1;
							i2 = 7-i2;
							
							display[i3][i2] &= ~(1<<i1);
						
							to_led(i2+1,display[0][i2],display[1][i2]);					
						}
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
							to_led(i3+1,display[0][i3],display[1][i3]);					
					}
				}

				PORTC |= C3_RD;
				_delay_us(1);			// wait for valid data
			}

			// ====================== check/send keypad data
			PORTD = 0;                      // setup PORTD for output
			DDRD = 0xFF;


			for(i1=0;i1<8;i1++) {
				// sel row
				i2 = i1 << 4;				// to prevent momentary flicker to row 0
				PORTB = i2;
				
				button_last[i1] = button_current[i1];
				button_last[i1+8] = button_current[i1+8];
				//button_last[i1+16] = button_current[i1+16];
				//button_last[i1+24] = button_current[i1+24];
				
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
	
						PORTC |= C2_WR;
						PORTD = i4 << 4;
						//_delay_us(1);
						PORTC &= ~(C2_WR);

						PORTC |= C2_WR;
						PORTD = ((i1+8)<<4) | (7-i2);
						//_delay_us(1);
						PORTC &= ~(C2_WR);
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
	
						PORTC |= C2_WR;
						PORTD = i4 << 4;
						//_delay_us(1);
						PORTC &= ~(C2_WR);

						PORTC |= C2_WR;
						PORTD = ((i1)<<4) | (7-i2);
						//_delay_us(1);
						PORTC &= ~(C2_WR);
					}
					
				


					PORTE |= (E6_CLK);
					_delay_us(1);
					PORTE &= ~(E6_CLK);
					
					_delay_us(1);
				}

				PORTE &= ~(E7_LD);	
			}

			
			// ====================== check usb/sleep status
			if(!(PINB & B7_USB)) {
				usb_state = 0;
				for(i1=0;i1<16;i1++) {	// fade out
					to_all_led(10, 15-i1);
					_delay_ms(8);
					_delay_ms(8);
					_delay_ms(8);
					_delay_ms(8);
				}
				
				for(i1=0;i1<4;i1++)			// erase display buffer
					for(i2=0;i2<8;i2++)
						display[i1][i2] = 0;
						
				for(i1 = 1; i1 < 9; i1++) to_all_led(i1, 0);				// clear pattern
				to_all_led(10,15);											// full brightness
				
				for(i1=0;i1<255;i1++) _delay_ms(8);
			} 
			else if(PINC & C4_PWREN) {
				sleep_state = 1;

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
