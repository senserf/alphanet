/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/

/* Reusable module looping for heartbeats (tagList audit in this incarnation).
   Heartbeat == 0 calls a single pass through tagList with calls to report_tag.
*/

#include "sysio.h"
#include "commons.h"
#include "diag.h"
#include "tag_mgr.h"
#include "hold.h"
#include "tarp.h"

word	heartbeat = 15; // seconds

#define _LOO_DBG	0

/* we want to spread re-reporting of multiple tags, but we can't hang on
   a given list element, as it can be deallocated any moment. So, we have
   an audit marker (marka) and quickly pick the next element from the head
*/
static char * nextel () {
	char * ptr = tagList.nel;
 
	while (ptr && (in_tdt(ptr, marka) == tagList.marka ||
		(word)(seconds() - in_tdt(ptr, refTime)) < heartbeat)) {
#if _LOO_DBG
		app_diag_U ("LOO: skipped %u#%u", in_tdt(ptr, tagid),
				in_tdt(ptr, refTime));
#endif
		ptr = in_tdt(ptr, nel);
	}

	return ptr;
}

#ifdef __SMURPH__
#include "form.h"
static trueconst char stats_str[] = "Node %u uptime %u.%u:%u:%u "
	"master %u mem %u %u\r\n";
static void stats_ () {
	char * b = NULL;
	word mmin, mem;
	mem = memfree(0, &mmin);
	b = form (NULL, stats_str, local_host, (word)(seconds() / 86400),
		(word)((seconds() % 86400) / 3600), 
		(word)((seconds() % 3600) / 60),
		(word)(seconds() % 60), master_host,
		mem, mmin);
		
	if (b) {
		app_diag_U (b);
		ufree (b);
	}
}
#endif

fsm looper {
    lword htime;

    state BEG:

#ifdef __SMURPH__
		stats_();
#endif

#if _LOO_DBG
		app_diag_U ("LOO: BEG (%u) %u+%u", (word)seconds(),
			tagList.alrms, tagList.evnts);
#endif
		htime = seconds() + heartbeat;
		tagList.marka++;

	state TAG:
		char * tp = nextel();
		if (tp) {
#if _LOO_DBG
			app_diag_U ("LOO: in %u#%u", in_tdt(tp, tagid),
				in_tdt(tp, refTime));
#endif
			in_tdt(tp, marka) = tagList.marka;
			
			// NO RETRIES as of Dec. 2015
			app_diag_S ("NO RETRIES??");
			
			(void) report_tag (tp);
			delay (1024, TAG);
			release;
		}

		if (heartbeat == 0) {
			app_diag_W ("looper exits");
			finish;
		}
#if _LOO_DBG
		app_diag_U ("LOO: hold %u", (word)(htime - seconds()));
#endif
    state HOLD:
		hold (HOLD, htime);
		proceed BEG;
}

#undef _LOO_DBG
