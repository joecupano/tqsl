/***************************************************************************
                          load_cert.cpp  -  description
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

#include <cstdlib>
#include <iostream>
#include "tqsllib.h"

using std::string;
using std::ios;
using std::cerr;
using std::cout;
using std::endl;

int
cb(int, const char *msg, void *) {
	cout << msg << endl;
	return 0;
}

int
main(int argc, char *argv[]) {
	if (tqsl_init()) {
		cerr << tqsl_getErrorString() << endl;
		return EXIT_FAILURE;
	}
	for (int i = 1; i < argc; i++) {
		if (tqsl_importTQSLFile(argv[i], cb, 0)) {
			cerr << tqsl_getErrorString() << endl;
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
