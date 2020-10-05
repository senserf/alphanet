/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "vartypes.h"
#if BTYPE_WARSAW != BTYPE_WARSAW
#error WARSAW only
#endif

#include "war_tag.h"

static void btyp_init () {
	war_init ();
}

#include "root_tag.cc"

