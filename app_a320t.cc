/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014                           */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "vartypes.h"
#if BTYPE != BTYPE_AT_BUT1
#error AT_BUT1 only
#endif

#include "ap320_tag.h"

static void btyp_init () {
	ap320_init ();
}

#include "root_tag.cc"

