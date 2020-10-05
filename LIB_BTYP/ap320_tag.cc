/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "ap320_tag.h"
#include "alrms.h"
#ifndef  __SMURPH__
#include "buttons.h"
#endif

ap320_t	ap320;

static void do_butt (word b) {
	set_alrm (b +1); // buttons 0.., alrms 1..
}

void ap320_init () {
	buttons_action (do_butt);
}

