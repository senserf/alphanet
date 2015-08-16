#ifndef __pong_h__
#define __pong_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014                           */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "commons.h"

// 1 means OFF, 0 means 0N
#define	PING_LBT_SETTING	1
#define	PING_SPACING		(5 + (rnd () & 7))

extern pongParamsType  pong_params;

extern char pong_frame [sizeof(msgPongType) + sizeof(pongPloadType)];
extern fifek_t pframe_stash;

void init_pframe ();
void load_pframe ();
void stash_pframe ();
void upd_pframe (word pl, word tnr); // signature may end up board-specific(?)

fsm pong;

//+++ pong.cc

#endif
