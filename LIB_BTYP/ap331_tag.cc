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

ap331_t	ap331;

#define IN_LOOP_331 (curr[0] || curr[1] || curr[2] || curr[3])
#define CHANGED_331	(memcmp ((const byte *)(&curr), (const byte *)(&(ap331.loop)), AS3932_NBYTES))
#define COPY_331	memcpy ((byte *)(&(ap331.loop)), (const byte *)(&curr), AS3932_NBYTES)
#define FREQ_IN_LOOP_331	10
#define FREQ_OFF_LOOP_331	2

#ifndef SENSOR_AS3932
// why is this needed for vuee? dupa
#define SENSOR_AS3932	1
#endif

fsm monloop {
	/* vuee doesn't compile with as3932_data_t curr */
	byte curr[AS3932_NBYTES];
	
	state ML_LOOP:
		as3932_on();
		
	state ML_READ:
		read_sensor (ML_READ, SENSOR_AS3932, (address)(&curr));
		as3932_off();
		
		if (CHANGED_331) {
			COPY_331;
			set_alrm (LOOP_ALRM_ID);
		}
		
		// would wait_sensor () off loop be much more expensive (no as3932_off)? dupa
		delay (IN_LOOP_331 ? FREQ_IN_LOOP_331 << 10 : FREQ_OFF_LOOP_331 << 10, ML_LOOP);
		release;
}
#undef IN_LOOP_331
#undef CHANGED_331
#undef COPY_331
#undef FREQ_IN_LOOP_331
#undef FREQ_OFF_LOOP_331

static void do_butt (word b) {
	set_alrm (b +1); // buttons 0.., alrms 1..
}

void ap331_init () {
	buttons_action (do_butt);
	runfsm monloop;
}

