#ifndef __inout_h__
#define __inout_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014                           */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "sysio.h"
#include "tarp_virt.h"

void init_inout ();
void talk (char * buf, sint siz, word whoto);
fsm hear;

//+++ inout.cc

#endif
