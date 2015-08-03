/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2014                    */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "diag.h"

void oss_ini () {
	app_diag_D ("oss_null");
}

void oss_tx (char * buf, word siz) {
	app_diag_D ("oss_null %x %u" buf, siz);
}

