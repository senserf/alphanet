/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014 - 2016                    */
/* All rights reserved.                                                 */
/* ==================================================================== */

/* Reusable module looping for heartbeats with voltage reading.
     Heartbeat == 0 calls a single pong.
     Proceed to VOLT instead of BEG will carry on with the existing htime.
     I don't think separate alarm pools are a good idea, hence TRIG_ALRM.
*/

#include "sysio.h"
#include "diag.h"
#include "vartypes.h"
#include "hold.h"
#include "sensors.h"
#include "pong.h"

#define _LOO_DBG	0
#define _HBEAT		3600
#define _HBEAT_MAX	3600

word	heartbeat = _HBEAT; // seconds

/* VOLTAGE is consistent across the boards, but I'd like to see how this
   works for common functionality on board-specific resources...
   I don't think VOLTAGE should be defined here, but it should be #defined
   somewhere... and likely not in variants.h. Let's see if we need some
   sensor-related blocks.
*/

#if BTYPE == BTYPE_CHRONOS
#include "chro_tag.h"
#define VOLTAGE &chronos.volt
#endif

#if BTYPE == BTYPE_WARSAW
#include "war_tag.h"
#define VOLTAGE &warsaw.volt
#endif

#if BTYPE == BTYPE_AT_BUT6
#include "ap319_tag.h"
#define VOLTAGE &ap319.volt
#endif

#if BTYPE == BTYPE_AT_BUT1
#include "ap320_tag.h"
#define VOLTAGE &ap320.volt
#endif

#if BTYPE == BTYPE_AT_LOOP
#include "ap331_tag.h"
#define VOLTAGE &ap331.volt
#endif

#ifndef VOLTAGE
#error where is VOLTAGE
#endif

#ifdef __SMURPH__
#include "form.h"
static trueconst char stats_str[] = "Node %u uptime %u.%u:%u:%u "
	"mem %u %u\r\n";
static void stats_ () {
	char * b = NULL;
	word mmin, mem;
	mem = memfree(0, &mmin);
	b = form (NULL, stats_str, local_host, (word)(seconds() / 86400),
		(word)((seconds() % 86400) / 3600), 
		(word)((seconds() % 3600) / 60),
		(word)(seconds() % 60),
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
		app_diag_U ("LOO: BEG (%u)", (word)seconds());
#endif
		htime = seconds() + heartbeat;

	state VOLT:
		read_sensor (VOLT, -1, VOLTAGE);

		if (!running (pong))
			runfsm pong;
		else
			stash_pframe ();

		if (heartbeat == 0) {
			app_diag_W ("looper exits");
			finish;
		}
#if _LOO_DBG
		app_diag_U ("LOO: hold %u", (word)(htime - seconds()));
#endif

	// this is temporary, I think: heartbeat should be global, settable from pong
	state HOLD:
		when (TRIG_ALRM, BEG); // it'll reset htime (doesn't have to)
		when (TRIG_RONIN, RONIN);
		when (TRIG_DORO, DORO);
		hold (HOLD, htime);
		proceed BEG;
		
	state RONIN:
		if (heartbeat < _HBEAT_MAX) {
			heartbeat += _HBEAT;
			htime += _HBEAT;
		}
		proceed HOLD;
		
	state DORO:
		if (heartbeat != _HBEAT) {
			heartbeat = _HBEAT;
			htime = seconds() + _HBEAT;
		}
		proceed HOLD;
		
}
#undef _LOO_DBG
#undef _HBEAT
#undef _HBEAT_MAX
