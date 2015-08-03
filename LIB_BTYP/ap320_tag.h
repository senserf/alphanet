#ifndef __ap320_tag_h__
#define __ap320_tag_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications 2014                            */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "commons.h"

typedef struct ap320Struct {
	word	volt;
	word	alrm_id  :3;	// which alarm raised
	word	alrm_seq :4;	// alarm seq (so host can spot missing)
	word	spare	 :9;
} ap320_t;
extern ap320_t	ap320;

void ap320_init ();

//+++ ap320_tag.cc
#endif
