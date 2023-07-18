/***************************************************************************
                          dumptqsldata.c  -  description
                             -------------------
    begin                : Mon Mar 3 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

/* Dumps the config data from the TQSL library */

#include <stdio.h>
#include <stdlib.h>
#include "tqsllib.h"

void
errchk(int stat) {
	if (stat) {
		printf("ERROR: %s\n", tqsl_getErrorString());
		exit(1);
	}
}

int
main() {
	int count, i;
	const char *cp1, *cp2;
	tQSL_Date start, end;
	int low, high;
	char buf1[20], buf2[20];

	errchk(tqsl_init());
	puts("===== MODES =====\n   Mode       Group");
	errchk(tqsl_getNumMode(&count));
	for (i = 0; i < count; i++) {
		errchk(tqsl_getMode(i, &cp1, &cp2));
		printf("   %-10.10s %s\n", cp1, cp2);
	}
	puts("\n===== BANDS =====\n   Band       Spectrum  Low      High");
	errchk(tqsl_getNumBand(&count));
	for (i = 0; i < count; i++) {
		errchk(tqsl_getBand(i, &cp1, &cp2, &low, &high));
		printf("   %-10.10s %-8.8s  %-8d %d\n", cp1, cp2, low, high);
	}
	puts("\n===== DXCC =====\n   Entity  Name");
	errchk(tqsl_getNumDXCCEntity(&count));
	for (i = 0; i < count; i++) {
		errchk(tqsl_getDXCCEntity(i, &low, &cp1));
		printf("   %-6d  %s\n", low, cp1);
	}
	puts("\n===== PROP_MODES =====\n   Mode    Descrip");
	errchk(tqsl_getNumPropagationMode(&count));
	for (i = 0; i < count; i++) {
		errchk(tqsl_getPropagationMode(i, &cp1, &cp2));
		printf("   %-6s  %s\n", cp1, cp2);
	}
	puts("\n===== SATELLITES =====\n   Sat     Start Date  End Date    Descrip");
	errchk(tqsl_getNumSatellite(&count));
	for (i = 0; i < count; i++) {
		errchk(tqsl_getSatellite(i, &cp1, &cp2, &start, &end));
		buf1[0] = buf2[0] = '\0';
		tqsl_convertDateToText(&start, buf1, sizeof buf1);
		tqsl_convertDateToText(&end, buf2, sizeof buf2);
		printf("   %-6s  %-10s  %-10s  %s\n", cp1, buf1, buf2, cp2);
	}
	return 0;
}
