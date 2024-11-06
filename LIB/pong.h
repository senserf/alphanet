/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __pong_h__
#define __pong_h__

#include "commons.h"

// 1 means OFF, 0 means 0N
#define	PING_LBT_SETTING	1

// changed on 16/04/22 from 5 to 30, after experiments in the AT building
#define	PING_SPACING		(30 + (rnd () & 7))

extern pongParamsType  pong_params;

extern char pong_frame [sizeof(msgPongType) + sizeof(pongPloadType)];
extern fifek_t pframe_stash;

void init_pframe ();
Boolean load_pframe (); // YES if ping should be run
void stash_pframe ();
void upd_pframe (word pl, word tnr); // signature may end up board-specific(?)

fsm pong;

//+++ pong.cc

#endif
