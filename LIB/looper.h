#ifndef __looper_h__
#define __looper_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014                           */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "sysio.h"
#include "vartypes.h"

extern word heartbeat;

fsm looper;

#if PTYPE == PTYPE_PEG
//+++ looper_peg.cc
#else
//+++ looper_tag.cc
#endif

#endif
