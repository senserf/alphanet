/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2014                    */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "commons.h"
#include "diag.h"
#include "inout.h"
#include "tag_mgr.h"
#include "form.h"

// let's do it just for one tag; we'll see how (if?) surveys can be combined with 'base' praxis
// likely, we'll do surveys with separate 'survey tags' and actual location queries (selected plevs)
// might fit in reports from 'regular tags'...
// when done properly, it won't be here and in a decent struct
static lword loca_ts;
static word loca_id;
static word loca_ref;
static byte loca_vec[32];

static void loca_out () {
	char  * mp;
	mp = get_mem (sizeof(msgLocaType), NO);
	if (mp == NULL) {
		app_diag_S ("Loca failed");
		return;
	}

	memset (mp, 0, sizeof(msgLocaType));
	in_header(mp, msg_type) = msg_loca;
	in_header(mp, rcv) = master_host;
	in_loca(mp, id) = loca_id;
	in_loca(mp, ref) = loca_ref;
	memcpy (in_loca(mp, vec), loca_vec, 32);
	talk (mp, sizeof(msgLocaType), TO_ALL); // will NOT go TO_NET on Master. see talk()
	
	ufree (mp);
	loca_ts = 0;
	loca_id = 0;
	loca_ref = 0;
	memset (loca_vec, 0, 32);
}

void msg_master_in (char * buf) {

	if (master_host != local_host)
		return;

	trigger (TRIG_MBEAC);
	app_diag_S ("master contra %u (%u)", in_header(buf, snd), (word)seconds());
}

void msg_ping_in (char * buf, word rssi) {
Clear:
	if (loca_id == 0) { // first elem
		loca_ts = seconds();
		loca_id = in_header(buf, snd);
		loca_ref = in_ping(buf, ref);
		loca_vec [in_ping(buf, slot)] = (word)rssi;
		return;
	}
	if (seconds() - loca_ts > 2) { // 3 may be 2 and a bit
		loca_out();
		goto Clear;
	}
	if (loca_id == in_header(buf, snd))
		loca_vec [in_ping(buf, slot)] = (word)rssi;
	// else another tag cut in - leave it alone
}

void msg_pong_in (char * buf, word rssi) {

	// 1s for hco and prox
	msgPongAckType  pong_ack = {{msg_pongAck,0,0,0,0,1,1,0}};

	// we've made ANY pong_in a loca watchdog
	if (loca_id == in_header(buf, snd) || loca_id != 0 && seconds() - loca_ts > 2)
		loca_out();
	
	if (needs_ack (in_header(buf, snd), buf + sizeof(headerType), rssi)) {
		pong_ack.header.rcv = in_header(buf, snd);
		pong_ack.dupeq = in_pong(buf, pd).dupeq;
		talk ((char *)&pong_ack, sizeof(msgPongAckType), TO_NET);
	} else 
		in_pong(buf, pd).noack = 1;

	app_diag_D ("Pong %u", in_header(buf, snd));
	ins_tag (buf, rssi);
}

void msg_fwd_in (char * buf, word siz) {
	msgFwdAckType ack;
	if (in_fwd(buf, opref) & 0x80) {
		memset (&ack, 0, sizeof(msgFwdAckType));
		ack.header.msg_type = msg_fwdAck;
		ack.header.rcv = in_header(buf, snd);
		ack.optyp = in_fwd(buf, optyp);
		ack.opref = in_fwd(buf, opref);
		talk ((char *)&ack, sizeof(msgFwdAckType), TO_NET);
	}
    talk (buf, siz, TO_OSS);
}

void msg_report_in (char * buf, word siz) {
	msgReportAckType ack = {{msg_reportAck,0,0,0,0,0,0,0}};

	// eliminate unnecessary RACKs for heartbeat 'alarms' (in 1.5, not in 1.0)
	if (((pongDataType *)(buf + sizeof(msgReportType)))->alrm_id == 0) { // heartbeat
		app_diag_D ("No RACK for alrm0 %u #%u", in_header(buf, snd), in_report(buf, ref));
	} else {
		ack.header.rcv = in_header(buf, snd);
		ack.ref = in_report(buf, ref);
		ack.tagid = in_report(buf, tagid);
		talk ((char *)&ack, sizeof(msgReportAckType), TO_NET);
	}
	talk (buf, siz, TO_OSS);
}

void msg_reportAck_in (char * buf) {
	char * b;

    if (del_tag (in_reportAck(buf, tagid), in_reportAck(buf, ref), 
		0, YES) > 1) {
	b = form (NULL, "Stale RAck %u #%u\r\n", 
		in_reportAck(buf, tagid), in_reportAck(buf, ref));

	if (b)
		talk (b, MAX_WORD, TO_OSS);

    } else {

	talk (buf, 77, TO_OSS); // note we get away with the lucky size

    }
}

