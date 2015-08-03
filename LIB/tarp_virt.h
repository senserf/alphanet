#ifndef __tarp_virt_h
#define __tarp_virt_h
/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2014 			*/
/* All rights reserved.							*/
/* ==================================================================== */

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
