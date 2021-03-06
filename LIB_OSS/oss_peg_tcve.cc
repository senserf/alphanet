/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "commons.h"
#include "vartypes.h"
#include "diag.h"
#include "tag_mgr.h"
#include "tarp.h"
#include "net.h"
#include "inout.h"
#include "tcvphys.h"
#include "phys_uart.h"
#include "loca.h"
#include "pegs.h"
#include "sniffer.h"

// only for heartbeat, we'll see
#include "looper.h"

#include "tcve_insert.cc"

#define _OSS_FR_LEN	3
/*

	I'm not sure if this won't change again, but for now (Feb. 2015):
	
	If obuf goes to OSSI, via fifek. So, obuf[0] is the length of the rest,
	Then, _OSS_FR_LEN follows, now 3B
	OSS data starts from b[_OSS_FR_LEN +1]:
	e.g.
	b[0] - fifek head with OSS content's length
	b[1]... OSS content which contains
	b[4]... OSS data
	
	If obuf goes via RF, it is the header plus OSS data
*/

// len: of OSS data (_OSS_FR_LEN (3B) less than 'OSS content')
static void wrap_send (byte * b, word len, word rmt) { // assumed perfect buffers & lengths
	if (rmt) {
		memset (b, 0, sizeof(msgRpcAckType));
		in_header(b, msg_type)	= msg_rpcAck;
		in_header(b, rcv) 		= rmt;
		in_header(b, hco)		= 1;
		in_header(b, prox)		= 1;
		b[sizeof(msgRpcAckType)] = (byte)len;
		talk ((char *)b, len +sizeof(msgRpcAckType)+1, TO_NET);
		ufree (b);
	} else {
		b[0] = len +_OSS_FR_LEN;
		b[1] = 0;
		b[2] = (byte)local_host;
		b[3] = (byte)(local_host >> 8);
		_oss_out ((char *)b, NO);
	}
}

#define _CONST_LEN 21
static void stats (word rmt) {
	word mmin, mem;
	byte * b = (byte *)get_mem (rmt ? sizeof(msgRpcAckType) + _CONST_LEN +1: _CONST_LEN +_OSS_FR_LEN +1, NO);
	word ind = rmt ? sizeof(msgRpcAckType) +1: _OSS_FR_LEN +1;

	if (b == NULL)
		return;

	mem = memfree(0, &mmin);

	b[ind++]  = 0xFF;
	b[ind++]  = REP_LOG;
	b[ind++]  = D_INFO;
	b[ind++]  = 0;
	b[ind++]  = 0x81;
	b[ind++]  = SYSVER_min;
	b[ind++] = SYSVER_MAJ;
	b[ind++] = 0xC1;
	b[ind++] = (byte)master_host;
	b[ind++] = (byte)(master_host >> 8);
	b[ind++] = 0xC2;
	b[ind++] = (byte)seconds();
	b[ind++] = (byte)(seconds() >> 8);
	b[ind++] = (byte)(seconds() >> 16);
	b[ind++] = (byte)(seconds() >> 24);
	b[ind++] = 0xC1;
	b[ind++] = (byte)mem;
	b[ind++] = (byte)(mem >> 8);
	b[ind++] = 0xC1;
	b[ind++] = (byte)mmin;
	b[ind] = (byte)(mmin >> 8);
	
	wrap_send (b, _CONST_LEN, rmt);
}
#undef _CONST_LEN

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
#define _CONST_LEN 5
static void nop_resp (byte opco, byte opre, byte oprc, word rmt) {
	byte * b;
	word ind = rmt ? sizeof(msgRpcAckType) +1: _OSS_FR_LEN +1;
		
	if (!(opre & 0x80)) { // no 'op ack' was requested
		if (oprc != RC_OK)
			app_diag_S ("silent opco %x oprc %x", opco, oprc);
		return;
	}

	if ((b = (byte *)get_mem (rmt ? sizeof(msgRpcAckType) + _CONST_LEN +1: _CONST_LEN +_OSS_FR_LEN +1, NO)) == NULL) {
		app_diag_S ("nop_resp mem %u", opco);
		return;
	}
	
	b [ind++] = opco;
	b [ind++] = opre;
	b [ind++] = oprc;
	b [ind++] = (byte)local_host;
	b [ind] = (byte)(local_host >> 8);

	wrap_send (b, _CONST_LEN, rmt);
	
}
#undef _CONST_LEN

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
										2, // PAR_PMOD 0x10
										2, // PAR_TARP_RSS
										2, // PAR_RFCHAN
										0, 0, 0, 0, 0, 0, 0,
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
		case PAR_PMOD:
			*ptr = pegfl.peg_mod;
			break;
		case PAR_TARP_RSS:
			*ptr = tarp_ctrl.rssi_th;
			break;
		case PAR_RFCHAN:
			*ptr = net_opt (PHYSOPT_GETCHANNEL, NULL);
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
			*ptr = ((byte)snifcio.nid_opt << 4) | snifcio.rep_opt;
			break;
		default:
			return RC_EPAR;
	}
	return RC_OK;
}

fsm mbeacon;
fsm looper;
fsm locaudit;

// *ptr (pcode) assumed reasonably checked (nonzero length, within pcode space)
static byte set_param (byte * ptr, word rmt) {
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
			set_tarp_fwd(*(ptr+1));
			break;

		case PAR_TARP_S:
			if (pegfl.peg_mod == PMOD_REG) { // must be in a special mode (conf, cust)
				rc = RC_ERES;
				break;
			}
			set_tarp_slack(*(ptr+1));
			break;

		case PAR_TARP_R:
			if (pegfl.peg_mod == PMOD_REG) { // must be in a special mode (conf, cust)
				rc = RC_ERES;
				break;
			}
			set_tarp_rte_rec(*(ptr+1));
			break;

		case PAR_TARP_L:
			if (pegfl.peg_mod == PMOD_REG) { // must be in a special mode (conf, cust)
				rc = RC_ERES;
				break;
			}
			set_tarp_level(*(ptr+1));
			break;

		// 'drop weak' is rather exotic; if really in need, use PAR_TARP
		case PAR_TARP:
			if (pegfl.peg_mod == PMOD_REG) { // must be in a special mode (conf, cust)
				rc = RC_ERES;
				break;
			}
			tarp_ctrl.param = *(ptr+1);
			break;

		case PAR_TARP_RSS:
			if (pegfl.peg_mod == PMOD_REG) { // must be in a special mode (conf, cust)
				rc = RC_ERES;
				break;
			}
			tarp_ctrl.rssi_th = *(ptr+1);
			break;

		case PAR_RFCHAN:
			if (pegfl.peg_mod == PMOD_REG) { // must be in a special mode (conf, cust)
				rc = RC_ERES;
				break;
			}
			w = *(ptr+1);
			net_opt (PHYSOPT_SETCHANNEL, &w);
			break;

		case PAR_PMOD:
			if (rmt && *(ptr+1) > PMOD_CONF) {
				rc = RC_ERES;
				break;
			}
			if (pegfl.peg_mod == *(ptr+1))
				break;
			if (pegfl.peg_mod == PMOD_EXCC)
				reset();
			pegfl.peg_mod = *(ptr+1);
			if (pegfl.peg_mod == PMOD_EXCC) {
				if (running (mbeacon)) {
					killall (mbeacon);
					leds (MASTER_STATUS_LED, 0);
				}
				if (running (looper))
					killall (looper);
				if (running (locaudit))
					killall (locaudit);
				reset_tags();
				tagList.block = YES;
				set_tarp_fwd(0);
			}
			break;

		case PAR_SNIFF:
			if (rmt || pegfl.peg_mod == PMOD_REG) { // must be in a special mode (conf, cust)
				rc = RC_ERES;
				break;
			}
			if (sniffer_ctrl (*(ptr +1) >>4, *(ptr+1) & 0x0f))
				rc = RC_EVAL;
			break;
			
		case PAR_NID:
			if (pegfl.peg_mod == PMOD_REG) { // must be in a special mode (conf, cust)
				rc = RC_ERES;
				break;
			}
			net_id = get_word (ptr, 1);
			net_opt (PHYSOPT_SETSID, &net_id);
			break;
			
		case PAR_MID:
		case PAR_TAG_MGR:
		case PAR_AUDIT:
		case PAR_BEAC:
			rc = RC_ENIMP;
			break;

		default:
			rc = RC_EPAR;
	}
	return rc;
}

// confusing: _OSS_FR_LEN - frame head (for out only), 4 - request op fields, 5 - response op fields
static void cmd_get (byte * buf, word len, word rmt) {

	byte * bend = buf + len; // 1st out
	byte * ptr = buf +4;	 // 1st param
	byte * obu;
	byte oprc = RC_OK;
	byte reslen = 5;		// + 5 op_ fields, not counting params yet
	word ind = rmt ? sizeof(msgRpcAckType) +1: _OSS_FR_LEN +1;

	// calculate response buffer size
	while (ptr < bend) {
		if (*ptr >= PAR_CODE_SIZE || par_len[*ptr] == 0) {
			oprc = RC_EPAR;
			ptr++;
			continue;
		}

		reslen += par_len[*ptr++];
		
		// limit rmt response
		if (rmt && reslen + ind > MAX_PLEN) {
			bend = ptr;
			reslen -= par_len[*(ptr -1)];
			oprc = RC_ELEN;
		}
	}
	
	if ((obu = (byte *)get_mem (ind + reslen, NO)) == NULL) {
		app_diag_S ("get no mem %u", ind + reslen);
		return;
	}

	obu [ind++] = CMD_GET;
	obu [ind++] = buf [1];	// op_ref
	obu [ind++] = oprc;
	obu [ind++] = (word)local_host;
	obu [ind++] = (word)(local_host >>8);
	
	ptr = buf +4;
	
	while (ptr < bend) {
		if (*ptr >= PAR_CODE_SIZE || par_len[*ptr] == 0) {
			ptr++;
			continue;
		}
		
		// load a single param
		obu[ind] = *ptr;		 				// symbol
		(void)get_param (&obu[ind +1], *ptr);	 	// value
		ind += par_len [*ptr++];
		
	}
	wrap_send (obu, reslen, rmt);

}

// Other options could be implemented, we've chosen this: we set params from the list, until the 1st failure.
// Then, if op_rc has high bit set, we return with op_rc. We never return data (param values).
static void cmd_set (byte * buf, word len, word rmt) {
	byte * bend = buf + len; // 1st out the buf
	byte * ptr = buf +4;
	byte oprc = RC_OK;
	
	while (oprc == RC_OK && ptr < bend) {
		if (*ptr >= PAR_CODE_SIZE || par_len[*ptr] == 0) {
			oprc = RC_EPAR;
		} else {
			oprc = set_param (ptr, rmt);
			ptr += par_len [*ptr];
		}
	}

	nop_resp (buf[0], buf[1], oprc, rmt);
}

static byte check_ass (byte * buf, word len) {

	if (len < 5 || ((len - 5) % 3))	// len-5 is the length of series of (id, bitmap)
		return RC_ELEN;

	// no bcast
	if (get_word (buf, 2) != local_host)
		return RC_EADDR;

	return RC_OK;
}

static void cmd_set_ass (byte * buf, word len, word rmt) {
	byte rc = check_ass (buf, len);

	if (rc == RC_OK)
		b2treg ((len-5) /3, buf +4);

	nop_resp (buf [0], buf [1], rc, rmt);

}

static void cmd_get_ass (byte * buf, word len, word rmt) {
	word m;
	byte * b;
	byte rc, ind;

	if ((rc = check_ass (buf, len)) != RC_OK) {
		nop_resp (buf [0], buf [1], rc, rmt);
		return;
	}
	
	m = slice_treg ((word)buf[4]); // m: how many
	if (rmt) {
		if (m > (len = (MAX_PLEN - sizeof(msgRpcAckType) -1-5-1) /3)) { // now max is 12
			app_diag_W ("getass short %u->%u", m, len);
			m = len;
			rc = RC_ELEN;
		}
	}
	
	len = 5+1 + (m ? 3*m : 1);
	ind = 1+ (rmt ? sizeof(msgRpcAckType) : _OSS_FR_LEN);
	if ((b = (byte *)get_mem (ind + len, NO)) == NULL) {
		app_diag_S ("getass mem %u", ind + len);
		return;
	}

	b [ind++] = CMD_GET_ASSOC;
	b [ind++] = buf [1];  // op_ref
	b [ind++] = rc;
	b [ind++] = (word)local_host;
	b [ind++] = (word)(local_host >>8);
	b [ind] = buf [4]; // index
	if (m)
		treg2b (&b[ind], m);
	else
		b[++ind] = pegfl.learn_mod;
		
	wrap_send (b, len, rmt);
}
	
static void cmd_clr_ass (byte * buf, word len, word rmt) {
	byte rc = check_ass (buf, len);

	if (rc == RC_OK)
		reset_treg();
		
	nop_resp (buf [0], buf [1], rc, rmt);
}	

static void cmd_relay (byte * buf, word len, word rmt) {
	byte * b;
	word w;
	
	// at least 1 byte to send and no more than (arbitrary) 40
	if (len < 4 +2 +1 || len > 4 +2 +40) {
		nop_resp (buf [0], buf [1], RC_ELEN, rmt);
		return;
	}
	
	// no bcast
	if (get_word (buf, 2) != local_host) {
		nop_resp (buf [0], buf [1], RC_EADDR, rmt);
		return;
	}

	w = len + sizeof(msgFwdType) - 6;
	if ((b = (byte *)get_mem (w, NO)) == NULL) {
		nop_resp (buf [0], buf [1], RC_ERES, rmt);
		return;
	}
	
	memset (b, 0, w);
	in_header(b, msg_type) = msg_fwd;
	in_header(b, rcv) = get_word (buf, 4);
	in_fwd(b, optyp) = buf[0];
	in_fwd(b, opref) = buf[1];
	in_fwd(b, len) = w - sizeof(msgFwdType);
	memcpy ((char *)(b+sizeof(msgFwdType)), (char *)buf +6, in_fwd(b, len));
	talk ((char *)b, w, TO_NET);
	// show in emul for tests in vuee
	app_diag_U ("fwd %x to %u #%u", in_fwd(b, optyp), in_header(b, rcv), in_fwd(b, opref));
	ufree (b);
	// note that likely we should NOT respond, fwdAck will if requested
	// nop_resp (buf [0], buf [1], RC_OK, rmt);
}

static void cmd_nhood (byte * buf, word len, word rmt) {
	byte *b;
	byte rc = RC_OK;
	
	if (len != 4+4) {
		rc = RC_ELEN;
		goto Resp;
	}
	
	// no bcast
	if (get_word (buf, 2) != local_host) {
		rc = RC_EADDR;
		goto Resp;
	}
	
	if ((b = (byte *)get_mem (sizeof(msgNhType), NO)) == NULL) {
		rc = RC_ERES;
		goto Resp;
	}

	memset (b, 0, sizeof(msgNhType));
	in_header(b, msg_type) = msg_nh;
	in_header(b, rcv) = get_word (buf, 4);
	
	if (buf[7]) // nonzero means # of hops
		in_header(b, hco) = buf[7];
	else
		in_header(b, prox) = 1;
	
	in_nh(b, rsvp) = rmt ? rmt : local_host;
	in_nh(b, ref) = buf[6];

	talk ((char *)b, sizeof(msgNhType), TO_NET);
	// show in emul for tests in vuee
	app_diag_U ("nh to %u(%u.%u) rsvp %u %u", in_header(b, rcv), in_header(b, hco), in_header(b, prox), in_nh(b, rsvp), in_nh(b, ref));
	ufree (b);
	
Resp:
	nop_resp (buf [0], buf [1], rc, rmt);

}

static void send_rpc (byte * buf, word len, word rmt) {
	byte * b;

	if (pegfl.peg_mod < PMOD_CUST) {
		nop_resp (buf [0], buf [1], RC_ERES, 0);
		return;
	}
	if (len > MAX_PLEN - sizeof(msgRpcType) -1) { // packet: msgRpcType (now just the header)+1B_with_len+len
		nop_resp (buf [0], buf [1], RC_ELEN, 0);
		return;
	}
	if ((b = (byte *)get_mem (len +sizeof(msgRpcType) +1, NO)) == NULL) {
		nop_resp (buf [0], buf [1], RC_ERES, 0);
		return;
	}
	memset (b, 0, sizeof(msgRpcType));
	in_header(b, msg_type)	= msg_rpc;
	in_header(b, rcv) 		= rmt;
	in_header(b, hco)		= 1;
	in_header(b, prox)		= 1;
	b[sizeof(msgRpcType)] = len;
	memcpy (b +sizeof(msgRpcType) +1, buf, len);
	talk ((char *)b, len +sizeof(msgRpcType) +1, TO_NET);
	ufree (b);
}

static void process_cmd_in (byte * buf, word len, word rmt) {

	switch (buf[0]) { // op_code
		case CMD_GET:
			cmd_get (buf, len, rmt);
			break;
		case CMD_SET:
			cmd_set (buf, len, rmt);
			break;
		case CMD_SET_ASSOC:
			cmd_set_ass (buf, len, rmt);
			break;			
		case CMD_GET_ASSOC:
			cmd_get_ass (buf, len, rmt);
			break;
		case CMD_CLR_ASSOC:
			cmd_clr_ass (buf, len, rmt);
			break;
		case CMD_RELAY_41:
		case CMD_RELAY_42:
		case CMD_RELAY_43:
			cmd_relay (buf, len, rmt);
			break;
		case CMD_NHOOD:
			cmd_nhood (buf, len, rmt);
			break;
		case CMD_RESET:
			reset();
		default:
			nop_resp (buf [0], buf [1], RC_ENIMP, rmt);
			app_diag_W ("process_cmd_in not yet");
	}
}

static byte volt_to_byte (word sen) {
//
// Convert voltage reading into an 8-bit value
//
	if ((sen = (sen - 1000) >> 3) > 255)
		return 255;
	return (byte) sen;
}

fsm cmd_in {

	state START:
		stats (0);

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

		// ack the frame (perhaps it is premature but for now it'll do)
		if (ib[0] & 0x80)
			sack (ib[0], ossi.lin_stat);
			
		if (ossi.lin_stat != RC_OK)	// we won't go any further
			goto goLisn;

		// we're done with the frame, entering cmd processing
		// we assume FG_ACKR was served (may be a bad assumption, we'll see)
		
		// bcast could be both local and remote, but it causes double responses, annoying
		// and bound to confuse Renesas et al. So, no bcast. remote = 0 goes locally, as before.
		w = get_word ((byte *)ib, 5);
		if (w != 0 && w != local_host)
			send_rpc ((byte *)(ib +3), plen -3, w);
		else
		// if (w == 0 || w == local_host)
			// in Feb 2015 the frame passed to processing was stripped of the 3 head bytes, watch out for moronic mistakes
			process_cmd_in ((byte *)(ib +3), plen -3, 0);

goLisn:	tcv_endp ((address)ib);
		proceed LISN;
}
///////////////////////////////

#define _ppp	(p + sizeof(pongDataType))
static void board_out (char * p, char * b) {

	// for boards other tham ap319, 320, 331: just for fun
	switch (((pongDataType *)p)->btyp) {

		case BTYPE_ALPHANET_CHRONOS:
		case BTYPE_ALPHANET_CHRONOS_WHITE:
			b[11] = volt_to_byte (((pongPloadType0 *)_ppp)->volt);
			b[16] = (byte)((pongPloadType0 *)_ppp)->move_ago; // vy not
			b[17] = (byte)((pongPloadType0 *)_ppp)->move_nr;
			b[18] = b[19] = b[20] = 0;
			break;

		case BTYPE_AT_BASE:
			b[11] = volt_to_byte (((pongPloadType2 *)_ppp)->volt);
			b[16] = 9;
			b[17] = 14;
			b[18] = b[19] = b[20] = 0;
			break;
			
		case BTYPE_AT_BUT6:
		case BTYPE_AT_BUT6_1_0:
			b[16] |= ((pongPloadType3 *)_ppp)->glob;
			b[11] = volt_to_byte (((pongPloadType3 *)_ppp)->volt);
			b[17] = ((pongPloadType3 *)_ppp)->dial;
			b[18] = b[19] = b[20] = 0;
			break;

		case BTYPE_AT_BUT1:
		case BTYPE_AT_BUT1_1_0:
			b[16] |= 1;
			b[11] = volt_to_byte (((pongPloadType4 *)_ppp)->volt);
			b[17] = b[18] = b[19] = b[20] = 0;
			break;
			
		case BTYPE_WARSAW:
			b[16] = (byte)((pongPloadType5 *)_ppp)->random_shit;
			b[11] = volt_to_byte (((pongPloadType5 *)_ppp)->volt);
			b[17] = (byte)((pongPloadType5 *)_ppp)->steady_shit;
			b[18] = b[19] = b[20] = 0;
			break;

		case BTYPE_AT_LOOP:
			b[16] |= 1;
			b[11] = volt_to_byte (((pongPloadType6 *)_ppp)->volt);
			memcpy (&b[17], (byte *)(((pongPloadType6 *)_ppp)->loop), 4);
			break;

		default:
			app_diag_W ("btyp %u", ((pongDataType *)p)->btyp);
	}
}
#undef _ppp

// RPC basically kills the notion of non-uniform OSSIs unless we're prepared to translate their structs or have a yet another common abstraction, which
// seems ridiculous. So, we do RPC here, in TCVE only. If msg_rpc, msg_rpcAck are needed in other OSSIs, it may be implemented there, but with this
// approach, the commands can NOT be sent from OSSI(a) to a node with OSSI(b). Well, soon we'll get rid of multiple OSSIs (and half of the code here).
static Boolean valid_rpc (byte * b, word len, word rmt) {
	// I don't know how to do it with less additional crap. Crap:
	if (pegfl.peg_mod == PMOD_REG && b[0] == CMD_SET && len >= 6 && b[4] == PAR_PMOD && b[5] == PMOD_CONF) {
		pegfl.peg_mod = PMOD_CONF;
		pegfl.conf_id = rmt & PMOD_MASK;
	}
	
	if (pegfl.peg_mod != PMOD_CONF)
		return NO;
		
	// let's make ALL attempts suspicious
	if ((rmt & PMOD_MASK) != pegfl.conf_id) {
		app_diag_S ("rpc conf_id %u", rmt);
		pegfl.conf_id = 0;
		pegfl.peg_mod = PMOD_REG;
		return NO;
	}
	return YES;
}

// this is the main out on the oss i/f, I don't know at all if this makes
// more sense than no generic i/f at all
// NO parameter sanitization
void oss_tx (char * b, word siz) {
	char *bu;
	word varsiz;

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
				byte len = 21 (malloc 21 +1) or +2+LOCAVEC_SIZ with loca data, or +2+LOCASHORT_SIZ
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
				byte	board-spec[4]; e.g. dial, loop id
				byte	locat; // 1 - full location data, 2 - short loca
				// locat != LOCA_NONE only
				// variant LOCA_FULL
				word	ref;
				byte	locavec[LOCAVEC_SIZ];
				// variant LOCA_SHORT
				word	ref;
				byte	locavec[LOCASHORT_SIZ];				
			*/

#define LOCALATA (((pongDataType *)(b + sizeof(msgReportType)))->locat)
			varsiz = 21;
			if (LOCALATA == LOCA_FULL)
				varsiz += 2 + LOCAVEC_SIZ;
			if (LOCALATA == LOCA_SHORT)
				varsiz += 2 + LOCASHORT_SIZ;
			if ((bu = get_mem (varsiz +1, NO)) == NULL)
				return;
			
			bu[0] = varsiz;
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
			// with AP331, this board-specific insert is extended to 4 bytes b[17..20]
			board_out (b + sizeof(msgReportType), bu);
			bu[21] = LOCALATA;

			if (LOCALATA == LOCA_FULL) {	// loca ref & full vector
				bu[22] = (byte)in_report(b, ref);
				bu[23] = (byte)(in_report(b, ref) >> 8);
				memcpy (&bu[24], b + siz - LOCAVEC_SIZ, LOCAVEC_SIZ);
			} else if (LOCALATA == LOCA_SHORT) {	// loca ref & short vector & loop[4]
				bu[22] = (byte)in_report(b, ref);
				bu[23] = (byte)(in_report(b, ref) >> 8);
				memcpy (&bu[24], b + siz - LOCASHORT_SIZ, LOCASHORT_SIZ);
			}
			_oss_out (bu, NO);
			break;
#undef LOCALATA

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
			nop_resp (in_fwdAck(b, optyp), in_fwdAck(b, opref), RC_OK, 0);
			app_diag_U ("fwdAck #%u fr%u", in_fwdAck(b, opref), in_header(b, snd));
			break;

		case msg_loca:
			/*
				byte len = 11+LOCAVEC_SIZ (malloc 11+LOCAVEC_SIZ +1)
				// out:
				byte	seq;
				word	local_host;
				byte	efef;
				byte	reptype;
				word	pegid;
				word	tagid;
				word	ref;
				byte	vec[LOCAVEC_SIZ];
			*/			
			if ((bu = get_mem (12 +LOCAVEC_SIZ, NO)) == NULL)
				return;
			
			bu[0] = 11 +LOCAVEC_SIZ;
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
			memcpy (&bu[12], in_loca(b, vec), LOCAVEC_SIZ);
			_oss_out (bu, NO);
			break;
			
		case msg_rpc:

			if (valid_rpc ((byte *)(b +sizeof(msgRpcType) +1), (word)b[sizeof(msgRpcType)] /*siz - sizeof(msgRpcType)*/, in_header(b, snd)))
				process_cmd_in ((byte *)(b +sizeof(msgRpcType) +1), (word)b[sizeof(msgRpcType)] /*siz - sizeof(msgRpcType)*/, in_header(b, snd));
			else
				// respond with ERES, if the node is not in CONF state
				nop_resp (b[sizeof(msgRpcType) +1], b[sizeof(msgRpcType) +2], RC_ERES, in_header(b, snd));
				// should we stay silent, if the node is not in CONF state?
				// app_diag_S ("rpc? %u", in_header(b, snd));
			break;
			
		case msg_rpcAck:
			if (pegfl.peg_mod < PMOD_CUST)
				app_diag_S ("rpcAck? %u", in_header(b, snd));
				
			else { // put in frame and pass to OSS

				if ((bu = get_mem (1+3+ b[sizeof(msgRpcAckType)], NO)) == NULL)
					return;
					
				bu[0] = 3 + b[sizeof(msgRpcAckType)] /*siz +3-sizeof(msgRpcAckType)]*/;
				bu[1] = 0;
				bu[2] = (byte)local_host;
				bu[3] = (byte)(local_host >> 8);
				memcpy (&bu[4], b+sizeof(msgRpcAckType) +1, b[sizeof(msgRpcAckType)]);
				_oss_out (bu, NO);
			}
			break;
			
		case msg_nhAck:
			/*
				byte len = 11 (malloc 11 +1)
				// out:
				byte	seq;
				word	local_host;
				byte	efef;
				byte	reptype;
				word	nodeid;
				byte	ref;
				byte	hoc;
				byte	rssf;
				byte	rssb;

			*/			

			if ((bu = get_mem (12, NO)) == NULL)
				return;
			bu[0] = 11;
			bu[1] = 0;
			bu[2] = (byte)local_host;
			bu[3] = (byte)(local_host >> 8);
			bu[4] = 0xFF;
			bu[5] = REP_NHOOD;
			bu[6] = (byte)(in_header(b, snd));
			bu[7] = (byte)(in_header(b, snd) >> 8);
			bu[8] = in_nhAck(b, ref);
			bu[9] = in_header(b, hoc);
			bu[10] = in_nhAck(b, rss);
			bu[11] = in_header(b, seq_no); // cheated rssi on the rcv
			_oss_out (bu, NO);
			break;

		case msg_rfid:
			/*
				byte len = 13+in_rfid(b,len) (malloc this +1)
				// out:
				byte	seq;
				word	local_host;
				byte	efef;
				byte	reptype;
				word	nodeid;
				byte	rssi;
				byte	btyp;
				byte	ttyp;
				byte	next;
				byte	cnt;
				byte	len;
				len bytes follow
			*/			

			if ((bu = get_mem (14 + in_rfid(b, len), NO)) == NULL)
				return;
			bu[0] = 13 + in_rfid(b, len);
			bu[1] = 0;
			bu[2] = (byte)local_host;
			bu[3] = (byte)(local_host >> 8);
			bu[4] = 0xFF;
			bu[5] = REP_RFID;
			bu[6] = (byte)(in_header(b, snd));
			bu[7] = (byte)(in_header(b, snd) >> 8);
			bu[8] = in_header(b, seq_no); // cheated rssi on the rcv
			bu[9] = in_rfid(b, btyp);
			bu[10] = in_rfid(b, ttyp);
			bu[11] = in_rfid(b, next);
			bu[12] = in_rfid(b, cnt);
			bu[13] = in_rfid(b, len);
			memcpy (&bu[14], b + sizeof(msgRfidType), in_rfid(b, len));
			_oss_out (bu, NO);
			break;
			
		case msg_sniff:
			/* take over from sniffer.cc; note that this is the only instance (so far) that we get
			   the b(uffer) already allocated, ready for fifek
			b[0] msg_sniff 		-> to become len
			b[1] len			-> seq (0)
			b[2], b[3] 			-> local_host
			b[4] 				-> efef
			b[5] 				-> reptype
			b[6] complete packet starts - it includes sid at head and entropy, rssi at tail
			*/
			b[0] = b[1];
			b[1] = 0;
			b[2] = (byte)local_host;
			b[3] = (byte)(local_host >> 8);
			b[4] = 0xff;
			b[5] = REP_SNIFF;
			_oss_out (b, NO);
			break;

		default:
			app_diag_S ("unfinished? %x %u", *(address)b, siz);
	}
}

#undef _OSS_FR_LEN
