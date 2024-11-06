/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "vartypes.h"
#if BTYPE != BTYPE_AT_LOOP
#error AT_LOOP only
#endif

#include "ap331_tag.h"

static void btyp_init () {
	ap331_init ();
}

#include "root_tag.cc"
