/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/

#include "diag.h"
#include "form.h"

void _app_diag (const char * fmt, ...) {

	char * buf;

	va_list		ap;
	va_start (ap, fmt);

	if ((buf = vform (NULL, fmt, ap)) == NULL) {
		diag ("no mem");
		return;
	}
#ifdef __SMURPH__
	emul (0, "app: %s", buf);
#else
	diag ("app: %s", buf);
#endif
	ufree (buf);
}
