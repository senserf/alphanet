#ifndef __lit_h__
#define __lit_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014.                          */
/* All rights reserved.                                                 */
/* ==================================================================== */

//+++ lit.cc

//////////////////////////////////
// all practical leds controls amount to quite involving operations,
// I believe vastly redundant for any given praxis. So, we have
// just the structs and basic fsm here, to draw from.
// With the same rationale: this is just simple 1-led controls, likely to
// spill over to buzzer / beeper.
//////////////////////////////////

#include "vartypes.h"

#define LED_R   0
#define LED_G   1
#define LED_B   2
#define LED_4   3

#define LED_OFF 0
#define LED_ON  1
#define LED_BLINK 2

// limit LEDs to tags (for modularity illustration?)
#if PTYPE == PTYPE_TAG

#if BTYPE == BTYPE_AT_BUT6 || BTYPE == BTYPE_AT_BUT1
#define LED_ALRM LED_4
#else
#define LED_ALRM LED_R
#endif

#else
#error no LED_ALRM
error no LED_ALRM FIXME;
#endif

typedef struct litStruct {
	word	span :6;
	word	which :2;
	word	what :2;
	word	lood :4; // loop delay
	word	stat :1; // state (if we care)
	word	spare :1;
} lit_t;

extern lit_t lit;

void set_lit (word spa, word whi, word wha, word loo);
void clr_lit (Boolean kim); // kill him

#endif
