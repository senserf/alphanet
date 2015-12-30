/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2015                    */
/* All rights reserved.                                                 */
/* ==================================================================== */
#ifndef __loca_h
#define __loca_h

//+++ loca.cc

// in seconds... likely, a single constant will do
#define LOCA_TOUT_PING 5
#define LOCA_TOUT_PONG 5

#ifdef __SMURPH__
#define LOCA_TRAC	1
#else
#define LOCA_TRAC	0
#endif

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
