/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "vartypes.h"
#if BTYPE != BTYPE_ALPHANET_CHRONOS && BTYPE != BTYPE_ALPHANET_CHRONOS_WHITE
#error CHRONOS only
#endif

#include "chro_tag.h"

static void btyp_init () {
	chro_init();
}

#include "root_tag.cc"

