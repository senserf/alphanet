/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2014                    */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "commons.h"
#include "diag.h"
#include "tarp.h"
#include "vartypes.h"
#include "pegs.h"

/*
 * "Virtual" stuff needed by NET & TARP =======================================
 */
word guide_rtr (headerType *  b) {
		if (pegfl.peg_mod == PMOD_EXCC)
			return 0;
        return (b->rcv == 0 || b->msg_type  == msg_pong || b->msg_type  == msg_ping ||
                        b->msg_type == msg_pongAck ||
						b->msg_type == msg_rpc || b->msg_type == msg_rpcAck) ? 1 : 2;
}

int tr_offset (headerType *h) {
        // Unused ??
        return 0;
}

Boolean msg_isBind (msg_t m) {
        return NO;
}

Boolean msg_isTrace (msg_t m) {
        return NO;
}

Boolean msg_isNew (msg_t m) {
        return NO;
}

Boolean msg_isClear (byte o) {
        return YES;
}

#if PTYPE == PTYPE_PEG
#include "tag_mgr.h"

Boolean msg_isMaster (msg_t m) {
        return (m == msg_master && local_host != DEF_MHOST);
}

fsm mbeacon;
fsm looper;
void set_master_chg () {
	if (running (mbeacon)) { // I was the Master
		killall (mbeacon);
		tarp_ctrl.param |= 0x01; // routing ON
		app_diag_W ("Abdicating for %u", master_host);
		highlight_clear();
		tagList.block = NO;
		tarp_ctrl.param = 0xB1;
		runfsm looper;
#ifdef MASTER_STATUS_LED
		leds (MASTER_STATUS_LED, 0);
#endif
	} else {
		app_diag_W ("Set master to %u", master_host);
	}
}
#else
Boolean msg_isMaster (msg_t m) { return NO; }
void set_master_chg () { return; }
#endif

