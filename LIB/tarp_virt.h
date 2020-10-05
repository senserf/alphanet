/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __tarp_virt_h
#define __tarp_virt_h

// TARP's virtual functions

#include "tarp.h"

int tr_offset (headerType*);
Boolean msg_isBind (msg_t);
Boolean msg_isTrace (msg_t);
Boolean msg_isMaster (msg_t);
Boolean msg_isNew (msg_t);
Boolean msg_isClear (byte);
void set_master_chg ();
word  guide_rtr (headerType*);

//+++ tarp_virt.cc

#endif
