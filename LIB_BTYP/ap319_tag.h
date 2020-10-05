/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __ap319_tag_h__
#define __ap319_tag_h__
#include "commons.h"

typedef struct ap319Struct {
	word	volt;
	word	dial	:8;
	word	gmap	:8;
	word	alrm_id  :3;	// which alarm raised
	word	alrm_seq :4;	// alarm seq (so host can spot missing)
	word	spare	 :9;
} ap319_t;
extern ap319_t	ap319;

void ap319_init ();
void ap319_rsw ();

//+++ ap319_tag.cc
#endif
