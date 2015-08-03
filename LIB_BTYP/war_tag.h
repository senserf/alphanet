#ifndef __war_tag_h__
#define __war_tag_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications 2014                            */
/* All rights reserved.                                                 */
/* ==================================================================== */
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
