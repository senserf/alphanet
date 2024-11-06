/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "commons.h"
#include "vartypes.h"
#include "form.h"
#include "ser.h"
#include "diag.h"
#include "tag_mgr.h"
#include "tarp.h"
#include "alrms.h"

#include "ser_insert.cc"

static trueconst char welcome_str[] = "SPOT(1.0) Tag:\r\n"
  "\t? - status quo\r\n\ta id - raise alarm(id)\r\n"
  "\th - help\r\nq - quit\r\n";

static trueconst char stats_str[] = "Node %u uptime %u.%u:%u:%u "
	"mem %u %u\r\n";

static char * stats () {
	char * b = NULL;
	word mmin, mem;
	mem = memfree(0, &mmin);
	b = form (NULL, stats_str, local_host, (word)(seconds() / 86400),
		(word)((seconds() % 86400) / 3600), 
		(word)((seconds() % 3600) / 60),
		(word)(seconds() % 60),
		mem, mmin);
	return b;
}
///////////////// oss in ////////////////

/* no need for that here, but we could build appropriate structs here and
   call process_incoming() as fsm hear does for RF in.
*/

fsm mbeacon;
fsm cmd_in {
        char * obuf;
        char   ibuf [UART_INPUT_BUFFER_LENGTH];

	state START:
		obuf = (char *)welcome_str;
		proceed UIOUT;

        state INI:
                ibuf[0] = '\0';

        state LISN: // LISTEN keyword??
		word w;

                ser_in (LISN, ibuf, UART_INPUT_BUFFER_LENGTH);
                if (strlen(ibuf) == 0 // CR on empty line would do it
                        || ibuf[0] == ' ') // ignore lines starting blank
                                proceed LISN;

		switch (ibuf[0]) {
			case 'h':
				obuf = (char *)welcome_str;
				proceed UIOUT;

			case 'q': reset();

			case 'a':
				w = 0;
				scan (ibuf +1, "%u", &w);
				if (w) {
					set_alrm (w);
					proceed INI;
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

// this is the main out on the oss i/f, I don't know at all if this makes
// more sense than no generic i/f at all
// NO parameter sanitization
void oss_tx (char * b, word siz) {
	char *bu;

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
	    case msg_pong:
			bu = form (NULL, "Pong(%u) at %u try %u.%u alrm %u.%u #%u\r\n",
				in_pong(b, pd).btyp, (word)seconds(),
				in_pong(b, pd).trynr, in_pong(b, pd).plev,
				in_pong(b, pd).alrm_id, in_pong(b, pd).alrm_seq,
				in_pong(b, pd).dupeq);
			_oss_out (bu);
			break;
	    case msg_pongAck:
			bu = form (NULL, "Ack #%u from %u\r\n",
				in_pongAck(b, dupeq), in_header(b, snd));
			_oss_out (bu);
			break;
	    default:
			app_diag_U ("dupa unfinished?");
	}
}

