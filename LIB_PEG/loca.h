/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2015                    */
/* All rights reserved.                                                 */
/* ==================================================================== */
#ifndef __loca_h
#define __loca_h

//+++ loca.cc

// be careful with messages: 32 maxes out current report packets
// also, this 32 is hardcoded in commons.h:msgLocaType
#define LOCAVEC_SIZ	32
typedef struct locaStruct {
	lword 	ts;
	word	id;
	word 	ref;
	byte 	vec[LOCAVEC_SIZ];
} loca_t;

extern loca_t	loca;
void loca_out (Boolean sendLoca);

#endif
