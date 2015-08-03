/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2014                           */
/* All rights reserved.                                                 */
/* ==================================================================== */

#include "inout.h"
#include "looper.h"
#include "pong.h"
#include "diag.h"
#include "net.h"
#include "lit.h"

fsm ronin {
	state ST:
		when (TRIG_RONIN, ACT);
		release;

	state ACT:
		clr_lit (YES);
		set_lit (30, LED_ALRM, LED_BLINK, 0);
		proceed ST;
}

static void init0 () {
	powerdown();
    master_host = local_host; // I'm not sure what this is for...
    tarp_ctrl.param &= 0xFE; // routing off

	btyp_init ();
	init_inout ();
	net_opt (PHYSOPT_RXOFF, NULL); // default is ON
}

static void init () {
	init_pframe();
    runfsm hear;
    runfsm looper;
	runfsm ronin;
}

void process_incoming (char * buf, word size, word rssi) {

  switch (in_header(buf, msg_type)) {

	case msg_pongAck:
		if (in_pongAck(buf, dupeq) == 
				in_pong(pong_frame, pd).dupeq)
			trigger (TRIG_ACK);
		else
			app_diag_W ("Alien ack %u", in_pongAck(buf, dupeq));

		// this is truly superfluous, here just for illustration,
		// if anything is truly needed, it should be inited from
		// pong when (TRIG_ACK, ...)
		talk (buf, size, TO_OSS);
		break;

	default:
		app_diag_D ("Msg %u fr %u (%u %u)", in_header(buf, msg_type),
			in_header(buf, snd), size, rssi);
  }

}

fsm root {

	entry RS_INIT:
#ifdef __SMURPH__
		word d = 1024 + (rnd() % 5121);
#else
		word d = 3072;
#endif
		init0();
		set_lit (d >> 10, LED_ALRM, LED_BLINK, 0);
		delay (d, RS_INIT1);
		release;

	entry RS_INIT1:
		init();
		finish;
}

