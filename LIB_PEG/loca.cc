/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2015                    */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "commons.h"
#include "diag.h"
#include "loca.h"
#include "inout.h"

// let's do it just for one tag; we'll see how (if?) surveys can be combined with 'base' praxis
// likely, we'll do surveys with separate 'survey tags' and actual location queries (selected plevs)
// might fit in reports from 'regular tags'...
// when done properly, it won't be here and in a decent struct

loca_t	loca;

void loca_out (Boolean sendLoca) {
	char  * mp;

	if (sendLoca) {
		mp = get_mem (sizeof(msgLocaType), NO);
		if (mp == NULL) {
			app_diag_S ("Loca failed");
			goto Clear;
		}

		memset (mp, 0, sizeof(msgLocaType));
		in_header(mp, msg_type) = msg_loca;
		in_header(mp, rcv) = master_host;
		in_loca(mp, id) = loca.id;
		in_loca(mp, ref) = loca.ref;
		memcpy (in_loca(mp, vec), loca.vec, LOCAVEC_SIZ);
		talk (mp, sizeof(msgLocaType), TO_ALL); // will NOT go TO_NET on Master. see talk()		
		ufree (mp);
	}

Clear:
	loca.ts = 0;
	loca.id = 0;
	loca.ref = 0;
	memset (loca.vec, 0, LOCAVEC_SIZ);
}

