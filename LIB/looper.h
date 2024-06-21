/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __looper_h__
#define __looper_h__

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
