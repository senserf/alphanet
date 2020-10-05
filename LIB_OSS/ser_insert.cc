/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/

///////////////// oss out ////////////////
// this is better than multiple fsm oss_out I used before
//

static fifek_t oss_cb;

// if we compile with the 2 #ifs switched off, we'll be overwriting circ. buf;
// i.e. old lines will be lost (instead of new ones).
static word _oss_out (char * b) {

	if (b == NULL)
		return 2;
#if 1
	if (!fifek_full (&oss_cb)) {
#endif
		fifek_push (&oss_cb, b);
		trigger (TRIG_OSSO);
		return 0;
#if 1
	}
	// we (arbitrary) decide: don't overwrite
	app_diag_S ("OSS oflow (%u)", (word)seconds());
	ufree (b);
	return 1;
#endif
}
 
fsm perp_oss_out () {
	char * ptr;

	state CHECK:
		if (fifek_empty (&oss_cb)) {
			when (TRIG_OSSO, CHECK);
			release;
		}
		ptr = fifek_pull (&oss_cb);

	state RETRY:
		ser_outb (RETRY, ptr);
		proceed (CHECK);
}

/////////////////

fsm cmd_in;
void oss_ini () {

#ifdef BOARD_WARSAW_BLUE
	// Use UART 2 via Bluetooth
	ser_select (1);
#endif
	fifek_ini (&oss_cb, 15);
	if (!running (perp_oss_out))
		runfsm perp_oss_out;

	if (!running (cmd_in))
		runfsm cmd_in;
}

