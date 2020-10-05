/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __rfid_h__
#define __rfid_h__

#include "commons.h"

// _READY msu be zero
#define RFID_READY	0
#define RFID_STOP	1
// #define RFID_RESET all else ican be implicit... I think

// there will be blood
#define RFID_TYPE_LOOP 1

// likely, another function of btyp / ttyp
#define RFID_INIT	{4, 1, 1, 5, 0, 0}
typedef struct {
	word plen	:8;
	word ini	:8;
	word inc	:8;
	word max	:8;
	word next	:8;
	word cnt	:8;
} rfid_t;
extern rfid_t rfid_ctrl;

fsm rfid;

//+++ rfid.cc

#endif
