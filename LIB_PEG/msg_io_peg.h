/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __msg_io_peg_h
#define __msg_io_peg_h

void msg_ping_in (char * buf, word rssi);
void msg_pong_in (char * buf, word rssi);
void msg_report_in (char * buf, word siz);
void msg_reportAck_in (char * buf);
void msg_fwd_in (char * buf, word siz);
void msg_master_in (char * buf);
void msg_nh_in (char * buf, word rssi);

//+++ msg_io_peg.cc

#endif
