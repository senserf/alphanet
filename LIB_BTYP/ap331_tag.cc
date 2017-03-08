/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2016                           */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "ap331_tag.h"
#include "alrms.h"
#ifndef  __SMURPH__
#include "buttons.h"
#endif
#include "diag.h"
#include "rfid.h"

ap331_t	ap331;

#define SENSE_LOOP_331 		1
#define	HALFTIME_331		1024
#define	MINCOUNT_331		3
#ifndef SENSOR_AS3932
#define SENSOR_AS3932		1
#endif

fsm debouncer {

	state LD_WAIT:

		wait_sensor (SENSOR_AS3932, LD_EVENT);

	state LD_EVENT:

		lword tmp;

		read_sensor (WNONE, SENSOR_AS3932, (address)&tmp);
		// tmp is guaranteed to be nonzero
		if (tmp == ap331.tentative) {
			// A repeat value
			if (ap331.debcnt >= MINCOUNT_331-1) {
				// Debounced and valid
				if (ap331.loop != ap331.tentative) {
					ap331.changed = YES;
					ap331.loop = ap331.tentative;
				}
				ap331.set = YES;
			} else
				ap331.debcnt++;
		} else {
			// A new value to debounce
			ap331.tentative = tmp;
			ap331.debcnt = 0;
		}

		sameas LD_WAIT;
}

fsm monloop {
	
	state ML_LOOP:

		ap331.tentative = 0;
		ap331.set = ap331.changed = NO;
		as3932_on ();
		runfsm debouncer;
		delay (HALFTIME_331, ML_READ);
		release;
		
	state ML_READ:

		as3932_off ();
		killall (debouncer);

		if (!ap331.set && ap331.loop) {
			// Moving out of loop; this is the only way for loop
			// to become zero
			ap331.loop = 0;
			set_alrm (LOOP_ALRM_ID);
		} else if (ap331.changed) {
			// From zero or from one loop to another (loop is
			// nonzero at this point)
			set_alrm (LOOP_ALRM_ID);
			if (!running (rfid)) {
				// reset or not to reset?
				// rfid_ctrl.cnt = 0;
				runfsm rfid;
			} else {
				rfid_ctrl.next = rfid_ctrl.ini;
				// reset or not to reset?
				// rfid_ctrl.cnt = 0;
				trigger (TRIG_RFID);
			}

		}

		delay (HALFTIME_331, ML_LOOP);
}

static void do_butt (word b) {
	set_alrm (b +1); // buttons 0.., alrms 1..
}

void ap331_init () {
	buttons_action (do_butt);
#if SENSE_LOOP_331
	runfsm monloop;
#endif
}
#undef IN_LOOP_331
#undef HALFTIME_331
#undef SENSE_LOOP_331
#undef MINCOUNT_331
