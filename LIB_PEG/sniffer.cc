#include "sysio.h"
#include "tcvphys.h"
#include "tcvplug.h"
#include "sniffer.h"
#include "inout.h"
#include "commons.h"
#include "diag.h"

#define	SNIF_DBG	1

snif_t snifcio = {-1, 0, 1, 0}; // fd, nid_opt, rep_opt, spare bits

///////////// plugin callbacks ////////////////
static int tcv_ope_sniff (int phy, int fd, va_list plid) {
/*
 * Only one instance of the plugin is ever allowed
 */
	if (snifcio.fd < 0) {
		snifcio.fd = fd;
		return 0;
	}
	return ERROR;
}

static int tcv_clo_sniff (int phy, int fd) {

	if (snifcio.fd < 0)
		return ERROR;

	snifcio.fd = -1;
	return 0;
}

static int tcv_rcv_sniff (int phy, address p, int len, int *ses,
							     tcvadp_t *bounds) {
	char * c;

	if (phy != 0 || snifcio.fd < 0)	{	// assuming RF on phy 0
		return TCV_DSP_PASS;
	}
	
	/* this is weird and strongly against 'layering' but it is tempting;
	   no tcvp_new, prepare buffer for tcve output and call oss_tx.
	   spot / locaspot were meant as a blueprint for various hw and oss platforms,
	   but it evolved into quite uniform Alphanet. We should clone it elsewhere,
	   get rid of all ossis but tcve.
	   Note that all ad-ons present in net.c, e.g. encryption, are not here. This
	   is one big hack; sniffing is such by definition.
	*/
	// The session is open & we're on RF i/f
	
	// apply rep_opt
	if (snifcio.rep_opt == SNIF_REP_SAME && (*p == net_id || *p == 0) ||
			snifcio.rep_opt == SNIF_REP_ALL ||
			snifcio.rep_opt == SNIF_REP_OPPO && (*p != 0 && *p != net_id || p[3] != local_host && p[3] != 0)) {

		/*
		b[0] msg_sniff 		-> to become len
		b[1] len			-> seq (0)
		b[2], b[3] 			-> local_host
		b[4] 				-> efef
		b[5] 				-> reptype
		b[6] complete packet starts here, it includes sid at head and entropy, rssi at tail
		*/
		if ((c = get_mem (len +6, NO)) != NULL) {
			c[0] = msg_sniff;
			c[1] = len +5;
			memcpy (c +6, (char *)p, len);
			talk (c, len +6, TO_OSS);
		}
#if SNIF_DBG
		app_diag_U ("SNIFF REP %u %u", *p, p[3]);
#endif
	}

	if (snifcio.nid_opt == SNIF_NID_OPPO ||
			snifcio.nid_opt == SNIF_NID_ALL && *p != 0 && *p != net_id) {
#if SNIF_DBG
		app_diag_U ("SNIFF DROP %u %u", *p, p[3]);
#endif
		return TCV_DSP_DROP;
	}

#if SNIF_DBG
	app_diag_U ("SNIFF PASS %u %u", *p, p[3]);
#endif
	return TCV_DSP_PASS;
}

static int tcv_frm_sniff (address p, tcvadp_t *bounds) {

	return bounds->head = bounds->tail = 0;
}

static trueconst tcvplug_t plug_sniff =
		{ tcv_ope_sniff, tcv_clo_sniff, tcv_rcv_sniff, tcv_frm_sniff,
			NULL, NULL, NULL, 0x1001 };

///////////////////

word sniffer_ctrl (byte nid, byte rep) {
	
	word nonenet = WNONE;
#if SNIF_DBG
	word w;
#endif
	// just in case, check
	if (nid > SNIF_NID_OPPO || rep > SNIF_REP_OPPO)
		return 1;

	if (rep != SNIF_REP_VOID)
		snifcio.rep_opt = rep;

	if (nid == snifcio.nid_opt || nid == SNIF_NID_VOID) // no action triggered
		return 0;
	
	if (snifcio.nid_opt == SNIF_NID_VIRG) { // register the plug
		tcv_plug (TCV_MAX_PLUGS -1, &plug_sniff);
#if SNIF_DBG
		app_diag_U ("SNIF reg %u %u %d", nid, rep, snifcio.fd);
#endif
		snifcio.nid_opt = SNIF_NID_OFF;
	}
	
	if (snifcio.nid_opt == SNIF_NID_OFF && nid > SNIF_NID_OFF) {
		tcv_open (WNONE, 0, TCV_MAX_PLUGS -1);
#if SNIF_DBG
		app_diag_U ("SNIF open %u %u %d", nid, rep, snifcio.fd);
#endif
	} else if (nid == SNIF_NID_OFF && snifcio.nid_opt > SNIF_NID_OFF) {
#if SNIF_DBG
		app_diag_U ("SNIF close %u %u %d", nid, rep, snifcio.fd);
#endif
		tcv_close (WNONE, snifcio.fd);
	}
	snifcio.nid_opt = nid;

#if SNIF_DBG
	w = net_opt (PHYSOPT_GETSID, NULL);
#endif

	if (nid == SNIF_NID_ALL || nid == SNIF_NID_OPPO) // assuming empty setsid cheap and harmless
		net_opt (PHYSOPT_SETSID, &nonenet);
	else
		net_opt (PHYSOPT_SETSID, &net_id);

#if SNIF_DBG
	if (w != net_opt (PHYSOPT_GETSID, NULL))
		app_diag_U ("SNIF SID (%u->%u) %u %u %d", w, net_opt (PHYSOPT_GETSID, NULL), nid, rep, snifcio.fd);
#endif

	return 0;
}
#undef	SNIF_DBG
