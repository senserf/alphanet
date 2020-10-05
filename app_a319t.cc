/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "vartypes.h"
#if BTYPE != BTYPE_AT_BUT6
#error AT_BUT6 only
#endif

#include "ap319_tag.h"

static void btyp_init () {
	ap319_init ();
}

#include "root_tag.cc"

