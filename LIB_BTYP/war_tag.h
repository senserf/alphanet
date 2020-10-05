/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __war_tag_h__
#define __war_tag_h__
#include "commons.h"

typedef struct warStruct {
	word	volt;
	word	random_shit;
	word	alrm_id  :3;	// which alarm raised
	word	alrm_seq :4;	// alarm seq (so host can spot missing)
	word	spare	 :9;
} war_t;
extern war_t	warsaw;

void war_init ();

//+++ war_tag.cc
#endif
