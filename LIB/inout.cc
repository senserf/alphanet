/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014                           */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "commons.h"
#include "diag.h"
#include "net.h"
#include "tarp.h"
#include "vartypes.h"

void process_incoming (char * buf, word siz, word rssi);

// these two:
void oss_tx (char * buf, word siz);
void oss_ini ();
// are in:
#if PTYPE == PTYPE_PEG

#if BTYPE == BTYPE_WARSAW
//+++ oss_peg_ser.cc
#elif BTYPE == BTYPE_AT_BASE
//+++ oss_peg_tcve.cc
#else
//+++ oss_null.cc
#endif

#else // tags

#if BTYPE == BTYPE_WARSAW
//+++ oss_tag_ser.cc
#else
//+++ oss_null.cc
#endif

#endif

void init_inout () {
	net_id = DEF_NID;
	if (net_init (INFO_PHYS_CC1100, INFO_PLUG_TARP) < 0) {
                app_diag_F ("net_init failed");
                reset();
        }
	net_opt (PHYSOPT_SETSID, &net_id);
	local_host = (word)host_id;
	oss_ini ();
}

fsm hear {

        char *buf;
        sint psize;
        word rssi;

        entry RC_TRY:

                if (buf) {
                        ufree (buf);
                        buf = NULL;
                        psize = 0;
                }
                psize = net_rx (RC_TRY, &buf, &rssi, 0);
                if (psize <= 0) {
                        app_diag_S ("net_rx failed (%d)", psize);
                        proceed RC_TRY;
                }
// too much for most debugging
#if 0
                app_diag_D ("RCV (%d): %x-%u-%u-%u-%u-%u\r\n",
                          psize, in_header(buf, msg_type),
                          in_header(buf, seq_no) & 0xffff,
                          in_header(buf, snd),
                          in_header(buf, rcv),
                          in_header(buf, hoc) & 0xffff,
                          in_header(buf, hco) & 0xffff);
#endif
        entry RC_MSG:

                process_incoming (buf, psize, rssi >> 8);
                proceed RC_TRY;
}

/* whoto: 0 - TO_ALL; 1 - TO_NET; 2 - TO_OSS; more? */
void talk (char * buf, sint size, word whoto) {

	if (buf == NULL) {
		app_diag_S ("talk NULL");
		return;
	}

        if (in_header(buf, rcv) != local_host && whoto != TO_OSS) {
// dupa fix this net_tx, swerrs & dbg in it?
        	if (net_tx (WNONE, buf, size, 0) == 0) {
                	app_diag_D ("Sent(%d) %u to %u", size,
                        	in_header(buf, msg_type),
                        	in_header(buf, rcv));
        	} else {
                	app_diag_S ("NETx %u failed", in_header(buf, msg_type));
		}
	}

	if (in_header(buf, rcv) == local_host || whoto != TO_NET)
		oss_tx (buf, size);
}

