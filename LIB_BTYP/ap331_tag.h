#ifndef __ap331_tag_h__
#define __ap331_tag_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications 2016                            */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "commons.h"
#include "as3932.h"

/* vuee doesn't compile with as3932_data_t loop */
typedef struct ap331Struct {
	word	volt;
	word	alrm_id  :3;	// which alarm raised
	word	alrm_seq :4;	// alarm seq (so host can spot missing)
	word	spare	 :9;
	byte	loop[AS3932_NBYTES];
} ap331_t;
extern ap331_t	ap331;

void ap331_init ();

//+++ ap331_tag.cc
#endif
