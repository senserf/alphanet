/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "war_tag.h"
#include "alrms.h"

war_t	warsaw; // my state

fsm randal () {
	state INI:
	    delay ((50 + (rnd() % 10)) << 10, ALRM);
	    release;
	state ALRM:
	    word w = (word)rnd();
	    if (w & 1) // further .5 chance
		set_alrm (7);

	    proceed INI;
}

void war_init () {
	runfsm randal;
}

