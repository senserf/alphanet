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

// without it AP331 is deaf loop-wise (monloop is not run and as3932 is never on).
// when we (ever) know how to tackle tag provisioning, this should be a configurable parameter.
#define SENSE_LOOP_331 1

// #define IN_LOOP_331 (curr[0] || curr[1] || curr[2] || curr[3])
#define IN_LOOP_331 (*((lword*)(&(ap331.loop))) != 0)
// #define CHANGED_331	(memcmp ((const byte *)(&curr), (const byte *)(&(ap331.loop)), AS3932_NBYTES))
#define CHANGED_331	(*((lword*)(&(ap331.loop))) != *((lword*)(&curr)))
// #define COPY_331	memcpy ((byte *)(&(ap331.loop)), (const byte *)(&curr), AS3932_NBYTES)
#define COPY_331	*((lword*)(&(ap331.loop))) = *((lword*)(&curr))
#define	HALFTIME_331		1024

#ifndef SENSOR_AS3932
// why is this needed for vuee? dupa
#define SENSOR_AS3932	1
#endif

fsm monloop {
	/* vuee doesn't compile with as3932_data_t curr */
	
	state ML_LOOP:

		as3932_on ();
		delay (HALFTIME_331, ML_READ);
		release;
		
	state ML_READ:

		byte curr [AS3932_NBYTES];

		read_sensor (ML_READ, SENSOR_AS3932, (address)(&curr));
		as3932_off ();
		
		if (CHANGED_331) {
			COPY_331;
			set_alrm (LOOP_ALRM_ID);
			if (IN_LOOP_331) {
				rfid_ctrl.act = RFID_READY; // I'm not sure if a race is even possible, but
				if (running (rfid)) {
					trigger (TRIG_RFID);
				} else {
					runfsm rfid;
				}
			} else { // just out of loops
				rfid_ctrl.act = RFID_STOP;
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
#undef CHANGED_331
#undef COPY_331
#undef HALFTIME_331
#undef SENSE_LOOP_331
