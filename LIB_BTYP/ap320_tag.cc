/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2011-2014                      */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "ap320_tag.h"
#include "alrms.h"

ap320_t	ap320;

static void do_butt (word b) {
	set_alrm (b +1); // buttons 0.., alrms 1..
}

void ap320_init () {
	buttons_action (do_butt);
}

