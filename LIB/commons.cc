/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "commons.h"
#include "diag.h"
#include "net.h"

char * get_mem (word len, Boolean r) {

        char * buf = (char *)umalloc (len);

        if (buf == NULL) {
                app_diag_S ("No mem");
                if (r)
                        reset();
        }
        return buf;
}

word get_word (byte * buf, word n) {
	return ((word)buf[n+1] << 8) | buf[n];
}

/////////////////////// fifek //////////////////////////
// it is used in all pegs and tags with stacked alarms, so it may be in commons
//
#define _FIFEK_DBG	0
#define _FIFEK_DEEP_DBG 0

#if _FIFEK_DEEP_DBG
fsm fifek_test {

	fifek_t repo;
	word mmin, mem, ts;

	state INI:
		fifek_ini (&repo, 5);
	state NEW:
		ts = (word)seconds();
		mem = memfree(0, &mmin);
		app_diag_U ("FIT starts(%u) s%u %u.%u", ts, repo.s, mem, mmin);

	state LOOP:
		char *ptr;

		if ((word)(seconds() - ts) > 10) {
			mem = memfree(0, &mmin);
			ts = seconds();
			app_diag_U ("FIT loops(%u) %u.%u %u %u %u",
				ts, mem, mmin, repo.h, repo.t, repo.n);
		}
		mem = (word)(rnd() % 100);
		if (mem == 99) {
			fifek_reset (&repo, (word)(rnd() % 6) +1);
			delay ((word)(rnd() % 1024) +1, NEW);
			release;
		}
		if (mem / 3 == 0)
			ufree (fifek_pull (&repo));
		else {
			ptr = get_mem ((word)(rnd() % 10) +1, NO);
			fifek_push (&repo, ptr);
		}

		delay ((word)(rnd() % 1024) +1, LOOP);
		release;
}
#endif

Boolean fifek_empty (fifek_t *fif) {
	return fif->n == 0;
}

Boolean fifek_full (fifek_t *fif) {
        return fif->n == fif->s;
}

void fifek_ini (fifek_t *fif, word siz) {
	memset (fif, 0, sizeof(fifek_t));

	if (siz > 15) {
		app_diag_S ("fifek trunced %u", siz);
		siz = 15;
	}
	fif->b = (char **) get_mem (siz * sizeof(char *), YES);
	memset (fif->b, 0, siz * sizeof(char *));
	fif->s = siz;

#if _FIFEK_DBG
	app_diag_U ("FIF%x ini %u", fif, siz);

#if _FIFEK_DEEP_DBG
// good for bug chasing, not for praxis testing
	if (!running (fifek_test))
		runfsm fifek_test;
#endif

#endif
}

char * fifek_pull (fifek_t *fif) {
	char * ptr;

	if (fifek_empty (fif)) {

#if _FIFEK_DBG
	app_diag_U ("FIF%x pull empty", fif);
#endif
		return NULL;
	}

	ptr = fif->b[fif->t];
	// this must be, or we chose a slow and painful if decide to overwrite 
	fif->b[fif->t] = NULL;
#if _FIFEK_DBG
	 app_diag_U ("FIF%x pulled %x (%u %u %u)", 
		fif, ptr, fif->h, fif->t, fif->n);
#endif
	--fif->n;
	++fif->t;
	fif->t %= fif->s;
#if _FIFEK_DBG
         app_diag_U ("FIF%x epull %u %u %u", fif, fif->h, fif->t, fif->n);
#endif
	return ptr;
}

void fifek_push (fifek_t *fif, char * el) {

#if _FIFEK_DBG
         app_diag_U ("FIF%x pushb %x (%u %u %u)", 
		fif, el, fif->h, fif->t, fif->n);
#endif

	if (fifek_full (fif)) 
		ufree (fifek_pull (fif));


	fif->b[fif->h] = el;
	++fif->n;
	++fif->h;
	fif->h %= fif->s;
#if _FIFEK_DBG
         app_diag_U ("FIF%x pushe %u %u %u", fif, fif->h, fif->t, fif->n);
#endif
}

void fifek_reset (fifek_t *fif, word siz) {

#if _FIFEK_DBG
	app_diag_U ("FIF%x reset %u->%u", fif, fif->s, siz);
#endif
	while (!fifek_empty (fif))
		ufree (fifek_pull (fif));

	ufree (fif->b);
	fifek_ini (fif, siz);
}
#undef _FIFEK_DBG

// for now, note that we violate nomenclature to maintain intentions: this is NOT for TARP...
void set_pxopts (word n, word xp, word cav) {
#if (RADIO_OPTIONS & RADIO_OPTION_PXOPTIONS)
	net_pxopts = ((xp & 7) << 12) | (cav & 0x0fff);
	if (n)
		net_pxopts |= 0x8000;
#else
	net_opt (PHYSOPT_SETPOWER, &xp);
#endif

}

word get_pxopts () {
#if (RADIO_OPTIONS & RADIO_OPTION_PXOPTIONS)
	return net_pxopts;
#else
	return net_opt (PHYSOPT_GETPOWER, NULL);
#endif
}
