/***************************************************************************
                          station_loc.cpp  -  description
                             -------------------
    begin                : Sat Dec 14 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include <string.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include "tqsllib.h"
#include "tqslexc.h"

using std::string;
using std::ios;
using std::cerr;
using std::cout;
using std::endl;

int
usage() {
	cerr << "Usage: station_loc callsign [dxcc]" << endl;
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[]) {
	try {
		string call, dxcc;
		if (argc < 2)
			usage();
		call = argv[1];
		if (argc > 2)
			dxcc = argv[2];
		if (tqsl_init())
			throw tqslexc();

		tQSL_Location loc;
		if (tqsl_initStationLocationCapture(&loc))
			throw tqslexc();
		if (tqsl_setStationLocationCapturePage(loc, 1))
			throw tqslexc();
		// We know that the first field of page 1 is always call and the 2nd is DXCC
		int nfield;
		tqsl_getNumLocationFieldListItems(loc, 0, &nfield);
		int i;
		for (i = 0; i < nfield; i++) {
			char buf[256];
			if (tqsl_getLocationFieldListItem(loc, 0, i, buf, sizeof buf))
				throw tqslexc();
			if (!strcasecmp(buf, call.c_str()))
				break;
		}
		if (i == nfield)
			throw myexc(string("Can't init station location for call = ") + call);
		if (tqsl_setLocationFieldIndex(loc, 0, i))
			throw tqslexc();
		if (tqsl_updateStationLocationCapture(loc))
			throw tqslexc();
		if (dxcc != "") {
			int nfield;
			tqsl_getNumLocationFieldListItems(loc, 1, &nfield);
//cerr << nfield << endl;
			for (i = 0; i < nfield; i++) {
				char buf[256];
				if (tqsl_setLocationFieldIndex(loc, 1, i))
					throw tqslexc();
				if (tqsl_getLocationFieldCharData(loc, 1, buf, sizeof buf))
					throw tqslexc();
//cerr << buf << endl;
				if (!strcasecmp(buf, dxcc.c_str()))
					break;
			}
			if (i == nfield)
				throw myexc(string("Can't init location for DXCC = ") + dxcc);
			if (tqsl_setLocationFieldIndex(loc, 1, i))
				throw tqslexc();
		}
		int dxcc_idx;
		if (tqsl_getLocationFieldIndex(loc, 1, &dxcc_idx))
			throw tqslexc();
		char buf[256];
		if (tqsl_getLocationFieldListItem(loc, 1, dxcc_idx, buf, sizeof buf))
			throw tqslexc();
		string lname = call + "_auto";
		if (tqsl_setStationLocationCaptureName(loc, lname.c_str()))
			throw tqslexc();
		if (tqsl_saveStationLocationCapture(loc, 1))
			throw tqslexc();
		tqsl_endStationLocationCapture(&loc);
		cout << "Wrote station location for " << call << " - " << buf << endl;
	} catch(exception& x) {
		cerr << "Aborted: " << x.what() << endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
