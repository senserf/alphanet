/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014.                          */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "vartypes.h"
#include "lit.h"

/* Board-specifics... for SPOT it seems that this will be only for tags */
#if PTYPE == PTYPE_PEG
#error nur fur tags
error nur fur tags
#endif

#if BTYPE == BTYPE_CHRONOS
#include "chro_tag.h"

void set_alrm (word a) {
	chronos.alrm_id = a;
	chronos.alrm_seq++;
	chro_hi ("ALAR");
	chro_lo ("ON");
	if (!running (beep))
		runfsm beep(1);
	trigger (TRIG_ALRM);
	clr_lit (YES);
	set_lit (2, LED_ALRM, LED_ON, 0);
}

void clr_alrm () {
	chronos.alrm_id = 0;
}
#endif

#if BTYPE == BTYPE_WARSAW
#include "war_tag.h"

void set_alrm (word a) {
	warsaw.random_shit = (word)rnd();
	warsaw.alrm_id = a > 7 ? 7 : a;
	warsaw.alrm_seq++;
	trigger (TRIG_ALRM);
	clr_lit (YES);
	set_lit (2, LED_ALRM, LED_ON, 0);
}

void clr_alrm () {
	warsaw.alrm_id = 0;
	warsaw.random_shit = 0;
}
#endif

#if BTYPE == BTYPE_AT_BUT6
#include "ap319_tag.h"

void set_alrm (word a) {
	ap319.alrm_id = a;
	ap319.alrm_seq++;
	trigger (TRIG_ALRM);
	clr_lit (YES);
	set_lit (2, LED_ALRM, LED_ON, 0);
}

void clr_alrm () {
	ap319.alrm_id = 0;
}
#endif

#if BTYPE == BTYPE_AT_BUT1
#include "ap320_tag.h"

void set_alrm (word a) {
	ap320.alrm_id = a;
	ap320.alrm_seq++;
	trigger (TRIG_ALRM);
	clr_lit (YES);
	set_lit (2, LED_ALRM, LED_ON, 0);
}

void clr_alrm () {
	ap320.alrm_id = 0;
}
#endif
