/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __vartypes_h__
#define __vartypes_h__
#include "sysio.h"
/* Board- and pgm- specific */

// these pongPloadType. must be accessible to decode payloads
typedef struct pongPloadStruct0 {
	word	volt;
	word	move_ago;
	word	move_nr;
} pongPloadType0;		// 6 bytes

typedef struct pongPloadStruct1 {
	word	volt;
	word	move_ago;
	word	move_nr;
} pongPloadType1;		// 6 bytes

typedef struct pongPloadStruct2 {
	word	volt;
} pongPloadType2;

typedef struct pongPloadStruct3 {
	word	volt;
	word	dial :8; // 2 dials 0..99 or ..FF?
	word	glob :1;
	word 	spare:7;	// 4 bytes
} pongPloadType3;

typedef struct pongPloadStruct4 {
	word	volt;		// 2 bytes
} pongPloadType4;

typedef struct pongPloadStruct5 {
	word	volt;
	word	random_shit;
	word	steady_shit;	// 6 bytes
} pongPloadType5;

typedef struct pongPloadStruct6 {
	word	volt;
	byte	loop[4];	// 6 bytes
} pongPloadType6;

#define PTYPE_PEG	0
#define PTYPE_TAG	1

#ifdef	COMPAT_1_0
#define BTYPE_AT_BUT6_1_0			3
#define BTYPE_AT_BUT1_1_0			4
// Dummy values
#define BTYPE_ALPHANET_CHRONOS			14
#define BTYPE_ALPHANET_CHRONOS_WHITE		15
#else
#define BTYPE_ALPHANET_CHRONOS			3
#define BTYPE_ALPHANET_CHRONOS_WHITE		4
// Dummy values
#define BTYPE_AT_BUT6_1_0			14
#define BTYPE_AT_BUT1_1_0			15
#endif

#define BTYPE_SPARE0				0
#define BTYPE_AT_BASE				2
#define BTYPE_AT_BUT6				6
#define BTYPE_AT_BUT1				1
#define BTYPE_WARSAW				5
#define BTYPE_AT_LOOP				7

#if defined PGMLABEL_warp || defined PGMLABEL_a321p || defined PGMLABEL_launchxlp
#define PTYPE	PTYPE_PEG
#elif defined PGMLABEL_chrt || defined PGMLABEL_wart || defined PGMLABEL_a320t || defined PGMLABEL_a319t || defined PGMLABEL_a331t
#define PTYPE	PTYPE_TAG
#else
#error PTYPE?
#endif

#if defined(BOARD_ALPHANET_CHRONOS) || defined(BOARD_ALPHANET_CHRONOS_WHITE)

#ifdef	COMPAT_1_0
#error  "no CHRONOS in 1.0 compat mode"
#endif

#ifdef BOARD_ALPHANET_CHRONOS
#define BTYPE	BTYPE_ALPHANET_CHRONOS
typedef pongPloadType0 pongPloadType;
#else
#define BTYPE   BTYPE_ALPHANET_CHRONOS_WHITE
typedef pongPloadType1 pongPloadType;
#endif

#endif

#if defined(BOARD_ALPHANET_AP321_BASE) || defined(BOARD_ALPHANET_BASE_XCC430) || defined(BOARD_ALPHANET_BASE_WARSAW) || defined(BOARD_ALPHANET_AP441_BASE) || defined(BOARD_ALPHANET_AP336_BASE) || defined(BOARD_ALPHANET_BASE_LAUNCHXL) || defined(BOARD_ALPHANET_CP423_USB)

#define BTYPE   BTYPE_AT_BASE
typedef pongPloadType2 pongPloadType;
#endif

#ifdef BOARD_ALPHANET_AP319_BUTTONS
#define BTYPE   BTYPE_AT_BUT6
typedef pongPloadType3 pongPloadType;
#endif

#if defined(BOARD_ALPHANET_AP320_PANIC) || defined(BOARD_ALPHANET_PANIC_OLIMEX) || defined(BOARD_ALPHANET_TAG_WARSAW) || defined(BOARD_ALPHANET_SENSORTAG)
#define BTYPE   BTYPE_AT_BUT1
typedef pongPloadType4 pongPloadType;
#endif

#if defined BOARD_WARSAW || defined BOARD_WARSAW_BLUE
#define BTYPE   BTYPE_WARSAW
typedef pongPloadType5 pongPloadType;
#endif

#if defined(BOARD_ALPHANET_AP331_PANIC) || defined(BOARD_ALPHANET_AP331_PANIC_V32)
#define BTYPE   BTYPE_AT_LOOP
typedef pongPloadType6 pongPloadType;
#endif

#ifndef BTYPE
#error unsupported or undefined board
#endif

// likely not needed... free, anyway
#define NTYPE ((BTYPE << 4) | PTYPE)

#endif
