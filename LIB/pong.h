#ifndef __pong_h__
#define __pong_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014                           */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "commons.h"

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
