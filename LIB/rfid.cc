/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2016                           */
/* All rights reserved.                                                 */
/* ==================================================================== */

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
	memcpy (plo, ap331.loop, rfid_ctrl.plen);
}
#else
// placeholder
static void load_rfid (char * plo) {
	lword ts = seconds();
	memcpy (plo, (char *)&ts, rfid_ctrl.plen);
}
#endif

fsm rfid {
	char * msg = NULL;	// this is the only redundant msg = NULL here, I accept bets

	state TRIG:
		ufree (msg);
		msg = NULL;
		if (rfid_ctrl.act == RFID_STOP) {
			rfid_ctrl.act = RFID_READY;
#if _RFID_DBG
			app_diag_U ("rfid stopped");
#endif
			finish;
		}
		// init or reload
		rfid_ctrl.act = RFID_READY;
		if ((msg = get_mem(sizeof(msgRfidType) + rfid_ctrl.plen, NO)) == NULL)
			finish;		
		memset (msg, 0, sizeof(msgRfidType) + rfid_ctrl.plen);
		in_header(msg, msg_type) = msg_rfid;
		in_header(msg, rcv) = 0;
		in_header(msg, hco) = 1;
		in_header(msg, prox) = 1;
		in_rfid(msg, btyp) = BTYPE;
		in_rfid(msg, ttyp) = RFID_TYPE_LOOP;
		in_rfid(msg, next) = rfid_ctrl.ini;
		in_rfid(msg, cnt) = 0;
		in_rfid(msg, len) = rfid_ctrl.plen;
		load_rfid (msg + sizeof(msgRfidType));
		delay ((word)rfid_ctrl.ini << 10, ITER);
		release;
		
	state ITER:
		if (memcmp (msg + sizeof(msgRfidType), ap331.loop, rfid_ctrl.plen)) {
#if _RFID_DBG
			app_diag_U ("rfid kicked");
#endif
			sameas TRIG;
		}
		++in_rfid(msg, cnt);
		if (in_rfid(msg, next) < rfid_ctrl.max)
			in_rfid(msg, next) += rfid_ctrl.inc;

		set_pxopts (0, 7, 0);
		talk (msg, sizeof(msgRfidType) + rfid_ctrl.plen, TO_NET);
#if _RFID_DBG
		app_diag_U ("rfid msg out #%u at %u", in_rfid(msg, cnt), (word)seconds());
#endif
		// rfid_ctrl is accessible outside and when this is bad, we're dead but pretentious, better check:
		if (in_rfid(msg, next) == 0 || in_rfid(msg, next) > 63) {
			app_diag_S ("rfid bad next %d", in_rfid(msg, next));
			ufree (msg);
			msg = NULL;
			finish;
		}
#if _RFID_DBG
			else
				app_diag_U ("rfid next in %d", in_rfid(msg, next));
#endif


		when (TRIG_RFID, TRIG);
		delay (in_rfid(msg, next) << 10, ITER);
		release;
}
#undef _RFID_DBG

