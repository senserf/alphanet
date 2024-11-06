/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __loca_h
#define __loca_h

//+++ loca.cc

// in seconds... likely, a single constant will do
// 10 comes from 4 tries 3s apart, after loca burst (3*3+1)
#define LOCA_TOUT_PING 10
#define LOCA_TOUT_PONG 10

// Audit at freq LOCA_TOUT_AUDIT/2. It seems better than unused old bursts.
// Should not be less than max(_PING, _PONG)
#define LOCA_TOUT_AUDIT 14

// number of simultaneous tag bursts collections
#define LOCA_SNUM	3

#ifdef __SMURPH__
#define LOCA_TRAC	1
#else
#define LOCA_TRAC	0
#endif

// be careful with messages: 32 maxes out current report packets
// also, this 32 is hardcoded in commons.h:msgLocaType
#define LOCAVEC_SIZ	32
#define LOCASHORT_SIZ	8
typedef struct locaStruct {
	lword 	ts;
	word	id;
	word 	ref;
	byte 	vec[LOCAVEC_SIZ];
} loca_t;

extern loca_t	locarr[];	// now 40B per slot

void loca_out (word ind, Boolean sendLoca);
word loca_find (word id, word tout);
void loca_ld (word lsl, word id, word ref, word bsl, word rss);
fsm locaudit;
#endif
