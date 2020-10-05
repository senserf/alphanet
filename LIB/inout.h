/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __inout_h__
#define __inout_h__

#include "sysio.h"
#include "tarp_virt.h"

void init_inout ();
void talk (char * buf, sint siz, word whoto);
fsm hear;

//+++ inout.cc

#endif
