/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2014                    */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "lit.h"

lit_t lit;

fsm lit_mgr {

	state SON:
		lit.stat = 1;
		leds (lit.which, lit.what);
		delay (lit.span << 10, SOFF);
		release;

	state SOFF:
		leds (lit.which, LED_OFF);
		lit.stat = 0;
		if (lit.lood == 0) 
			finish;
		delay (lit.lood << 10, SON);
		release;
}

void clr_lit (Boolean kim) {
	if (kim)
		killall (lit_mgr);
	leds (lit.which, LED_OFF);
	memset (&lit, 0, sizeof (lit_t));
}

void set_lit (word spa, word whi, word wha, word loo) {
	lit.span = spa;
	lit.which = whi;
	lit.what = wha;
	lit.lood = loo;
	if (!running (lit_mgr))
		runfsm lit_mgr;
}
