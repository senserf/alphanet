#ifndef __chro_tag_h__
#define __chro_tag_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications 2014                            */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "commons.h"

typedef struct chroStruct {
	lword	ev_ts;		// internal activity monitor
	word	move_ts; 	// last movement's (short) timestamp
	word	move_nr;	// movement counter (over uptime mod 2^16)

	word	volt;
	word	acc_mode :2;	// 0-off, 1-mot, 2-immob, 3-3D (not impl)
	word	alrm_id  :3;	// which alarm raised
	word	alrm_seq :4;	// alarm seq (so host can spot missing)
	word    last_but :3;
	word	spare	 :4;
} chro_t;
extern chro_t	chronos;

void chro_init ();

void chro_hi (const char *txt);
void chro_lo (const char *txt);
void chro_nn (word hi, word a);
void chro_xx (word hi, word a);

fsm buzz;
fsm beep;

//+++ chro_tag.cc
#endif
