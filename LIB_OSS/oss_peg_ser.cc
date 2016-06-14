/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2014                    */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "commons.h"
#include "vartypes.h"
#include "form.h"
#include "ser.h"
#include "diag.h"
#include "tag_mgr.h"
#include "tarp.h"
#include "inout.h"

#include "ser_insert.cc"

static trueconst char welcome_str[] = "SPOT(1.0) Peg:\r\n"
  "\t? - status quo\r\n\tm - become master / force mbeacon\r\n"
  "\tf id text - forward text to id\r\n"
  "\th - help\r\nq - quit\r\n";

static trueconst char stats_str[] = "Node %u uptime %u.%u:%u:%u "
	"master %u mem %u %u\r\n";

static char * stats () {
	char * b = NULL;
	word mmin, mem;
	mem = memfree(0, &mmin);
	b = form (NULL, stats_str, local_host, (word)(seconds() / 86400),
		(word)((seconds() % 86400) / 3600), 
		(word)((seconds() % 3600) / 60),
		(word)(seconds() % 60), master_host,
		mem, mmin);
	return b;
}
///////////////// oss in ////////////////

/* no need for that here, but we could build appropriate structs here and
   call process_incoming() as fsm hear does for RF in.
*/

fsm mbeacon;
fsm looper;
fsm cmd_in {
	char * obuf;
	char   ibuf [UART_INPUT_BUFFER_LENGTH];

	state START:
		obuf = (char *)welcome_str;
		proceed UIOUT;

	state INI:
		ibuf[0] = '\0';

	state LISN: // LISTEN keyword??
		char * b;
		word w, l;

		ser_in (LISN, ibuf, UART_INPUT_BUFFER_LENGTH);
		if (strlen(ibuf) == 0 // CR on empty line would do it
				|| ibuf[0] == ' ') // ignore lines starting blank
			proceed LISN;

		switch (ibuf[0]) {
			case 'h':
				obuf = (char *)welcome_str;
				proceed UIOUT;

			case 'q': reset();

			case 'm':
				if (local_host == master_host) {
					if (running (mbeacon)) {
					    trigger (TRIG_MBEAC);
					    obuf = (char *)"Sent mbeacon\r\n";
					} else {
					    runfsm mbeacon;
					    obuf = (char *)"?tarted mbeacon\r\n";
					}
				} else {
					master_host = local_host;
					tarp_ctrl.param = 0xB0;
					reset_tags();
					tagList.block = YES;
					killall (looper);
					runfsm mbeacon;
					obuf = (char *)"I am M\r\n";
				}
				proceed UIOUT;

			case 'f':
				w = local_host;
				scan (ibuf +1, "%u", &w);
				if (w != local_host) {
				    l = strlen(ibuf) + sizeof(msgFwdType);

				    if ((b = get_mem (l, NO)) != NULL) {
						memset (b, 0, l);
						in_header(b, msg_type) = msg_fwd;
						in_header(b, rcv) = w;
						in_fwd(b, opref) = (byte)seconds();
						// terminating NULL should fit in:
						strcpy (b+sizeof(msgFwdType), ibuf+1);
						talk (b, l, TO_ALL);
						ufree (b);
						proceed INI;
				    }
				}
				// fall through

			default: // ?
				obuf = stats();
				_oss_out (obuf);
				proceed INI;
		}

	state UIOUT:
		ser_out (UIOUT, obuf);
		proceed INI;
}
///////////////////////////////

#define _ppp	(p + sizeof(pongDataType))
static char * board_out (char * p) {
	char * b;

	switch (((pongDataType *)p)->btyp) {
		case BTYPE_CHRONOS:
		case BTYPE_CHRONOS_WHITE:
			b = form (NULL, "V %u move %u.%u",
				((pongPloadType0 *)_ppp)->volt,
				((pongPloadType0 *)_ppp)->move_ago,
				((pongPloadType0 *)_ppp)->move_nr);
			break;
		case BTYPE_AT_BASE:
			 b = form (NULL, "V %u",
				((pongPloadType2 *)_ppp)->volt);
			break;
		case BTYPE_AT_BUT6:
			b = form (NULL, "V %u dial %u.%u",
				((pongPloadType3 *)_ppp)->volt,
				((pongPloadType3 *)_ppp)->dial,
				((pongPloadType3 *)_ppp)->glob);
			break;
		case BTYPE_AT_BUT1:
			b = form (NULL, "V %u",
				((pongPloadType4 *)_ppp)->volt);
			break;
		case BTYPE_WARSAW:
			b = form (NULL, "V %u s %u.%u",
				((pongPloadType5 *)_ppp)->volt,
				((pongPloadType5 *)_ppp)->random_shit,
				((pongPloadType5 *)_ppp)->steady_shit);
			break;
		case BTYPE_AT_LOOP:
			b = form (NULL, "V %u",
				((pongPloadType6 *)_ppp)->volt);
			break;
		default:
			app_diag_W ("btyp %u", ((pongDataType *)p)->btyp);
			b = NULL;
	}
	return b;
}
#undef _ppp

// this is the main out on the oss i/f, I don't know at all if this makes
// more sense than no generic i/f at all
// NO parameter sanitization
void oss_tx (char * b, word siz) {
	char *bu, *bt;
	Boolean incoming;

	// allocated, freed after output
	if (siz == MAX_WORD) {
		_oss_out (b);
		return;
	}
	if (siz == 0) { // copied for direct output
		bu = form (NULL, "%s", b);
		if (bu)
			_oss_out (bu);
		return;
	}

	// b[0] doesn't have to be msg_type; if not, must be off the enum
	switch (b[0]) {
	    case msg_report:
		incoming = in_header(b, rcv) == local_host ? YES : NO;
		bt = board_out (b + sizeof(msgReportType)); 
		bu = form (NULL, "%s Report #%u tag %u->%u rss %u ago %u "
		    "btyp %u plev %u alrm %u.%u fl %u try %u: %s\r\n",
			incoming ?  "IN" : "OUT",
			in_report(b, ref), in_report(b, tagid),
			in_header(b, snd),
			in_report(b, rssi), in_report(b, ago),
			((pongDataType *)(b + sizeof(msgReportType)))->btyp,
			((pongDataType *)(b + sizeof(msgReportType)))->plev,
			((pongDataType *)(b + sizeof(msgReportType)))->alrm_id,
			((pongDataType *)(b + sizeof(msgReportType)))->alrm_seq,
			((pongDataType *)(b + sizeof(msgReportType)))->fl2,
			((pongDataType *)(b + sizeof(msgReportType)))->trynr,
			bt ? bt : "?");
		ufree (bt);
		_oss_out (bu);
		break;

	    case msg_reportAck:
		bu = form (NULL, "RepAck #%u tag %u (%u+%u)\r\n",
			in_reportAck(b, ref), in_reportAck(b, tagid),
			tagList.alrms, tagList.evnts);
		_oss_out (bu);
		break;

	    case msg_fwd:
		incoming = in_header(b, snd) != 0 &&
			in_header(b, snd) != local_host;
		bu = form (NULL, "%s FWD #%u %u [%s]\r\n",
		incoming ? "IN" : "OUT", in_fwd(b, opref),
		incoming ?  in_header(b, snd) : in_header(b, rcv),
		incoming ?  b + sizeof(msgFwdType) : ".");
		_oss_out (bu);
		break;

	    case msg_fwdAck:
		bu = form (NULL, "FwdAck #%u fr %u\r\n",
			in_fwd(b, opref), in_header(b, snd));
		_oss_out (bu);
		break;

	    default:
		app_diag_U ("dupa unfinished?");
	}
}

