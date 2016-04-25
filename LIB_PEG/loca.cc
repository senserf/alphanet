/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2016                    */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "commons.h"
#include "diag.h"
#include "loca.h"
#include "inout.h"

loca_t	locarr[LOCA_SNUM];

void loca_out (word ind, Boolean sendLoca) {
	char  * mp;

	// it is more convenient here as there is quite a few callers
	if (locarr[ind].id == 0)
		return;

	if (sendLoca) {
		mp = get_mem (sizeof(msgLocaType), NO);
		if (mp == NULL) {
			app_diag_S ("Loca malloc failed");
			goto Clear;
		}
		memset (mp, 0, sizeof(msgLocaType));
		in_header(mp, msg_type) = msg_loca;
		in_header(mp, rcv) = master_host;
		in_loca(mp, id) = locarr[ind].id;
		in_loca(mp, ref) = locarr[ind].ref;
		memcpy (in_loca(mp, vec), locarr[ind].vec, LOCAVEC_SIZ);
		talk (mp, sizeof(msgLocaType), TO_ALL); // will NOT go TO_NET on Master. see talk()		
		ufree (mp);
	}

Clear:
	memset (&locarr[ind], 0, sizeof(loca_t));
}

word loca_find (word id, word tout) {
#if LOCA_TRAC
	word bnum, bslo;
#endif
	word i = 0;
	while (i < LOCA_SNUM && locarr[i].id != id) {
		if (locarr[i].id && tout && (word)(seconds() - locarr[i].ts) > tout) {

#if LOCA_TRAC
			bnum = bslo = 0;
			while (bslo < LOCAVEC_SIZ) {
				if (locarr[i].vec[bslo] != 0) bnum++;
				bslo++;
			}
			app_diag_U ("LOCA clr %u %u %u %u", locarr[i].id, id, bnum, (word)(seconds() - locarr[i].ts));
#endif
			loca_out (i, YES);
		}
		i++;
	}
	return i;
}	

// loca slot, tag id, reference, slot within burst, rssi
void loca_ld (word lsl, word id, word ref, word bsl, word rss) {
	if (lsl == LOCA_SNUM) {
		if ((lsl = loca_find (0, 0)) == LOCA_SNUM) {
#if LOCA_TRAC
			app_diag_U ("LOCA full %u %u %u", id, ref, bsl);
#endif
			return;
		}
	}
	locarr[lsl].id = id;
	locarr[lsl].ref = ref;
	locarr[lsl].ts = (word)seconds();
	locarr[lsl].vec[bsl] = rss;
}

fsm locaudit {
	state INI:
		delay (LOCA_TOUT_AUDIT << 9, AUDI); // sec /2
		release;
	state AUDI:
		(void)loca_find (MAX_WORD, LOCA_TOUT_AUDIT);
		proceed INI;
}
