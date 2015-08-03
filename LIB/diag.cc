/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2013.                   */
/* All rights reserved.                                                 */
/* ==================================================================== */

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
