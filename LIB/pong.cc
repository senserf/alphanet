/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/

#include "commons.h"
#include "vartypes.h"
#include "inout.h"
#include "diag.h"
#include "net.h"
#include "pong.h"
#include "alrms.h"

// retry delay, try  nr, rx span, spare bits, power levels
pongParamsType  pong_params = { 3, 4, 2, 0, 0x7777};

char pong_frame [sizeof(msgPongType) + sizeof(pongPloadType)];

// a quick kludge
fsm ping {
	char * msg;

	state LOAD:
		if ((msg = get_mem(sizeof(msgPingType), NO)) == NULL)
			finish;
		
		memset (msg, 0, sizeof(msgPingType));
		in_header(msg, msg_type) = msg_ping;
		in_header(msg, rcv) = 0;
		in_header(msg, hco) = 1;
		in_header(msg, prox) = 1;
		in_ping(msg, ref) = (word)seconds();
		in_ping(msg, slot) = 0;
	
	state ITER:
		// word pl;		
		// pl = in_ping(msg, slot) / 4;
		// net_opt (PHYSOPT_SETPOWER, &pl);
		set_pxoptions (PING_LBT_SETTING, in_ping(msg, slot) / 4, 0);
		// PG: talk doesn't wait for memory; as we have an FSM to send
		// these pings, and they form a burst, perhaps it would make
		// sense to wait for memory, if not available immediately,
		// because the burst is not completely unlikely to momentarily
		// deplete it (esepecially with LBT on), so I would replace the
		// command below with:
		//
		// net_tx (ITER, msg, sizeof (msgPingType), 0);
		//
		// Note the even though bursts are sent with LBT off, a packet
		// in front may be blocking the queue
		//
		talk (msg, sizeof(msgPingType), TO_NET);
		if (++in_ping(msg, slot) > 31) {
			ufree (msg);
			finish;
		}
		delay (PING_SPACING, ITER);
		release;
}
		
#define _PONG_DBG 0

fsm pong {

	word tr;  // = 0; many times forgot that tr is NOT on stack

	state LOAD:
	
		if (load_pframe () && !running (ping)) {
			runfsm ping;
			joinall (ping, LOADED);
			release;
		}
			
	state LOADED:
		tr = 0;
		
#if _PONG_DBG
		app_diag_U ("PONG BEG (%u) retry %u.%u rx %u pl %x", (word)seconds(),
			pong_params.retry_del, pong_params.retry_nr,
			pong_params.rx_span, pong_params.pow_levels);
#endif

	state ITER:

		word   level;

		level = tr < 3 ?
			((pong_params.pow_levels >> (tr << 2)) & 0x000f) :
			pong_params.pow_levels 	>> 12;

		if (level > 0) {
			upd_pframe (level, tr);
			// net_opt (PHYSOPT_SETPOWER, &level);
			set_pxoptions (0, level, 0);
			net_opt (PHYSOPT_RXON, NULL);

			talk (pong_frame, sizeof(msgPongType) +
				sizeof(pongPloadType),
				TO_ALL /* just to illustrate TO_NET*/);
				
#if _PONG_DBG
			app_diag_U ("PONG out (%u) l %u tr %u", (word)seconds(),
				level, tr);
#endif
			when (TRIG_ACK, ACKIN);
			delay (pong_params.rx_span << 10, ROFF);

		} else {
		
#if _PONG_DBG
			app_diag_U ("PONG skip level (%u) %u", 
				(word)seconds(), tr);
#endif
			delay (pong_params.retry_del << 10, PAUSE);
		}
		release;

	state ROFF:
		net_opt (PHYSOPT_RXOFF, NULL);

#if _PONG_DBG
		app_diag_U ("PONG rxoff (%u)", (word)seconds());
#endif
		delay ((pong_params.retry_del - pong_params.rx_span) <<  10,
			PAUSE);
		release;

	state PAUSE:
		if (++tr < pong_params.retry_nr)
			proceed ITER;

		trigger (TRIG_RONIN);
		proceed FIN;

	state ACKIN:
		trigger (TRIG_DORO);

	state FIN:
		net_opt (PHYSOPT_RXOFF, NULL);
		clr_alrm();
		
#if _PONG_DBG
		app_diag_U ("PONG ends (%u)", seconds());
#endif

		if (!fifek_empty (&pframe_stash)) {
			// for the record: once proceed ALWAYS ended up with failed 1st pong,
			// so I blamed back-to-back TXs. Can't see it now. Keeping this in case
			// symptoms reappear (i.e. cause is still active, elsewhere).
			// delay (50, LOAD);
			// release;
			proceed LOAD;
		}
		finish;
}
#undef _PONG_DBG

// the same for all boards (so far)
void init_pframe () {
	in_header(pong_frame, msg_type) = msg_pong;
	in_header(pong_frame, rcv) = 0;
	in_header(pong_frame, hco) = 1;
	in_header(pong_frame, prox) = 1;
	in_pong(pong_frame, pd.btyp) = BTYPE;
	in_pong(pong_frame, pd.len) = sizeof(pongPloadType);
	fifek_ini (&pframe_stash, 2); // 2-elem alarm stash
};

void upd_pframe (word pl, word tnr) {
	in_pong(pong_frame, pd).plev = pl;
	in_pong(pong_frame, pd).trynr = tnr;
}

// I truly dislike this idea of stashing alarms, even if I understand this may
// be needed in some praxes... That's why I want a solid pong_frame and
// dynamic stash, even if it seems weird and a bit wasteful.
// It is tempting to stash not thw whole frames, but info to load them;
// however it wouldn't be truly frame stashing and sooner or later we would
// have to maintain a separate struct.
fifek_t pframe_stash;

#if BTYPE == BTYPE_ALPHANET_CHRONOS || BTYPE == BTYPE_ALPHANET_CHRONOS_WHITE
#include "chro_tag.h"

Boolean load_pframe () {
	char * pf;

    in_pong(pong_frame, pd.dupeq) += 1;
	in_pong(pong_frame, pd.locat) = LOCA_SHORT;

    if (fifek_empty (&pframe_stash)) {
		in_pong(pong_frame, pd.alrm_id) = chronos.alrm_id;
		in_pong(pong_frame, pd.alrm_seq) = chronos.alrm_seq;
		in_pong(pong_frame, pd.fl2) = chronos.acc_mode;
		in_pongPload(pong_frame, volt) = chronos.volt;
		in_pongPload(pong_frame, move_ago) =
			(word)(seconds() - chronos.move_ts);
		in_pongPload(pong_frame, move_nr) = chronos.move_nr;
    } else {
        pf = fifek_pull (&pframe_stash);
		in_pong(pong_frame, pd.alrm_id) = in_pong(pf, pd.alrm_id);
        in_pong(pong_frame, pd.alrm_seq) = in_pong(pf, pd.alrm_seq);
        in_pong(pong_frame, pd.fl2) = in_pong(pf, pd.fl2);
        in_pongPload(pong_frame, volt) = in_pongPload(pf, volt);
        in_pongPload(pong_frame, move_ago) =
                (word)(seconds() - in_pongPload(pf, move_ago));
        in_pongPload(pong_frame, move_nr) = in_pongPload(pf, move_nr);
		ufree (pf);
    }
	return YES;
}

// we're overwriting alrms older than the last and this - it is trivial to
// increase the stash - 2nd param in call to fifek_ini().
void stash_pframe () {
	char * pf;

	if (chronos.alrm_id == 0)
		return; // only alrms count

	if ((pf = get_mem (sizeof(msgPongType) + sizeof(pongPloadType), 
			NO)) == NULL)
		return;

	in_pong(pf, pd.locat) = LOCA_SHORT;
	in_pong(pf, pd.alrm_id) = chronos.alrm_id;
	in_pong(pf, pd.alrm_seq) = chronos.alrm_seq;
	in_pong(pf, pd.fl2) = chronos.acc_mode;
	in_pongPload(pf, volt) = chronos.volt;
	in_pongPload(pf, move_ago) = chronos.move_ts;
	in_pongPload(pf, move_nr) = chronos.move_nr;
	fifek_push(&pframe_stash, pf);
}

#endif

#if BTYPE == BTYPE_WARSAW
#include "war_tag.h"

Boolean load_pframe () {
	char * pf;

    in_pong(pong_frame, pd.dupeq) += 1;
    in_pong(pong_frame, pd.fl2)++; // vy not?
	in_pong(pong_frame, pd.locat) = LOCA_SHORT;
    in_pongPload(pong_frame, steady_shit)++;

    if (fifek_empty (&pframe_stash)) {
        in_pong(pong_frame, pd.alrm_id) = warsaw.alrm_id;
        in_pong(pong_frame, pd.alrm_seq) = warsaw.alrm_seq;
        in_pongPload(pong_frame, volt) = warsaw.volt;
		in_pongPload(pong_frame, random_shit) = warsaw.random_shit;
    } else {
		pf = fifek_pull (&pframe_stash);
		in_pong(pong_frame, pd.alrm_id) = in_pong(pf, pd.alrm_id);
		in_pong(pong_frame, pd.alrm_seq) = in_pong(pf, pd.alrm_seq);
		in_pongPload(pong_frame, volt) = in_pongPload(pf, volt);
		in_pongPload(pong_frame, random_shit) = in_pongPload(pf, random_shit);
		ufree (pf);
    }
	return YES;
}

// we're overwriting alrms older than the last and this - it is trivial to
// increase the stash - 2nd param in call to fifek_ini().
void stash_pframe () {
	char * pf;

	if (warsaw.alrm_id == 0)
		return; // only alrms count

	if ((pf = get_mem (sizeof(msgPongType) + sizeof(pongPloadType),
			NO)) == NULL)
		return;

	in_pong(pf, pd.locat) = LOCA_SHORT;
	in_pong(pf, pd.alrm_id) = warsaw.alrm_id;
	in_pong(pf, pd.alrm_seq) = warsaw.alrm_seq;
	in_pongPload(pf, volt) = warsaw.volt;
	in_pongPload(pf, random_shit) = warsaw.random_shit;
	fifek_push(&pframe_stash, pf);
}

#endif

#if BTYPE == BTYPE_AT_BUT6
#include "ap319_tag.h"

Boolean load_pframe () {
	char * pf;
	
    in_pong(pong_frame, pd.dupeq) += 1;
    in_pong(pong_frame, pd.fl2)++; // vy not?

    if (fifek_empty (&pframe_stash)) {
		ap319_rsw ();
        in_pong(pong_frame, pd.alrm_id) = ap319.alrm_id;
        in_pong(pong_frame, pd.alrm_seq) = ap319.alrm_seq;
        in_pongPload(pong_frame, volt) = ap319.volt;
        in_pongPload(pong_frame, dial) = ap319.dial;
		in_pongPload(pong_frame, glob) = 
			!ap319.alrm_id || (ap319.gmap & (1 << (ap319.alrm_id -1))) ? 1 : 0;
    } else {
		pf = fifek_pull (&pframe_stash);
		in_pong(pong_frame, pd.alrm_id) = in_pong(pf, pd.alrm_id);
		in_pong(pong_frame, pd.alrm_seq) = in_pong(pf, pd.alrm_seq);
		in_pongPload(pong_frame, volt) = in_pongPload(pf, volt);
		in_pongPload(pong_frame, dial) = in_pongPload(pf, dial);
		in_pongPload(pong_frame, glob) = in_pongPload(pf, glob);
		ufree (pf);
    }
	
	in_pong(pong_frame, pd.locat) = in_pongPload(pong_frame, glob) ? LOCA_FULL : LOCA_NONE;
	return in_pong(pong_frame, pd.locat) == LOCA_NONE ? NO : YES;
}

// we're overwriting alrms older than the last and this - it is trivial to
// increase the stash - 2nd param in call to fifek_ini().
void stash_pframe () {
	char * pf;

	if (ap319.alrm_id == 0)
		return; // only alrms count

	if ((pf = get_mem (sizeof(msgPongType) + sizeof(pongPloadType),
			NO)) == NULL)
		return;

	in_pong(pf, pd.alrm_id) = ap319.alrm_id;
	in_pong(pf, pd.alrm_seq) = ap319.alrm_seq;
	in_pongPload(pf, volt) = ap319.volt;
	in_pongPload(pf, dial) = ap319.dial;
	in_pongPload(pf, glob) = (ap319.gmap >> (ap319.alrm_id -1)) & 1;
	// not necessary in_pong(pf, pd.locat) = in_pongPload(pf, glob) ? LOCA_FULL : LOCA_NONE;
	fifek_push(&pframe_stash, pf);
}

#endif

#if BTYPE == BTYPE_AT_BUT1
#include "ap320_tag.h"

Boolean load_pframe () {
	char * pf;
	
    in_pong(pong_frame, pd.dupeq) += 1;
    in_pong(pong_frame, pd.fl2)++; // vy not?
	in_pong(pong_frame, pd.locat) = LOCA_FULL;

    if (fifek_empty (&pframe_stash)) {
        in_pong(pong_frame, pd.alrm_id) = ap320.alrm_id;
        in_pong(pong_frame, pd.alrm_seq) = ap320.alrm_seq;
        in_pongPload(pong_frame, volt) = ap320.volt;
    } else {
		pf = fifek_pull (&pframe_stash);
		in_pong(pong_frame, pd.alrm_id) = in_pong(pf, pd.alrm_id);
		in_pong(pong_frame, pd.alrm_seq) = in_pong(pf, pd.alrm_seq);
		in_pongPload(pong_frame, volt) = in_pongPload(pf, volt);
		ufree (pf);
    }
	return YES;
}

// we're overwriting alrms older than the last and this - it is trivial to
// increase the stash - 2nd param in call to fifek_ini().
void stash_pframe () {
	char * pf;

	if (ap320.alrm_id == 0)
		return; // only alrms count

	if ((pf = get_mem (sizeof(msgPongType) + sizeof(pongPloadType),
			NO)) == NULL)
		return;

	in_pong(pf, pd.locat) = LOCA_FULL;
	in_pong(pf, pd.alrm_id) = ap320.alrm_id;
	in_pong(pf, pd.alrm_seq) = ap320.alrm_seq;
	in_pongPload(pf, volt) = ap320.volt;
	fifek_push(&pframe_stash, pf);
}

#endif

#if BTYPE == BTYPE_AT_LOOP
#include "ap331_tag.h"

Boolean load_pframe () {
	char * pf;
	
    in_pong(pong_frame, pd.dupeq) += 1;
    in_pong(pong_frame, pd.fl2)++; // vy not?

    if (fifek_empty (&pframe_stash)) {
        in_pong(pong_frame, pd.alrm_id) = ap331.alrm_id;
        in_pong(pong_frame, pd.alrm_seq) = ap331.alrm_seq;
        in_pongPload(pong_frame, volt) = ap331.volt;
        memcpy (in_pongPload(pong_frame, loop), &(ap331.loop), AS3932_NBYTES);
    } else {
		pf = fifek_pull (&pframe_stash);
		in_pong(pong_frame, pd.alrm_id) = in_pong(pf, pd.alrm_id);
		in_pong(pong_frame, pd.alrm_seq) = in_pong(pf, pd.alrm_seq);
		in_pongPload(pong_frame, volt) = in_pongPload(pf, volt);
        memcpy (in_pongPload(pong_frame, loop), in_pongPload(pf, loop), AS3932_NBYTES);
		ufree (pf);
    }
	// That below is doable if later on we twist things to shorten the ap331 'frame' for button alarms.
	// Nobody cares, so let's make it easy: only short vectors for ap331
	// in_pong(pong_frame, pd.locat) = (in_pong(pong_frame, pd.alrm_id) ==  LOOP_ALRM_ID ? LOCA_SHORT : LOCA_FULL);
	in_pong(pong_frame, pd.locat) = LOCA_SHORT;
	
	return YES;
}

// we're overwriting alrms older than the last and this - it is trivial to
// increase the stash - 2nd param in call to fifek_ini().
void stash_pframe () {
	char * pf;

	if (ap331.alrm_id == 0)
		return; // only alrms count

	if ((pf = get_mem (sizeof(msgPongType) + sizeof(pongPloadType),
			NO)) == NULL)
		return;

	in_pong(pf, pd.alrm_id) = ap331.alrm_id;
	in_pong(pf, pd.alrm_seq) = ap331.alrm_seq;
	in_pongPload(pf, volt) = ap331.volt;
	memcpy (in_pongPload(pf, loop), &(ap331.loop), AS3932_NBYTES);
	fifek_push(&pframe_stash, pf);
}

#endif

