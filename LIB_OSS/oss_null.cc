/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "diag.h"

void oss_ini () {
	app_diag_D ("oss_null");
}

void oss_tx (char * buf, word siz) {
	app_diag_D ("oss_null %x %u" buf, siz);
}

