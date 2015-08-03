/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014                           */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "vartypes.h"
#if BTYPE != BTYPE_AT_BUT6
#error AT_BUT6 only
#endif

#include "ap319_tag.h"

static void btyp_init () {
	ap319_init ();
}

#include "root_tag.cc"

