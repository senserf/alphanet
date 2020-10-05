/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "plug_null.h"

///////////////// ossi ////////////////
// lin: last seq# in  (no FG_ACKR)
// lout: last seq# out(no FG_ACKR)
// lin_stat: last status (of cmd in) or RC_OK if not calculated
// lout_ack: last seq# with FG_ACKR set STILL ACTIVE (retrying)

// out frame has its length on byte 0
/////////////////////////////////////////////////////////////////
typedef struct ossiStruct {
	word lin :8;
	word lout :8;
	word lin_stat :8;
	word lout_ack :8;
} ossi_t;
static ossi_t ossi = {0, 0, 0, 0};

static fifek_t oss_cb;
static sint oss_fd;
static char * urgent = NULL;

// if we compile with the 2 #ifs switched off, we'll be overwriting circ. buf;
// i.e. old lines will be lost (instead of new ones).
static word _oss_out (char * b, Boolean urg) {

	if (b == NULL)
		return 2;
		
	if (urg) {
		if (urgent) {
			app_diag_S ("urg owrite %x", urgent);
			ufree (urgent);
		}
		urgent = b;
		trigger (TRIG_OSSO);
		return 0;
	}
	
#if 1
	if (!fifek_full (&oss_cb)) {
#endif
		fifek_push (&oss_cb, b);
		trigger (TRIG_OSSO);
		return 0;
#if 1
	}
	// we (arbitrarily) decide: don't overwrite
	app_diag_S ("OSS oflow (%u)", (word)seconds());
	ufree (b);
	return 1;
#endif
}
 
fsm perp_oss_out () {
	char * ptr;
	address pkt;
	byte rtr, urg_fl;

	state CHECK:
		if (urgent == NULL && fifek_empty (&oss_cb)) {
			when (TRIG_OSSO, CHECK);
			release;
		}
		if (urgent) {
			ptr = urgent;
			urgent = NULL;
			urg_fl = YES;
		} else {
			ptr = fifek_pull (&oss_cb);
			urg_fl = NO;
		}
		
		if (ptr[0] < 5) { // wrong len
			app_diag_S ("OSSO len %d", (sint)ptr[0]);
			ufree (ptr);
			if (urg_fl)
				urg_fl = NO;
			proceed CHECK;
		}
		
	state WNP:
		if (urg_fl) {
			pkt = tcv_wnpu (WNP, oss_fd, (sint)ptr[0]);
			urg_fl = NO;
		} else {
			pkt = tcv_wnp (WNP, oss_fd, (sint)ptr[0]);
		}

		if (ptr[4] == 0) // (n)ack
			goto OutCrap;

		// this is 'send next' (not a retry)
		if (++ossi.lout > 127)
			ossi.lout = 2;
			
		if ((ptr[1] & 0x80)) { // we'll wait for (n)ack & retry
			ptr[1] |= ossi.lout;
			ossi.lout_ack = ptr[1];
			rtr = 3; // 1 would be no retries but with a pause for ack
		} else {
			ptr[1] = ossi.lout;
			ossi.lout_ack = 0;
			rtr = 0;
		}
		
OutCrap:
		memcpy ((char *)pkt, ptr +1, (sint)ptr[0]);
		tcv_endp (pkt);
		
		if (rtr == 0)
			sameas DONE;
			
		when (TRIG_OSSIN, DONE);
#ifdef __SMURPH__
		delay (500, --rtr != 0 ? RTRY : NOACK);
#else
		delay (50, --rtr != 0 ? RTRY : NOACK);
#endif
		release;
		
	state RTRY:
		pkt = tcv_wnpu (RTRY, oss_fd, (sint)ptr[0]);
		goto OutCrap;
		
	state NOACK:
		app_diag_W ("NoAck %x", (word)ptr[1]);
		// fall through
	state DONE:
		ufree (ptr);
		ossi.lout_ack = 0;
		proceed (CHECK);
}

/////////////////

fsm cmd_in;
void oss_ini () {

	phys_uart (1, 80, 0); // 80B, not used anywhere else(?)
	tcv_plug (1, &plug_null); // tarp is 0
	
	// we should be returning, the caller blinking, whatever
	if ((oss_fd = tcv_open (WNONE, 1, 1)) < 0) { // last arg, is the (null) plugin
		app_diag_F ("TCVE error");
		reset();
	}
	
	fifek_ini (&oss_cb, 15);
	if (!running (perp_oss_out))
		runfsm perp_oss_out;

	if (!running (cmd_in))
		runfsm cmd_in;
}

