#ifndef __rfid_h__
#define __rfid_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2016                           */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "commons.h"

// _READY msu be zero
#define RFID_READY	0
#define RFID_STOP	1
// #define RFID_RESET all else ican be implicit... I think

// there will be blood
#define RFID_TYPE_LOOP 1

// likely, another function of btyp / ttyp
#define RFID_INIT	{0, 4, 1, 1, 5, 0, 0}
typedef struct {
	word act 	:3;
	word plen	:5;
	word ini	:8;
	word inc	:8;
	word max	:8;
	word cnt	:8;
	word spare	:8;
} rfid_t;
extern rfid_t rfid_ctrl;

fsm rfid;

//+++ rfid.cc

#endif
