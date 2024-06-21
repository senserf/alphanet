/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __pegs_h
#define __pegs_h

#include "sysio.h"
//+++ pegs.cc

#define PMOD_REG	0
#define	PMOD_CONF	1
#define PMOD_CUST	2
#define PMOD_EXCC	3

// this is for conf_id below, the length may change
#define PMOD_MASK	0x1FFF

// for easy check... I think it is CC1100_MAXPLEN -8
#define MAX_PLEN	52

typedef struct pegflStruct {
	word 	learn_mod	:1;
	word	peg_mod		:2;
	word 	conf_id		:13;	// half-serious insurance against wandering packets
} pegfl_t;

extern pegfl_t	pegfl;

#endif
