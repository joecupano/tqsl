/***************************************************************************
              converter.c  -  "C" example program for signing a log
                             -------------------
    begin                : Sun Dec 15 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include "tqsllib.h"
#include "tqslerrno.h"
#include "tqslconvert.h"


int usage() {
	fprintf(stderr, "Usage: converter [-ac] station-location infile [outfile]\n");
	exit(EXIT_FAILURE);
}

void fail(tQSL_Converter conv) {
	char buf[40];
	const char *err = tqsl_getErrorString();
	int lineno;
	buf[0] = '\0';
	if (conv && !tqsl_getConverterLine(conv, &lineno)) // && lineno > 0)
		sprintf(buf, " on line %d", lineno);
	fprintf(stderr, "Aborted: %s%s\n", err, buf);
	if (conv) tqsl_converterRollBack(conv);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[]) {
	enum { UNKNOWN, CABRILLO, ADIF } type = UNKNOWN;
	int opt;
	tQSL_Location loc;
	tQSL_Converter conv = 0;
	int stat = 1;
	char call[256];
	int dxcc;
	tQSL_Cert *certs;
	int ncerts;
	int haveout = 0;
	const char *ofile;
	int out;

	while ((opt = getopt(argc, argv, "ca")) != -1) {
		switch (opt) {
			case 'c':
				type = CABRILLO;
				break;
			case 'a':
				type = ADIF;
				break;
			default:
				usage();
		}
	}
	if (argc - optind < 2)
		usage();
	if (tqsl_init())
		fail(conv);
	// Get the specified station location data
	if (tqsl_getStationLocation(&loc, argv[optind++]))
		fail(conv);
	// Get the callsign and DXCC entity to use
	if (tqsl_getLocationCallSign(loc, call, sizeof call))
		fail(conv);
	if (tqsl_getLocationDXCCEntity(loc, &dxcc))
		fail(conv);
	// Get a list of available signing certificates
	if (tqsl_selectCertificates(&certs, &ncerts, call, dxcc, 0, 0, 1))
		fail(conv);
	if (ncerts < 1) {
		fprintf(stderr, "No certificates available for %s\n", call);
		exit (EXIT_FAILURE);
	}
	if (type == UNKNOWN || type == CABRILLO) {
		if ((stat = tqsl_beginCabrilloConverter(&conv, argv[optind], certs, ncerts, loc)) != 0
			&& type == CABRILLO)
			fail(conv);
	}
	if (stat) {
		if (tqsl_beginADIFConverter(&conv, argv[optind], certs, ncerts, loc))
			fail(conv);
	}
	tqsl_setConverterAllowDuplicates(conv, 0);
	optind++;
	ofile = (optind < argc) ? argv[optind] : "converted.tq8";
	out = creat(ofile, 0660);
	if (out < 0) {
		fprintf(stderr, "Unable to open %s\n", ofile);
		exit (EXIT_FAILURE);
	}
	tqsl_setConverterAppName(conv, "converter-sample");
	do {
		const char *gabbi = tqsl_getConverterGABBI(conv);
		if (gabbi) {
			haveout = 1;
			write(out, gabbi, strlen(gabbi));
			continue;
		}
   		if (tQSL_Error == TQSL_SIGNINIT_ERROR) {
   			tQSL_Cert cert;
   			if (tqsl_getConverterCert(conv, &cert))
				fail(conv);
   			if (tqsl_beginSigning(cert, 0, 0, 0))
				fail(conv);
   			continue;
   		}
   		if (tQSL_Error == TQSL_DUPLICATE_QSO)
			continue;
		break;
	} while (1);
	close(out);
	if (tQSL_Error != TQSL_NO_ERROR)
		fail(conv);
	else if (!haveout)
		fprintf(stderr, "Empty log file\n");
	tqsl_converterCommit(conv);
	return EXIT_SUCCESS;
}
