/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014                           */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "vartypes.h"
#if BTYPE_WARSAW != BTYPE_WARSAW
#error WARSAW only
#endif

#include "war_tag.h"

static void btyp_init () {
	war_init ();
}

#include "root_tag.cc"

