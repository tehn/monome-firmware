#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <inttypes.h>

#define kButtonStateDown 1
#define kButtonStateUp   0

#define kButtonDownEvent 1
#define kButtonUpEvent   0

#define kButtonDownDefaultDebounceCount 1
#define kButtonUpDefaultDebounceCount   20

#define kButtonNewEvent   1
#define kButtonNoEvent    0

extern uint8_t button_current[8],             // bitmap of physical button state (depressed or released)
             button_last[8],                // bitmap of physical button state last time buttons were read
			button_state[8],              // bitmap of debounced button state
   			button_debounce_count[8][8],   // debounce counters for each button
             button_event[8];               // queued button events (kButtonDownEvent or kButtonDownEvent)

void buttonInit(void);
void buttonCheck(uint8_t row, uint8_t index);


#endif
