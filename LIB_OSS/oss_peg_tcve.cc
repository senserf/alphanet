/* ==================================================================== */
/* Copyright (C) Olsonet Communications, 2002 - 2014                    */
/* All rights reserved.                                                 */
/* ==================================================================== */
#include "commons.h"
#include "vartypes.h"
#include "diag.h"
#include "tag_mgr.h"
#include "tarp.h"
#include "inout.h"
#include "tcvphys.h"
#include "phys_uart.h"

// only for heartbeat, we'll see
#include "looper.h"

#include "tcve_insert.cc"

typedef struct statsStruct {
	word	type :8;
	word	len	 :8;
	word	nid;
	word	mid;
	word	up_l;
	word	up_h;
	word	mem;
	word	min;
} stats_t; // 14B

/*  in 1.5 little is mutable so it doesn't matter. Let's have version, lh, master, uptime and memfree
	byte len = 24 (malloc 24 +1)
	// out:
	byte	seq;
	word	lh;
	byte	efef;
	byte	reptype;// REP_LOG
	byte	sv;		// let's go with _I (info)
	byte	tp;		// #dbg# 0 - stats
	byte	it1; 	// word in hex 0x81
	word	ver;
	// lh is in the frame, it is enough even if we go remotely(?)
	byte 	it3;	// word 0xC1
	word	master_host;
	byte	it4;	// lword 0xC2
	lword	uptime;
	byte	it5;	// word 0xC1
	word	mem;
	byte	it6;	// word 0xC1
	word	mmin;

*/
static void stats () {
	word mmin, mem;
	byte * b = (byte *)get_mem (24 +1, NO);
	if (b == NULL)
		return;

	mem = memfree(0, &mmin);
	b[0]  = 24;
	b[1]  = 0;
	b[2]  = (byte)local_host;
	b[3]  = (byte)(local_host >> 8);
	b[4]  = 0xFF;
	b[5]  = REP_LOG;
	b[6]  = D_INFO;
	b[7]  = 0;
	b[8]  = 0x81;
	b[9]  = SYSVER_min;
	b[10] = SYSVER_MAJ;
	b[11] = 0xC1;
	b[12] = (byte)master_host;
	b[13] = (byte)(master_host >> 8);
	b[14] = 0xC2;
	b[15] = (byte)seconds();
	b[16] = (byte)(seconds() >> 8);
	b[17] = (byte)(seconds() >> 16);
	b[18] = (byte)(seconds() >> 24);
	b[19] = 0xC1;
	b[20] = (byte)mem;
	b[21] = (byte)(mem >> 8);
	b[22] = 0xC1;
	b[23] = (byte)mmin;
	b[24] = (byte)(mmin >> 8);

	_oss_out ((char *)b, NO);
}

// this is a 'low level' (n)ack
static void sack (byte seq, byte status) {
	char *b;

	if ((b = get_mem (5 +1, NO)) == NULL)
		return;

	b [0] = 5; // frame's length;
	b [1] = seq;
	
	((address)b) [1] = local_host; // happened to be aligned
	
	b [4] = 0;
	b [5] = status;
	
	_oss_out (b, YES);
}

// This is an 'empty' (no payload) response. It seems that this may be more popular that low-level acks.
// Note that we assume that RC_OK is returned in the frame - otherwise we wouldn't be here.
static void nop_resp (byte opco, byte opre, byte oprc) {
	char *b;

	if (!(opre & 0x80)) { // no 'op ack' was requested
		if (oprc != RC_OK)
			app_diag_S ("silent opco %x oprc %x", opco, oprc);
		return;
	}

	if ((b = get_mem (1+3+5, NO)) == NULL) {
		app_diag_S ("nop_resp mem 9 %u", opco);
		return;
	}
	
	b [0] = 8; // frame's length;
	b [1] = 0;
	
	((address)b) [1] = local_host;
	
	b [4] = opco;
	b [5] = opre;
	b [6] = oprc;
	b [7] = b[2];
	b [8] = b[3];
	
	_oss_out (b, NO);
}
	
///////////////// oss in ////////////////

// lengths of the pair (code,val). static par_lengths can be wrapped in a proc call to go on stack...
// could be compressed on 4 bits, if we care
#define PAR_CODE_SIZE	32
static const byte par_len[PAR_CODE_SIZE] = { 0, 
										3, // PAR_LH
										5, // ATTR_ESN
										3, // ATTR_MID
										3, // ATTR_NID
										2, // PAR_TARP_L
										2, // PAR_TARP_R
										2, // PAR_TARP_S
										2, // PAR_TARP_F
										2, // PAR_TARP 0x09
										7, // ATTR_TARP_CNT
										2, // PAR_TAG_MGR
										3, // PAR_AUDIT
										2, // PAR_AUTOACK
										3, // PAR_BEAC
										3, // ATTR_VER 0x0F
										0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
										5, // ATTR_UPTIME 0x1A
										5, // ATTR_MEM1
										5, // ATTR_MEM2
										2, // PAR_SNIFF 0x1D
										0, 0
};

// pcode assumed valid
static byte get_param (byte * ptr, byte pcode) {
	word w, w1;
	
	switch (pcode) {
		case PAR_LH:
			memcpy (ptr, &local_host, 2);
			break;
		case ATTR_ESN:
			// memcpy (ptr, &host_id, 4); // not lvalue
			*ptr = (byte)host_id;
			*(ptr +1) = (byte)(host_id >> 8);
			*(ptr +2) = (byte)(host_id >> 16);
			*(ptr +3) = (byte)(host_id >> 24);			
			break;
		case PAR_MID:
			memcpy (ptr, &master_host, 2);
			break;
		case PAR_NID:
			memcpy (ptr, &net_id, 2);
			break;
		case PAR_TARP_L:
		case PAR_TARP_R:
		case PAR_TARP_S:
		case PAR_TARP_F:
		case PAR_TARP:
			*ptr = tarp_ctrl.param;
			break;
		case ATTR_TARP_CNT:
			memcpy (ptr, &tarp_ctrl.rcv, 2);
			memcpy (ptr+2, &tarp_ctrl.snd, 2);
			memcpy (ptr+4, &tarp_ctrl.fwd, 2);
			break;
		case PAR_TAG_MGR:
			*ptr = tagList.block;
			break;
		case PAR_AUDIT:
			memcpy (ptr, &heartbeat, 2);
			break;
		case PAR_BEAC: // cheat: now it is hardcoded
			*ptr = local_host == master_host ? 60 : 0;
			*(ptr +1) = 0;
			break;
		case ATTR_VER:  // LSB
			*ptr = SYSVER_min;
			*(ptr +1) = SYSVER_MAJ;
			break;
		case ATTR_UPTIME:
			*ptr = (byte)seconds();
			*(ptr +1) = (byte)(seconds() >> 8);
			*(ptr +2) = (byte)(seconds() >> 16);
			*(ptr +3) = (byte)(seconds() >> 24);
			break;
		case ATTR_MEM1:
			w = memfree(0, &w1);
			memcpy (ptr, &w, 2);
			memcpy (ptr+2, &w1, 2);
			break;
		case ATTR_MEM2:
			w = maxfree(0, &w1);
			w1 = stackfree();
			memcpy (ptr, &w, 2);
			memcpy (ptr+2, &w1, 2);
			break;
		case PAR_SNIFF:
			*ptr = 0; // later but still in 1.5... we'd like to have it there, perhaps without promiscuous mode
			break;
		default:
			return RC_EPAR;
	}
	return RC_OK;
}

// *ptr (pcode) assumed reasonably checked (nonzero length, within pcode space)
static byte set_param (byte * ptr) {
	word w;
	byte rc = RC_OK;
	
	switch (*ptr) {
		case PAR_LH:			
			// 1.5 maintains const masters (DEF_MHOST)
			if (local_host != DEF_MHOST && (w = get_word (ptr, 1)) != 0 && w != DEF_MHOST)
				local_host = w;
			else
				rc = RC_EVAL;
			break;

		case PAR_TARP_F:
			if (*(ptr+1))
				tarp_ctrl.param |= 1;
			else
				tarp_ctrl.param &= 0xFE;
			break;
			
		case PAR_MID:
		case PAR_NID:
		case PAR_TARP_L:
		case PAR_TARP_R:
		case PAR_TARP_S:
		case PAR_TARP:
		case PAR_TAG_MGR:
		case PAR_AUDIT:
		case PAR_BEAC:
		case PAR_SNIFF:
			rc = RC_ENIMP;
			break;
		default:
			rc = RC_EPAR;
	}
	return rc;
}

// confusing: 3 - frame head, 4 - request op fields, 5 - response op fields
static void cmd_get (byte * buf, word len) {
	byte * bend = buf + len; // 1st out
	byte * ptr = buf +3 +4;
	byte * obu = NULL;
	byte * otr;
	byte oprc = RC_OK;
	byte reslen = 3 +5; // 3 frame + 5 op_ fields, not counting params yet
	
	// calculate response buffer size
	while (ptr < bend) {
		if (*ptr >= PAR_CODE_SIZE || par_len[*ptr] == 0) {
			oprc = RC_EPAR;
			ptr++;
			continue;
		}
		
		reslen += par_len[*ptr++];
	}
	
	if ((obu = (byte *)get_mem (reslen +1, NO)) == NULL) { // +1 is for the pkt length for oss_out
		app_diag_S ("get no mem %u", reslen +1);
		return;
	}
	obu [0] = reslen;	// this is for _oss_out
	obu [1] = 0;	// frame: seq will be set in perp_oss_out, we don'r want acks
	((address)obu) [1] = local_host;
	obu [4] = CMD_GET;
	obu [5] = buf [4];	// op_ref
	obu [6] = oprc;
	obu [7] = obu [2];
	obu [8] = obu [3];
	
	otr = obu +9;
	ptr = buf +3 +4;
	
	while (ptr < bend) {
		if (*ptr >= PAR_CODE_SIZE || par_len[*ptr] == 0) {
			ptr++;
			continue;
		}
		
		// load a single param
		*otr = *ptr;		 					// symbol
		(void)get_param (otr +1, *ptr);	 		// value
		otr += par_len [*ptr++];
		
	}
	
	_oss_out ((char *)obu, NO);
	
}

// Other options could be implemented, we've chosen this: we set params from the list, until the 1st failure.
// Then, if op_rc has high bit set, we return with op_rc. We never return data (param values).
static void cmd_set (byte * buf, word len) {
	byte * bend = buf + len; // 1st out the buf
	byte * ptr = buf +3 +4;
	char * obu = NULL;
	byte oprc = RC_OK;
	
	while (oprc == RC_OK && ptr < bend) {
		if (*ptr >= PAR_CODE_SIZE || par_len[*ptr] == 0) {
			oprc = RC_EPAR;
		} else {
			oprc = set_param (ptr);
			ptr += par_len [*ptr];
		}
	}

	nop_resp (buf[3], buf[4], oprc);
}

static byte check_ass (byte * buf, word len) {

	if (len < 8 || ((len - 8) % 3))	// len-8 is the length of series of (id, butmap)
		return RC_ELEN;

	// no bcast on any(?) level
	if (get_word (buf, 1) != local_host || get_word (buf, 5) != local_host)
		return RC_EADDR;

	return RC_OK;
}

static void cmd_set_ass (byte * buf, word len) {
	byte rc = check_ass (buf, len);

	if (rc == RC_OK)
		b2treg ((len-8) /3, buf +7);

	nop_resp (buf [3], buf [4], rc);

}

static void cmd_get_ass (byte * buf, word len) {
	word m;
	byte * b;
	byte rc;

	if ((rc = check_ass (buf, len)) != RC_OK) {
		nop_resp (buf [3], buf [4], rc);
		return;
	}
	
	m = slice_treg ((word)buf[7]); // m: how many
	if (m)
		len = 1+3+5+1 +3* m; // how many to size
	else
		len = 1+3+5+1 +1; // special case for learning mode
		
	if ((b = (byte *)get_mem (len, NO)) == NULL) {
		app_diag_S ("getass mem %u", len);
		return;
	}

	b [0] = len -1;
	b [1] = 0;
	((address)b) [1] = local_host;
	b [4] = CMD_GET_ASSOC;
	b [5] = buf [4];  // op_ref
	b [6] = RC_OK;
	b [7] = b [2];
	b [8] = b [3];
	b [9] = buf [7]; // index
	if (m)
		treg2b (b+9);
	else
		b[10] = learn_mod;
		
	_oss_out ((char *)b, NO);
}
	
static void cmd_clr_ass (byte * buf, word len) {
	byte rc = check_ass (buf, len);

	if (rc == RC_OK)
		reset_treg();
		
	nop_resp (buf [3], buf [4], rc);
}	

static void cmd_relay (byte * buf, word len) {
	byte * b;
	word w;
	
	// at least 1 byte to send and no more than (arbitrary) 40
	if (len < 3 +4 +2 +1 || len > 3+4 +2 +40) {
		nop_resp (buf [3], buf [4], RC_ELEN);
		return;
	}
	
	// no bcast on any(?) level
	if (get_word (buf, 1) != local_host || get_word (buf, 5) != local_host) {
		nop_resp (buf [3], buf [4], RC_EADDR);
		return;
	}

	w = len + sizeof(msgFwdType) - 9;
	if ((b = (byte *)get_mem (w, NO)) == NULL) {
		nop_resp (buf [3], buf [4], RC_ERES);
		return;
	}
	
	memset (b, 0, w);
	in_header(b, msg_type) = msg_fwd;
	in_header(b, rcv) = get_word (buf, 7);
	in_fwd(b, optyp) = buf[3];
	in_fwd(b, opref) = buf[4];
	in_fwd(b, len) = w - sizeof(msgFwdType);
	memcpy ((char *)(b+sizeof(msgFwdType)), (char *)buf +9, in_fwd(b, len));
	talk ((char *)b, w, TO_NET);
	// show in emul for tests in vuee
	app_diag_U ("fwd %x to %u #%u", in_fwd(b, optyp), in_header(b, rcv), in_fwd(b, opref));
	ufree (b);
	// note that likely we should NOT respond, fwdAck will if requested
	// nop_resp (buf [3], buf [4], RC_OK);
}

static void process_cmd_in ( byte * buf, word len) {

	switch (buf[3]) { // op_code
		case CMD_GET:
			cmd_get (buf, len);
			break;
		case CMD_SET:
			cmd_set (buf, len);
			break;
		case CMD_SET_ASSOC:
			cmd_set_ass (buf, len);
			break;			
		case CMD_GET_ASSOC:
			cmd_get_ass (buf, len);
			break;
		case CMD_CLR_ASSOC:
			cmd_clr_ass (buf, len);
			break;
		case CMD_RELAY_41:
		case CMD_RELAY_42:
		case CMD_RELAY_43:
			cmd_relay (buf, len);
			break;
		default:
			nop_resp (buf [3], buf [4], RC_ENIMP);
			app_diag_W ("process_cmd_in not yet");
	}
}

fsm mbeacon;
fsm looper;
fsm cmd_in {

	state START:
		stats ();

	state LISN: // LISTEN keyword??
		word plen, w;
		char * ib = (char *) tcv_rnp (LISN, oss_fd);

		if ((plen = tcv_left ((address)ib)) < 5) { // drop (5 - (n)ack is the shortest)
			app_diag_S ("cmdlen %u", plen);
			goto goLisn;
		}
		
		// 1.5: not clear if we should ignore others; now, keep as in 1.0, just for FG_ACKR tests 
		if (ossi.lout_ack) { // only (n)acks allowed
			if ((ossi.lout_ack & 0x7F) == (byte)ib[0]) {
				trigger (TRIG_OSSIN);
				app_diag_I ("Got (n)ack %x", (word)ib[4]);
			} else {
				app_diag_S ("badack %x", ((word)ossi.lout_ack << 8) | *ib);
			}
			goto goLisn;
		}
		
		// NOTE that with (n)ack pending, we won't get here
		
		// duplicates:
		if ((ib[0] & 0x7F) == ossi.lin) {

			if (ib[0] & 0x80)
				sack (ib[0], ossi.lin_stat == RC_OK ? RC_DUPOK : ossi.lin_stat);
				
			goto goLisn;
		}

		// signal peer's restart
		if ((ib[0] & 0x7F) == 1)
			app_diag_I ("Peer restarted");
		
		// accept & check the frame
		ossi.lin = ib[0] & 0x7F;
		ossi.lin_stat = RC_OK;
		
		// note that ossi.lin_stat is set no matter if it is to be sent or not
		if (ossi.lin == 0) {
			ossi.lin_stat = RC_EVAL;
			if (ib[0] & 0x80)
				sack (ib[0], ossi.lin_stat);
			goto goLisn;
		}
		
		w = get_word ((byte *)ib, 1);
		if (w != 0 && w != local_host)
			ossi.lin_stat = RC_EADDR;

		// disallow remote cmd (for now)
		w = get_word ((byte *)ib, 5);
		if (w != 0 && w != local_host)
			ossi.lin_stat = RC_EADDR;

		// ack the frame (perhaps it is premature but for now it'll do)
		if (ib[0] & 0x80)
			sack (ib[0], ossi.lin_stat);
			
		if (ossi.lin_stat != RC_OK)	// we won't go any further
			goto goLisn;

		// we're done with the frame, entering cmd processing
		// we assume FG_ACKR was served (may be a bad assumption, we'll see)
		// tempting, but whole frame is needed(?): process_cmd_in ((byte *)(ib +3), plen -3);
		process_cmd_in ((byte *)ib, plen);

goLisn:	tcv_endp ((address)ib);
		proceed LISN;
}
///////////////////////////////

#define _ppp	(p + sizeof(pongDataType))
static void board_out (char * p, char * b) {

	// for boards other tham ap319, 320: just for fun
	switch (((pongDataType *)p)->btyp) {
		case BTYPE_CHRONOS:
		case BTYPE_CHRONOS_WHITE:
			b[11] = (byte)((((pongPloadType0 *)_ppp)->volt - 1000) >> 3);
			b[16] = (byte)((pongPloadType0 *)_ppp)->move_ago; // vy not
			b[17] = (byte)((pongPloadType0 *)_ppp)->move_nr;
			break;

		case BTYPE_AT_BASE:
			b[11] = (byte)((((pongPloadType2 *)_ppp)->volt - 1000) >> 3);
			b[16] = 9;
			b[17] = 14;
			break;
			
		case BTYPE_AT_BUT6:
			b[16] |= ((pongPloadType3 *)_ppp)->glob;
			b[11] = (byte)((((pongPloadType3 *)_ppp)->volt - 1000) >> 3);
			b[17] = ((pongPloadType3 *)_ppp)->dial;
			break;

		case BTYPE_AT_BUT1:
			b[16] |= 1;
			b[11] = (byte)((((pongPloadType4 *)_ppp)->volt - 1000) >> 3);
			b[17] = 0;
			break;
			
		case BTYPE_WARSAW:
			b[16] = (byte)((pongPloadType5 *)_ppp)->random_shit;
			b[11] = (byte)((((pongPloadType5 *)_ppp)->volt - 1000) >> 3);
			b[17] = (byte)((pongPloadType5 *)_ppp)->steady_shit;
			break;

		default:
			app_diag_W ("btyp %u", ((pongDataType *)p)->btyp);
	}
}
#undef _ppp

// this is the main out on the oss i/f, I don't know at all if this makes
// more sense than no generic i/f at all
// NO parameter sanitization
void oss_tx (char * b, word siz) {
	char *bu;

	// I'm not sure (now) what to do with these 2 spec. cases
	// likely, we'll need a dbg i/f; for now dump to emul

	// allocated, freed after output
	if (siz == MAX_WORD) {
		app_diag_U (b);
		ufree (b);
		return;
	}
	if (siz == 0) { // copied for direct output
		app_diag_U (b);
		return;
	}

	// b[0] doesn't have to be msg_type; if not, must be off the enum
	switch (b[0]) {
	
		case msg_report:

			/*
				byte len = 17 (malloc 17 +1)
				// out:
				byte	seq;
				word	local_host;
				byte	efef;
				byte	reptype;
				word	pegid;
				word	tagid;
				byte	del;
				byte	bv;
				byte	rss;
				byte	xa;
				byte	et;
				byte 	sn;
				// Args
				byte	fl;
				byte	dial;
			*/			
			if ((bu = get_mem (18, NO)) == NULL)
				return;
			
			bu[0] = 17;
			bu[1] = 0; // would be 0x80 if we wanted ack (say, alrms on the master could be 'more reliable')
			bu[2] = (byte)local_host;
			bu[3] = (byte)(local_host >> 8);
			bu[4] = 0xFF;
			bu[5] = REP_EVENT;
			if (in_header(b, snd) == 0) {
				bu[6] = bu[2]; // local_host
				bu[7] = bu[3];
			} else {
				bu[6] = (byte)in_header(b, snd);
				bu[7] = (byte)(in_header(b, snd) >> 8);
			}
			bu[8] = (byte)in_report(b, tagid);
			bu[9] = (byte)(in_report(b, tagid) >> 8);
			bu[10] = in_report(b, ago);
			// board-specific bu[11] = voltage
			bu[12] = in_report(b, rssi);
			bu[13] = ((pongDataType *)(b + sizeof(msgReportType)))->plev;
			bu[14] = ((pongDataType *)(b + sizeof(msgReportType)))->alrm_id |
					(((pongDataType *)(b + sizeof(msgReportType)))->btyp << 4);
			bu[15] = ((pongDataType *)(b + sizeof(msgReportType)))->alrm_seq;
			bu[16] = ((pongDataType *)(b + sizeof(msgReportType)))->trynr << 4;
			//... and | 0x80 for not registered events (noack)
			if (((pongDataType *)(b + sizeof(msgReportType)))->noack)
				bu[16] |= 0x80;
			// board specific will bu[16] |= global flag
			// board-specific bu[17] = dial
			board_out (b + sizeof(msgReportType), bu);
			_oss_out (bu, NO);
			break;

	    case msg_reportAck:
		// renesas doesn't want to see it
		
			app_diag_U ("repAck #%u t%u l%u.%u", in_reportAck(b, ref),
				in_reportAck(b, tagid), tagList.alrms, tagList.evnts);
			break;

	    case msg_fwd:
			// I'm not sure why this is here...
			if (in_header(b, snd) == 0 || in_header(b, snd) == local_host) {
				app_diag_S ("fwd??");
				break;
			}

			/*  
				byte len = len + 8 (malloc ... +1)
				// out:
				byte	seq;
				word	lh;
				byte	efef;
				byte	reptype;	// REP_RELAY
				byte	optyp;		// 0x41, 42, 43
				// not now, we'll see later byte	opref;		// from msg_fwd
				word	src;
				byte[]  pload		// len from msg_fwd

			*/			

			if ((bu = get_mem (in_fwd(b, len) +8 +1, NO)) == NULL) {			
				break;
			}
			bu [0] = in_fwd(b, len) +8;
			bu [1] = 0;
			bu [2] = (byte)local_host;
			bu [3] = (byte)(local_host >> 8);
			bu [4] = 0xFF;
			bu [5] = REP_RELAY;
			bu [6] = in_fwd(b, optyp);
			// bu [7] = in_fwd(b, opref);
			bu [7] = (byte)in_header(b, snd);
			bu [8] = (byte)(in_header(b, snd) >> 8);
			memcpy (bu + 9, b + sizeof(msgFwdType), in_fwd(b, len));
			_oss_out (bu, NO);
			break;

	    case msg_fwdAck:
			nop_resp (in_fwdAck(b, optyp), in_fwdAck(b, opref), RC_OK);
			app_diag_U ("fwdAck #%u fr%u", in_fwdAck(b, opref), in_header(b, snd));
			break;

		case msg_loca:
			/*
				byte len = 11+32 (malloc 43 +1)
				// out:
				byte	seq;
				word	local_host;
				byte	efef;
				byte	reptype;
				word	pegid;
				word	tagid;
				word	ref;
				byte	vec[32];
			*/			
			if ((bu = get_mem (44, NO)) == NULL)
				return;
			
			bu[0] = 43;
			bu[1] = 0;
			bu[2] = (byte)local_host;
			bu[3] = (byte)(local_host >> 8);
			bu[4] = 0xFF;
			bu[5] = REP_LOCA;
			if (in_header(b, snd) == 0) {
				bu[6] = bu[2]; // local_host
				bu[7] = bu[3];
			} else {
				bu[6] = (byte)in_header(b, snd);
				bu[7] = (byte)(in_header(b, snd) >> 8);
			}
			bu[8] = (byte)in_loca(b, id);
			bu[9] = (byte)(in_loca(b, id) >> 8);
			bu[10] = (byte)in_loca(b, ref);
			bu[11] = (byte)(in_loca(b, ref) >> 8);
			memcpy (&bu[12], in_loca(b, vec), 32);
			_oss_out (bu, NO);
			break;
			
		default:
			app_diag_S ("unfinished? %x %u", *(address)b, siz);
	}
}

