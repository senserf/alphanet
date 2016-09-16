/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2016                    */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "commons.h"
#include "inout.h"
#include "looper.h"
#include "diag.h"
#include "tag_mgr.h"
#include "msg_io_peg.h"
#include "net.h"
#include "loca.h"

fsm mbeacon;
static void init () {
	// word pl = 7;

	master_host = DEF_MHOST;

	init_inout ();
	// net_opt (PHYSOPT_SETPOWER, &pl);
	set_pxopts (0, 7, 0);

	reset_tags();
	if (local_host == master_host) {
#ifdef MASTER_STATUS_LED
		leds (MASTER_STATUS_LED, 2);
#endif
        tarp_ctrl.param = 0xB2; // level 2, rec 3, slack 1, fwd off
	// tarp_ctrl.param = 0xB0; // trying slack 0
		runfsm mbeacon;
		// Set to NO to make the master receive Tag alarms
		tagList.block = YES;
	} else {
#ifdef MASTER_STATUS_LED
		leds (MASTER_STATUS_LED, 0);
#endif
        	tarp_ctrl.param = 0xB3; // level 2, rec 3, slack 1, fwd on
		// tarp_ctrl.param = 0xB1; // trying slack 0
		runfsm looper;
		runfsm locaudit;
	}
	runfsm hear;
}

fsm mbeacon {
	msgMasterType mf;
	word tmp; // use hold if long pauses become a feature

    state INI:	
	tmp = 0;
	memset (&mf, 0, sizeof(msgMasterType));
	mf.header.msg_type = msg_master;
	highlight_set (0, 0.0, NULL);

    state SEND:
	tmp = 0;
	talk ((char *)&mf, sizeof(msgMasterType), TO_NET);

    state DEL:
	if ((++tmp % 11) == 0) // % N <=> N-1 delays (N==1 is a quick but painful death)
		proceed SEND;
	when (TRIG_MBEAC, SEND);
	delay ((58 + (rnd() % 5)) << 10, DEL); // 60 +/- 2s
	release;

}

void process_incoming (char * buf, word size, word rssi) {

  switch (in_header(buf, msg_type)) {

	case msg_pong:
		if (tagList.block == NO)
			msg_pong_in (buf, rssi);
		break;

	case msg_ping:
		if (tagList.block == NO) // let's keep ping / pong off the master
			msg_ping_in (buf, rssi);
		break;
		
	case msg_loca:
		talk (buf, size, TO_OSS);
		// app_diag_S ("loca %u", size);
		break;

	case msg_report:
		msg_report_in (buf, size);
		// app_diag_S ("rep %u", size);
		break;

	case msg_reportAck:
		msg_reportAck_in (buf);
		break;

	case msg_fwd:
		msg_fwd_in (buf, size);
		break;

	case msg_fwdAck:
	case msg_rpc:
	case msg_rpcAck:
		talk (buf, size, TO_OSS);
		break;

	case msg_master:
		// handled in TARP, nothing more to do - not any more, a (temporary) kludge in msg_master_in
		msg_master_in (buf);
		break;

	case msg_nh:
		msg_nh_in (buf, rssi);
		break;

	case msg_nhAck:
		// harmless cheating:
		in_header(buf, seq_no) = rssi;
		talk (buf, size, TO_OSS);
		break;

	default:
		app_diag_S ("Got ?(%u)", in_header(buf, msg_type));

  }
}

// ==========================================================================

fsm root {

	state START:
#ifdef __SMURPH__
                // spread a bit synced node starts
                delay (rnd() % 1000, INIT);
                release;
#endif
	state INIT:
		init ();
		finish;
}

