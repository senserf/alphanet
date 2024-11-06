/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __tag_mgr_h
#define __tag_mgr_h

extern tagListType	tagList;

void reset_tags ();
word del_tag (word id, word ref, word dupeq, Boolean force);
void ins_tag (char * buf, word rssi);
Boolean report_tag (char * td);
Boolean is_global ( char * b);
Boolean needs_ack ( word id, char * b, word rss);

// word count_treg ();
void reset_treg ();
word upd_treg (word id, byte mask);
void b2treg (word l, byte * b);
void treg2b (byte *, word num);
Boolean in_treg (word id, byte but);
word slice_treg (word ind);

//+++ tag_mgr.cc

#endif
