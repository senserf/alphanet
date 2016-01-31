/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2011-2014                      */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "chro_tag.h"
#include "form.h"
#include "looper.h"
#include "pong.h"
#include "tarp.h"
#include "inout.h"
#include "net.h"
#include "alrms.h"
#ifndef  __SMURPH__
#include "buttons.h"
#endif

fsm acc;
fsm lcd_mon;

#ifdef __SMURPH__
void ezlcd_init ();
void ezlcd_on ();
void ezlcd_off ();
void cma3000_on (byte, byte, byte);
void cma3000_off ();
void buzzer_init ();
void buzzer_on ();
void buzzer_off ();
#else
#include "ez430_lcd.h"
#include "pins.h"
#endif

/////////////////  chro.h?? ///////////////////////////////////////////

chro_t	chronos; // my state

#ifndef __SMURPH__
static void chro_lcd (const char *txt, word fr, word to) {
//
// Displays characters on the LCD
//
        char c;

        while (1) {
                if ((c = *txt) != '\0') {
                        if (c >= 'a' && c <= 'z')
                                c = c - 'a' + 10;
                        else if (c >= 'A' && c <= 'Z')
                                c = c - 'A' + 10;
                        else if (c >= '0' && c <= '9')
                                c -= '0';
                        else
                                c = 32;
                        ezlcd_item (fr, (word)c | LCD_MODE_SET);
                        txt++;
                } else {
                        ezlcd_item (fr, LCD_MODE_CLEAR);
                }
                if (fr == to)
                        return;
                if (fr > to)
                        fr--;
                else
                        fr++;
        }
}

void chro_hi (const char *txt) { chro_lcd (txt, LCD_SEG_L1_3, LCD_SEG_L1_0); }
void chro_lo (const char *txt) { chro_lcd (txt, LCD_SEG_L2_4, LCD_SEG_L2_0); }

void chro_nn (word hi, word a) {

        char erm [6];

        if (hi) {
                form (erm, "%u", a % 10000);
                chro_hi (erm);
        } else {
                form (erm, "%u", a);
                chro_lo (erm);
        }
}

void chro_xx (word hi, word a) {

        char erm [6];

        form (erm, "%x", a);

        if (hi) {
                chro_hi (erm);
        } else {
                chro_lo (erm);
        }
}
#else
void ezlcd_init () { emul (0, "(%lu) Init", seconds()); }
void ezlcd_on () { emul (0, "(%lu) ON", seconds()); }
void ezlcd_off () { emul (0, "(%lu) OFF", seconds()); }

void chro_xx (word hi, word a) {
	emul (0, "(%lu) %s: %x", seconds(), hi ? "hi" : "lo", a);
}

void chro_nn (word hi, word a) {
	if (hi)
		emul (0, "(%lu) hi: %d", seconds(), a % 10000);
	else
		emul (0, "(%lu) lo: %d", seconds(), a);
}

void chro_hi (const char *txt) { emul (0, "(%lu) hi: %s", seconds(), txt); }
void chro_lo (const char *txt) { emul (0, "(%lu) lo: %s", seconds(), txt); }

void cma3000_on (byte m, byte n, byte o) { 
	emul (0, "(%lu) cma %d, %d, %d", seconds(), m, n, o);
}

void cma3000_off () { emul (0, "(%lu) cma off", seconds()); }

void buzzer_init () { emul (0, "(%lu) buzzer_init", seconds()); }
void buzzer_on () { emul (0, "(%lu) buzzer_on", seconds()); }
void buzzer_off () { emul (0, "(%lu) buzzer_off", seconds()); }

#endif

fsm buzz {
        state BON:
                buzzer_on();
                delay (300, BOFF);
                release;

        state BOFF:
                buzzer_off();
                finish;
}

// handling of RONIN / DORO triggers depends on if we want to maintain
// a 'permanent' beeping is there is no peg around. Here, we beep only
// if there is an unacked alrm (if alrm is passed). We ignore TRIG_RONIN.
// Could produce racing... we'll see.
fsm beep (Boolean alrm) {

        state BON:
                buzzer_on();
                delay (500, BOFF);
		if (alrm) when (TRIG_DORO, FIN);
                release;

        state BOFF:
                buzzer_off();
                delay (10000, BON);
		if (alrm) when (TRIG_DORO, FIN);
                release;
	state FIN:
		buzzer_off();
		finish;
}

fsm acc {

        state INI:
                if (chronos.acc_mode == 0) {
                        cma3000_off ();
                        finish;
                }
                cma3000_on (0, 1, 3);
                wait_sensor (0, MOTION); // embedded release

        state MOTION:
                cma3000_off ();
                chronos.move_ts = (word)seconds();
                chronos.move_nr++;

                if (chronos.acc_mode == 1)
			set_alrm (2);

                delay (10240, INI);
                release;
}

#define _LOOP   5120
#define _PAUSE  10240
#define _check_ev ((word)(seconds() - chronos.ev_ts) < (_PAUSE >> 10))
fsm lcd_monit {
        state ALF:
                if _check_ev
                        proceed PAUSE;

                chro_lo ("ALPHA");
                chro_nn (1, local_host);
                delay (_LOOP, ACC);
                release;

        state ACC:
                if _check_ev
                        proceed PAUSE;

                chro_hi ("ACC");
                chro_nn (0, chronos.acc_mode);
                delay (_LOOP, HBEAT);
                release;

        state HBEAT:
                if _check_ev
                        proceed PAUSE;

                chro_hi ("HBEAT");
                chro_nn (0, heartbeat);
                delay (_LOOP, POWER);
                release;

        state POWER:
                if _check_ev
                        proceed PAUSE;

                chro_hi ("POLE");
                chro_xx (0, pong_params.pow_levels);
                delay (_LOOP, ALF);
                release;

        state PAUSE:
                if _check_ev {
                        delay (_PAUSE, PAUSE);
                        release;
                }
                if (chronos.last_but == 4) { // implied sleep... I think
                        ezlcd_off();
                        finish;
                }
		chronos.last_but = 0; // cancel 'button context'
                proceed ALF;
}
#undef _LOOP
#undef _PAUSE
#undef _check_ev

static void do_butt (word b) {

        if (chronos.last_but == 4) // any button wakes up
                reset();

	chronos.ev_ts = (word)seconds();
        // sleep button #4
        if (b == 4) {
                chro_hi ("SOON");
                chro_lo ("SLEEP");
                cma3000_off ();
                net_opt (PHYSOPT_RXOFF, NULL);

                killall (pong);
                killall (buzz); killall (beep);
                buzzer_off();
                killall (acc); killall (looper);
                killall (hear);
		chronos.last_but = 4;
                return;
        }

        if (b == 2) {
	    if (chronos.last_but == 2) {
                switch (heartbeat) {
                        case 60:
                                heartbeat = 900;
                                break;
                        case 900:
                        case 1800:
                                heartbeat *= 2;
                                break;
                        case 3600:
                                heartbeat = 10;
                                break;
                        case 10: // these are mostly for testing
                        case 20:
                        case 30:
                        case 40:
                                heartbeat += 10;
                                break;
                        default:
                                heartbeat = 60;
                }
	    } else
		chronos.last_but = 2;

            chro_hi ("HBEAT");
            chro_nn (0, heartbeat);
            return;
        }

        if (b == 3) {
	    if (chronos.last_but == 3) {
                switch (pong_params.pow_levels) {
                        case 0x7531:
                                pong_params.pow_levels = 0x1111;
                                break;
                        case 0x1111:
                        case 0x2222:
                        case 0x3333:
                        case 0x4444:
                        case 0x5555:
                        case 0x6666:
                                pong_params.pow_levels += 0x1111;
                                break;
                        default:
                                pong_params.pow_levels = 0x7531;
                }
	    } else
		chronos.last_but = 3;

            chro_hi ("POER");
            chro_xx (0, pong_params.pow_levels);
            return;
        }

        if (b == 1) {
	    if (chronos.last_but == 1) {
                chronos.acc_mode = (chronos.acc_mode + 1) % 3;

                if (chronos.acc_mode && !running (acc))
                        runfsm acc;

                if (chronos.acc_mode == 0 && running (acc)) {
                        killall (acc);
                        cma3000_off ();
                }
	    } else
		chronos.last_but = 1;

            chro_hi ("ACC");
            chro_nn (0, chronos.acc_mode);
            return;
        }

        // only B0 triggers (forcing-in 'pending' changes, e.g. heartbeat)
	set_alrm (1);
}

void chro_init () {
        cma3000_on (0, 1, 3);
        ezlcd_init ();
        ezlcd_on ();
        //buzzer_init ();
        chro_lo ("ALPHA");
        chro_nn (1, local_host);
        memset (&chronos, 0, sizeof (chro_t));
        runfsm lcd_monit;
        buttons_action (do_butt);
}

