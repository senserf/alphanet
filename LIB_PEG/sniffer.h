/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __sniffer_h
#define	__sniffer_h

/* It is truly hard to guess: Likely, the i/f should be very simple or even binary, Let's try to stir it a bit:
SNIF_NID_VIRG (default): virgin: tcv_plug not called, please a void placeholder role in sniffer ctrl requests
SNIF_NID_OFF: sniffer off (now: tcv_close is called, fsm sniff finished)
SNIF_NID_SAME: SID is not touched, all packets passed to TARP
SNIF_NID_ALL: SETSID to NONE, all packets with sid = net_id or 0 are passed to TARP
SNIF_NID_OPPO: SETSID to NONE, no packet passed to TARP (I'm not sure how much sense it makes, but it may and is free)

SNIF_REP_VOID: no change requested, a placeholder never stored
SNIF_REP_SAME (default): only (all) packets with sid = net_id or 0 are reported
SNIF_REP_ALL: all reported
SNIF_REP_OPPO: reported what is not passed to TARP or not addressed to the host node
*/
#define SNIF_NID_VOID	0
#define SNIF_NID_VIRG	0
#define SNIF_NID_OFF	1
#define SNIF_NID_SAME	2
#define SNIF_NID_ALL	3
#define SNIF_NID_OPPO	4

#define SNIF_REP_VOID	0
#define SNIF_REP_SAME	1
#define SNIF_REP_ALL	2
#define SNIF_REP_OPPO	3
 
typedef struct snifStruct {
	char fd;			// -1 .. 127
	byte nid_opt	:3; // void / virgin(0), off(1), as praxis(2), all(3), opposite(4)
	byte rep_opt	:2; // void(0), as praxis (1), all (2), opposite (3)
	byte spare		:3;
} snif_t;

extern snif_t snifcio;
extern word sniffer_ctrl (byte nid, byte rep);

//+++ "sniffer.cc"

#endif
