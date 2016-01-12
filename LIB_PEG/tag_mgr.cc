/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014        			*/
/* All rights reserved.							*/
/* ==================================================================== */

#include "commons.h"
#include "diag.h"
#include "inout.h"
#include "vartypes.h"
#include "tag_mgr.h"
#include "loca.h"

/**************************************************************************
tagList is a simple list of structs with Master-unconfirmed tags. New alarms
from an already present tag overwrite (del, ins) the entry. Events don't
overwrite alarms. Acks from Master remove (del) corresponding entries.
***************************************************************************/
tagListType tagList;
Boolean learn_mod = 0;

// we've got rid of retries... it sucks, all is incoherent, rewrite? chle chle
// #define	_TMGR_MAX_RELIABLE	12
#define	_TMGR_MAX_RELIABLE	12

#define _TMGR_DBG	0
void reset_tags () {

	char * mel = tagList.nel;

	while (mel) {
		tagList.nel = in_tdt(mel, nel);
		ufree (mel);
		mel = tagList.nel;
	}
	memset (&tagList, 0, sizeof(tagListType));
}

/***********************************************
0 - deleted event
1 - deleted alarm
2 - wrong ref
3 - couldn't find tag
4 - found alarm, aborted (unforced) deletion
5 - duplicate (aborted deletion)
************************************************/
word del_tag (word id, word ref, word dupeq, Boolean force) {


	char * pl = tagList.nel;
	char * p = NULL;
	word ret;

	while (pl) {
		if (in_tdt(pl, tagid) == id) {

			if (ref == 0 || in_tdt(pl, refTime) == ref) {
				if (ref == 0 && dupeq == in_pdt(pl, dupeq))
					return 5;
				// del pl if allowed
				if (in_pdt(pl, alrm_id) == 0) {
					--tagList.evnts;
					ret = 0;
				} else {
					if (!force)
						return 4;

					--tagList.alrms;
					ret = 1;
				}
				if (p)
					in_tdt(p, nel) = in_tdt(pl, nel);
				else
					tagList.nel = in_tdt(pl, nel);

#if _TMGR_DBG
				app_diag_U ("TMGR(%u): del %u %u %u %u", (word)seconds(),
					in_tdt(pl, tagid), in_tdt(pl, refTime), tagList.alrms, tagList.evnts);
#endif

				ufree (pl);
			} else {
#if _TMGR_DBG
				app_diag_U ("TMGR(%u): del badref %u %u %u %u", (word)seconds(),
					ref, in_tdt(pl, refTime), tagList.alrms, tagList.evnts);
#endif
				return 2;
			}
			return ret;
		}
		p = pl; // yeah, I had it with **char - unreadable in 2 weeks
		pl = in_tdt(pl, nel);
	}
	return 3;
}

Boolean is_global ( char * b) { // b point at pdt (and optional board-specific ppt)

	if (((pongDataType *)b)->btyp != BTYPE_AT_BUT6)
		return YES;
		
	return ((pongPloadType3 *)(b+sizeof(pongDataType)))->glob;
}

#define LEARN_THOLD	150
Boolean needs_ack ( word id, char * b, word rss) { // b point at pdt (and optional board-specific ppt)

	if (is_global (b) || learn_mod && rss >= LEARN_THOLD)
		return YES;

	return (in_treg (id, (byte)((pongDataType *)b)->alrm_id));
//	return (~(in_treg (id, (byte)((pongDataType *)b)->alrm_id))); // dupa back to negative default
}
#undef LEARN_THOLD

/*************
returns 1 if report goes global, 0 otherwise
With location data (optionally) in, there is more:
- refTime is updated on these that go to the master
- locat flag is set to remove master's reportAcks
All this should be reworked if we get back to report retries
*************/
Boolean report_tag (char * td) {
	char  * mp;
	Boolean globa = is_global (td + sizeof(tagDataType));
	word	siz = sizeof(msgReportType) + sizeof(pongDataType) +
			in_pdt(td, len);

	in_pdt(td, locat) = NO;
	if (loca.id == in_tdt(td, tagid)) {
		if (globa) {
				siz += LOCAVEC_SIZ;
				in_pdt(td, locat) = YES;
			} else {
				loca_out (YES);  // that is for local alrms (distinct loca msgs)
			}
	}

	mp = get_mem (siz, NO); // continue if no mem

	if (mp == NULL) {
		app_diag_S ("Rep failed");
		return 0;
	}

	memset (mp, 0, siz);
	in_header(mp,  msg_type) = msg_report;
	in_header(mp,  rcv) = master_host;
	in_report(mp, ref) = in_pdt(td, locat) ? loca.ref : in_tdt(td, refTime);
	in_report(mp, tagid) = in_tdt(td, tagid);
	in_report(mp, rssi) = in_tdt(td, rssi);
	in_report(mp, ago) = (word)(seconds() - in_tdt(td, refTime)) > 255 ?
		255 : (byte)(seconds() - in_tdt(td, refTime));

	memcpy (mp + sizeof(msgReportType), td + sizeof(tagDataType),
		siz - sizeof(msgReportType));
	
	// if (globa && loca.id == in_tdt(td, tagid)) { equivalent... I hope:
	if (in_pdt(td, locat)) {
		memcpy (mp + siz -LOCAVEC_SIZ, loca.vec, LOCAVEC_SIZ);
		loca_out (NO);
	}
	// globa = is_global (mp + sizeof(msgReportType));
	talk (mp, siz, globa ? TO_ALL : TO_OSS); // will NOT go TO_NET on Master. see talk()

#if _TMGR_DBG
	app_diag_U ("TMGR(%u): rep %u %u %u", (word)seconds(),
		in_report(mp, tagid), in_report(mp, ago), globa);
#endif

	// remove old tags
	if (in_report(mp, ago) == 255) {
		siz = del_tag (in_report(mp, tagid), in_report(mp, ref), 0, YES);
		app_diag_W ("Del(%u) aged tag %u", siz, in_report(mp, tagid));
	}
	
	ufree (mp);
	return globa;
}

// I let it do empty quick things if local_host == master_host
void ins_tag (char * buf, word rssi) { // it is msg_pong in buf

	Boolean force = in_pong(buf, pd).alrm_id == 0 ? NO : YES;
	Boolean globa;
	word ret = del_tag (in_header(buf, snd), 0, in_pong(buf, pd).dupeq, 
			force);
	char * ptr;

	if (ret == 4) {
#if _TMGR_DBG
		app_diag_U ("TMGR Ins set alrm %u", in_header(buf, snd));
#endif
		return;
	}
	if (ret == 5) {
#if _TMGR_DBG
		app_diag_U ("TMGR dupeq %u", in_header(buf, snd));
#endif
		return;
	}

	ptr = get_mem (sizeof(tagDataType) + sizeof(pongDataType) +
		in_pong(buf, pd).len, YES); // failure will reset

	memcpy (ptr + sizeof(tagDataType), buf + sizeof(headerType),
		sizeof(pongDataType) + in_pong(buf, pd).len);

	in_tdt(ptr, refTime) = (word)seconds();
	in_tdt(ptr, tagid) = in_header(buf, snd);
	in_tdt(ptr, rssi) = rssi;
	in_tdt(ptr, marka) = tagList.marka;
	in_tdt(ptr, nel) = tagList.nel;

	globa = report_tag (ptr);

// this is a kludge to get rid of report retries (and rely on TARP_RTR)
// highly debatable, but may help with delivering location data from more pegs
// NO RETRIES
#if 0
	if (local_host == master_host) { // no insertions on Master
		globa = NO;
	} else {
	
		if (!globa || in_pdt(ptr, alrm_id) == 0 ||
				tagList.alrms + tagList.evnts >= _TMGR_MAX_RELIABLE) {
			
#if _TMGR_DBG
			app_diag_U ("TMGR(%u): ins skip %u %u %u %u", (word)seconds(),
				in_tdt(ptr, tagid), globa, in_pdt(ptr, alrm_id),
				tagList.alrms + tagList.evnts);
#endif

			globa = NO;
		} else {
			globa = YES;
		}
	} // by now globa means also 'insert'
	
	if (globa) {
		tagList.nel = ptr;

		if (force)
			tagList.alrms++;
		else
			tagList.evnts++;
#if _TMGR_DBG
		app_diag_U ("TMGR(%u): ins %u %u %u", (word)seconds(),
			in_tdt(ptr, tagid), tagList.alrms, tagList.evnts);
#endif
	} else 				// ... and free the element
#endif
// end of NO RETRIES

		ufree (ptr);
}

/******************TREG******************
  This part is about tag registration or rather discrimination between events that should be acked
  (global and registered local events) or unacked. The logic is rather twisted; I believe more appropriate
  would be to make tags address alarms to specific pegs. In any case, let's do it for Alphanet 1.0 without
  worrying too much about optimization; this alarm addressing will likely be implemented instead, as it
  has nice side effects, e.g. only genuinely interested pegs give a shit.
  
  Keeping it private here anyway, for the unlikely case we will be optimizing it.
****************************************/
#define TREG_NUM	20
typedef struct tregStruct {
	word tid[TREG_NUM];
	byte bmask[TREG_NUM];
} treg_t;

treg_t my_tags;
void reset_treg () {
	memset (&my_tags, 0, sizeof (treg_t));
}

#if 0
// this was in a bit more decent version
word count_treg () {
	word i, c = 0;
	for (i = 0; i < TREG_NUM; i++) {
		if (my_tags.tid[i] != 0) c++;
	}
	return c;
}
#endif

#if 0
// let's wait for a better moment 
/***************************
returns likely unused stuff:
0 - full / overflow
1 - updated
2 - added
3 - deleted
4 - void request
***************************/
word upd_treg (word id, byte mask) {
	word i, j = TREG_NUM;
	if (id == 0) {
		i = count_treg();
		reset_treg();
		return (i > 0 ? 3 : 4);
	}
	
	for (i = 0; i < TREG_NUM; i++) {
		if (my_tags.tid[i] == id) {
			if (mask == 0) {
				my_tags.tid[i] = 0;
				return 3;
			}
			if (my_tags.bmask[i] == mask)
				return 4;
			my_tags.bmask[i] = mask;
			return 1;
		}
		if (j == TREG_NUM && my_tags.tid[i] == 0)
			j = i; // first empty
	}
	
	if (mask == 0)
		return 4;
		
	if (j == TREG_NUM)
		return 0;
	
	my_tags.tid[j] = id;
	my_tags.bmask[j] = mask;
	return 2;
}
#endif

void b2treg (word l, byte * b) {
	byte * ptr;
	word i, n;

	if ((n = slice_treg ((word)*b)) > l)
		n = l;
		
	// special case: illegal index deals with 'learning mode'
	if (n == 0) {
		i = learn_mod;
		if (*(b+3) == 0) { // learning on
			learn_mod = YES;
		} else {
			if (*(b+3) == 0xff) { // learning off
				learn_mod = NO;
			} // else do nothing - no error codes, shitty acks
		}
#if _TMGR_DBG
		app_diag_U ("TMGR(%u): b2treg.learning (%u->%u)", (word)seconds(), i, learn_mod);
#endif
		return;
	}

	ptr = b +1;
	i = *b;
	while (n--) {
		memcpy (&my_tags.tid[i], ptr, 2);
		my_tags.bmask[i++] = *(ptr +2);
		ptr += 3;
	}
#if _TMGR_DBG
	app_diag_U ("TMGR(%u): b2treg (%u,%u)", (word)seconds(), l, (word)*b);
#endif

}

// we don't want to mess with oss allocation here, caller must do it and put the index in *b
void treg2b (byte *b) {
	word i = *b;
	byte * ptr = b +1;
	
	while (i < TREG_NUM) {
		memcpy (ptr, &my_tags.tid[i], 2);
		*(ptr +2) = my_tags.bmask[i++];
		ptr += 3;
	}
#if _TMGR_DBG
	app_diag_U ("TMGR(%u): treg2b %u", (word)seconds(), (word)*b);
#endif
}

Boolean in_treg (word id, byte but) {
	word i;
	
	if (id == 0 || but == 0)
		return NO;

	for (i = 0; i < TREG_NUM; i++) {
		if (my_tags.tid[i] == id && (my_tags.bmask[i] & (1 << (but-1))))
			return YES;
	}
	return NO;
}

word slice_treg (word ind) {
	return (ind < TREG_NUM ? TREG_NUM - ind : 0);
}

#undef _TMGR_DBG
#undef _TMGR_MAX_RELIABLE

