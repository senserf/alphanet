/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/

#include "rfid.h"
#include "vartypes.h"
#include "inout.h"
#include "diag.h"
#include "net.h"

#define _RFID_DBG 0

rfid_t rfid_ctrl = RFID_INIT;

// likely it'll be of ttyp as well, if used at all
#if BTYPE == BTYPE_AT_LOOP
#include "ap331_tag.h"
static void load_rfid (char * plo) {
	memcpy (plo, &(ap331.loop), rfid_ctrl.plen);
}
#else
// placeholder
static void load_rfid (char * plo) {
	lword ts = seconds();
	memcpy (plo, (char *)&ts, rfid_ctrl.plen);
}
#endif

fsm rfid {

	char *msg = NULL;

	state RFID_START:

		if ((msg = get_mem(sizeof(msgRfidType) + rfid_ctrl.plen, NO))
		    == NULL)
			finish;		

		in_header(msg, msg_type) = msg_rfid;
		in_header(msg, rcv) = 0;
		in_header(msg, hco) = 1;
		in_header(msg, prox) = 1;

		in_rfid(msg, btyp) = BTYPE;
		in_rfid(msg, ttyp) = RFID_TYPE_LOOP;
		in_rfid(msg, len) = rfid_ctrl.plen;
		rfid_ctrl.next = rfid_ctrl.ini;

	state RFID_LOOP:

		if (ap331.loop == 0) {
Die:
			ufree (msg);
			finish;
		}

		in_rfid(msg, next) = rfid_ctrl.next;
		in_rfid(msg, cnt) = rfid_ctrl.cnt;

		load_rfid (msg + sizeof(msgRfidType));

		++rfid_ctrl.cnt;

		if (rfid_ctrl.next < rfid_ctrl.max)
			rfid_ctrl.next += rfid_ctrl.inc;

		set_pxopts (0, 7, 0);
		talk (msg, sizeof(msgRfidType) + rfid_ctrl.plen, TO_NET);
#if _RFID_DBG
		app_diag_U ("rfid msg out #%u at %u", in_rfid(msg, cnt),
			(word)seconds ());
#endif
		if (rfid_ctrl.next == 0 || rfid_ctrl.next > 63) {
			app_diag_S ("rfid bad next %d", rfid_ctrl.next);
			goto Die;

		}
#if _RFID_DBG
		else
			app_diag_U ("rfid next in %d", rfid_ctrl.next);
#endif
		delay (rfid_ctrl.next << 10, RFID_LOOP);
		when (TRIG_RFID, RFID_LOOP);
		release;
}

#undef _RFID_DBG
