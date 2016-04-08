#ifndef __vartypes_h__
#define __vartypes_h__
/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014.                          */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "sysio.h"
/* Board- and pgm- specific */

// these pongPloadType. must be accessible to decode payloads
typedef struct pongPloadStruct0 {
	word	volt;
	word	move_ago;
	word	move_nr;
} pongPloadType0;

typedef struct pongPloadStruct1 {
	word	volt;
	word	move_ago;
	word	move_nr;
} pongPloadType1;

typedef struct pongPloadStruct2 {
	word	volt;
} pongPloadType2;

typedef struct pongPloadStruct3 {
	word	volt;
	word	dial :8; // 2 dials 0..99 or ..FF?
	word	glob :1;
	word 	spare:7;
} pongPloadType3;

typedef struct pongPloadStruct4 {
	word	volt;
} pongPloadType4;

typedef struct pongPloadStruct5 {
	word	volt;
	word	random_shit;
	word	steady_shit;
} pongPloadType5;

#define PTYPE_PEG	0
#define PTYPE_TAG	1

#define BTYPE_SPARE0				0
#define BTYPE_CHRONOS				3
#define BTYPE_AT_BUT6_1_0			3
#define BTYPE_CHRONOS_WHITE			4
#define BTYPE_AT_BUT1_1_0			4
#define BTYPE_AT_BASE				2
#define BTYPE_AT_BUT6				6
#define BTYPE_AT_BUT1				1
#define BTYPE_WARSAW				5
#define BTYPE_SPARE7				7

#if defined PGMLABEL_warp || defined PGMLABEL_a321p
#define PTYPE	PTYPE_PEG
#elif defined PGMLABEL_chrt || defined PGMLABEL_wart || defined PGMLABEL_a320t || defined PGMLABEL_a319t
#define PTYPE	PTYPE_TAG
#else
#error PTYPE?
#endif

#ifdef BOARD_CHRONOS
#define BTYPE	BTYPE_CHRONOS
typedef pongPloadType0 pongPloadType;
#endif

#ifdef BOARD_CHRONOS_WHITE
#define BTYPE   BTYPE_CHRONOS_WHITE
typedef pongPloadType1 pongPloadType;
#endif

#if defined(BOARD_ALPHANET_AP321_BASE) || defined(BOARD_ALPHANET_BASE_XCC430) || defined(BOARD_ALPHANET_BASE_WARSAW)
#define BTYPE   BTYPE_AT_BASE
typedef pongPloadType2 pongPloadType;
#endif

#ifdef BOARD_ALPHANET_AP319_BUTTONS
#define BTYPE   BTYPE_AT_BUT6
typedef pongPloadType3 pongPloadType;
#endif

#if defined(BOARD_ALPHANET_AP320_PANIC) || defined(BOARD_ALPHANET_AP331_PANIC) || defined(BOARD_ALPHANET_PANIC_OLIMEX) || defined(BOARD_ALPHANET_TAG_WARSAW)
#define BTYPE   BTYPE_AT_BUT1
typedef pongPloadType4 pongPloadType;
#endif

#if defined BOARD_WARSAW || defined BOARD_WARSAW_BLUE
#define BTYPE   BTYPE_WARSAW
typedef pongPloadType5 pongPloadType;
#endif

#ifndef BTYPE
#error unsupported board
#endif

// likely not needed... free, anyway
#define NTYPE ((BTYPE << 4) | PTYPE)

#endif
