/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2016                           */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "vartypes.h"
#if BTYPE != BTYPE_AT_LOOP
#error AT_LOOP only
#endif

#include "ap331_tag.h"

static void btyp_init () {
	ap331_init ();
}

#include "root_tag.cc"
