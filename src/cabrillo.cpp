/***************************************************************************
                          cabrillo.cpp  -  description
                             -------------------
    begin                : Thu Dec 5 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/


#define TQSLLIB_DEF

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include "tqsllib.h"
#include "tqslerrno.h"

#include "winstrdefs.h"

using std::map;
using std::string;

#define TQSL_CABRILLO_MAX_RECORD_LENGTH 120

DLLEXPORTDATA TQSL_CABRILLO_ERROR_TYPE tQSL_Cabrillo_Error;

static char errmsgbuf[256];
static char errmsgdata[128];

struct TQSL_CABRILLO;

static int freq_to_band(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp);
static int freq_to_mhz(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp);
static int mode_xlat(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp);
static int time_fixer(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp);

struct cabrillo_field_def {
	const char *name;
	int loc;
	int (*process)(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp);
};

static cabrillo_field_def cabrillo_dummy[] = {
	 { "CALL", 6, 0 },
	 { "BAND", 0, freq_to_band },
	 { "MODE", 1, mode_xlat },
	 { "QSO_DATE", 2, 0 },
	 { "TIME_ON", 3, time_fixer },
	 { "FREQ", 0, freq_to_mhz },
	 { "MYCALL", 4, 0 },
};

/*

// Cabrillo QSO template specs

// Call in field 6
static cabrillo_field_def cabrillo_c6[] = {
	{ "BAND", 0, freq_to_band },
	{ "MODE", 1, mode_xlat },
	{ "QSO_DATE", 2, 0 },
	{ "TIME_ON", 3, time_fixer },
	{ "CALL", 6, 0 },
	{ "FREQ", 0, 0 },
	{ "MYCALL", 4, 0 },
};

// Call in field 7
static cabrillo_field_def cabrillo_c7[] = {
	{ "BAND", 0, freq_to_band },
	{ "MODE", 1, mode_xlat },
	{ "QSO_DATE", 2, 0 },
	{ "TIME_ON", 3, time_fixer },
	{ "CALL", 7, 0 },
	{ "FREQ", 0, 0 },
	{ "MYCALL", 4, 0 },
};

// Call in field 8
static cabrillo_field_def cabrillo_c8[] = {
	{ "BAND", 0, freq_to_band },
	{ "MODE", 1, mode_xlat },
	{ "QSO_DATE", 2, 0 },
	{ "TIME_ON", 3, time_fixer },
	{ "CALL", 8, 0 },
	{ "FREQ", 0, 0 },
	{ "MYCALL", 4, 0 },
};

// Call in field 9
static cabrillo_field_def cabrillo_c9[] = {
	{ "BAND", 0, freq_to_band },
	{ "MODE", 1, mode_xlat },
	{ "QSO_DATE", 2, 0 },
	{ "TIME_ON", 3, time_fixer },
	{ "CALL", 9, 0 },
	{ "FREQ", 0, 0 },
	{ "MYCALL", 4, 0 },
};

*/

struct cabrillo_contest {
	char *contest_name;
	TQSL_CABRILLO_FREQ_TYPE type;
	cabrillo_field_def *fields;
	int nfields;
};

struct TQSL_CABRILLO {
	int sentinel;
	FILE *fp;
	char *filename;
	cabrillo_contest *contest;
	int field_idx;
	char rec[TQSL_CABRILLO_MAX_RECORD_LENGTH+1];
	char *datap;
	int line_no;
	char *fields[TQSL_CABRILLO_MAX_FIELDS];
};

#define CAST_TQSL_CABRILLO(p) ((struct TQSL_CABRILLO *)p)

static TQSL_CABRILLO *
check_cabrillo(tQSL_Cabrillo cabp) {
	if (tqsl_init())
		return 0;
	if (cabp == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 0;
	}
	if (CAST_TQSL_CABRILLO(cabp)->sentinel != 0x2449)
		return 0;
	return CAST_TQSL_CABRILLO(cabp);
}

static char *
tqsl_parse_cabrillo_record(char *rec) {
	char *cp = strchr(rec, ':');
	if (!cp)
		return 0;
	*cp++ = 0;
	if (strlen(rec) > TQSL_CABRILLO_FIELD_NAME_LENGTH_MAX)
		return 0;
	while (isspace(*cp))
		cp++;
	char *sp;
	if ((sp = strchr(cp, '\r')) != 0)
		*sp = '\0';
	if ((sp = strchr(cp, '\n')) != 0)
		*sp = '\0';
	for (sp = cp + strlen(cp); sp != cp; ) {
		sp--;
		if (isspace(*sp))
			*sp = '\0';
		else
			break;
	}
	for (sp = rec; *sp; sp++)
		*sp = toupper(*sp);
	return cp;
}

static int
freq_to_band(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp) {
	if (!strcasecmp(fp->value, "light")) {
		strncpy(fp->value, "SUBMM", sizeof fp->value);
		return 0;
	}
	int freq = strtol(fp->value, NULL, 10);
	const char *band = 0;
	if (freq < 30) {
		// Handle known CT misbehavior
		if (!strcmp(fp->value, "7"))
			freq = 7000;
		if (!strcmp(fp->value, "14"))
			freq = 14000;
		if (!strcmp(fp->value, "21"))
			freq = 21000;
		if (!strcmp(fp->value, "28"))
			freq = 28000;
	}
	if (freq >= 1800 && freq <= 2000)
		band = "160M";
	else if (freq >= 3500 && freq <= 4000)
		band = "80M";
	else if (freq >= 7000 && freq <= 7300)
		band = "40M";
	else if (freq >= 10100 && freq <= 10150)
		band = "30M";
	else if (freq >= 14000 && freq <= 14350)
		band = "20M";
	else if (freq >= 18068 && freq <= 18168)
		band = "17M";
	else if (freq >= 21000 && freq <= 21450)
		band = "15M";
	else if (freq >= 24890 && freq <= 24990)
		band = "12M";
	else if (freq >= 28000 && freq <= 29700)
		band = "10M";
	else if (freq == 50)
		band = "6M";
	else if (freq == 70)
		band = "4M";
	else if (freq == 144)
		band = "2M";
	else if (freq == 222)
		band = "1.25M";
	else if (freq == 432)
		band = "70CM";
	else if (freq == 902 || freq == 903)
		band = "33CM";
	else if (!strcasecmp(fp->value, "1.2G") || !strcasecmp(fp->value, "1.2"))
		band = "23CM";
	else if (!strcasecmp(fp->value, "2.3G") || !strcasecmp(fp->value, "2.3"))
		band = "13CM";
	else if (!strcasecmp(fp->value, "3.4G") || !strcasecmp(fp->value, "3.4"))
		band = "9CM";
	else if (!strcasecmp(fp->value, "5.7G") || !strcasecmp(fp->value, "5.7"))
		band = "6CM";
	else if (!strcasecmp(fp->value, "10G") || !strcasecmp(fp->value, "10"))
		band = "3CM";
	else if (!strcasecmp(fp->value, "24G") || !strcasecmp(fp->value, "24"))
		band = "1.25CM";
	else if (!strcasecmp(fp->value, "47G") || !strcasecmp(fp->value, "47"))
		band = "6MM";
	else if (!strcasecmp(fp->value, "75G") || !strcasecmp(fp->value, "75") ||
		 !strcasecmp(fp->value, "76G") || !strcasecmp(fp->value, "76"))
		band = "4MM";
	else if (!strcasecmp(fp->value, "119G") || !strcasecmp(fp->value, "119") ||
		 !strcasecmp(fp->value, "122G") || !strcasecmp(fp->value, "122") ||
		 !strcasecmp(fp->value, "123G") || !strcasecmp(fp->value, "123"))
		band = "2.5MM";
	else if (!strcasecmp(fp->value, "142G") || !strcasecmp(fp->value, "142") ||
		 !strcasecmp(fp->value, "134G") || !strcasecmp(fp->value, "134"))
		band = "2MM";
	else if (!strcasecmp(fp->value, "241G")  || !strcasecmp(fp->value, "241")||
		 !strcasecmp(fp->value, "242G") || !strcasecmp(fp->value, "242"))
		band = "1MM";
	else if (!strcasecmp(fp->value, "300G") || !strcasecmp(fp->value, "300") || !strcasecmp(fp->value, "LIGHT"))
		band = "SUBMM";

	if (band && cab->contest->type ==  TQSL_CABRILLO_UNKNOWN) {
		if (freq < 1000)
			cab->contest->type = TQSL_CABRILLO_VHF;
		else
			cab->contest->type = TQSL_CABRILLO_HF;
	}
	if (band == 0)
		return 1;
	strncpy(fp->value, band, sizeof fp->value);
	return 0;
}

static int
freq_to_mhz(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp) {
	if (!strcasecmp(fp->value, "light")) {
		return 0;
	}
	int freq = strtol(fp->value, NULL, 10);
	double freqmhz = freq;
	freqmhz /= 1000;

	if (freq < 30) {
		// Handle known CT misbehavior
		if (freq == 7)
			freqmhz = 7.0;
		if (freq == 14)
			freqmhz = 14.0;
		if (freq == 21)
			freqmhz = 21.0;
		if (freq == 28)
			freqmhz = 28.0;
	}
	// VHF+
	if (!strcasecmp(fp->value, "50"))
		freqmhz = 50.0;
	else if (!strcasecmp(fp->value, "70"))
		freqmhz = 70.0;
	else if (!strcasecmp(fp->value, "144"))
		freqmhz = 144.0;
	else if (!strcasecmp(fp->value, "222"))
		freqmhz = 222.0;
	else if (!strcasecmp(fp->value, "432"))
		freqmhz = 432.0;
	else if (!strcasecmp(fp->value, "902") ||
		 !strcasecmp(fp->value, "903"))
		freqmhz = 902.0;
	else if (!strcasecmp(fp->value, "1.2G") || !strcasecmp(fp->value, "1.2"))
		freqmhz = 1240.0;
	else if (!strcasecmp(fp->value, "2.3G") || !strcasecmp(fp->value, "2.3"))
		freqmhz = 2300.0;
	else if (!strcasecmp(fp->value, "3.4G") || !strcasecmp(fp->value, "3.4"))
		freqmhz = 3300.0;
	else if (!strcasecmp(fp->value, "5.7G") || !strcasecmp(fp->value, "5.7"))
		freqmhz = 5650.0;
	else if (!strcasecmp(fp->value, "10G")  || !strcasecmp(fp->value, "10"))
		freqmhz = 10000.0;
	else if (!strcasecmp(fp->value, "24G")  || !strcasecmp(fp->value, "24"))
		freqmhz = 24000.0;
	else if (!strcasecmp(fp->value, "47G")  || !strcasecmp(fp->value, "47"))
		freqmhz = 47000.0;
	else if (!strcasecmp(fp->value, "75G")  || !strcasecmp(fp->value, "75") ||
		 !strcasecmp(fp->value, "76G")  || !strcasecmp(fp->value, "76"))
		freqmhz = 75500.0;
	else if (!strcasecmp(fp->value, "119G") || !strcasecmp(fp->value, "119"))
		freqmhz = 119980.0;
	else if (!strcasecmp(fp->value, "142G") || !strcasecmp(fp->value, "142"))
		freqmhz = 142000.0;
	else if (!strcasecmp(fp->value, "241G") || !strcasecmp(fp->value, "241") ||
		 !strcasecmp(fp->value, "242G") || !strcasecmp(fp->value, "242"))
		freqmhz = 241000.0;
	else if (!strcasecmp(fp->value, "300G") || !strcasecmp(fp->value, "300"))
		freqmhz = 300000.0;

	if (freqmhz > 0 && cab->contest->type ==  TQSL_CABRILLO_UNKNOWN) {
		if (freqmhz >= 50.0)		// VHF
			cab->contest->type = TQSL_CABRILLO_VHF;
		else
			cab->contest->type = TQSL_CABRILLO_HF;
	}

	snprintf(fp->value, sizeof fp->value, "%#f", freqmhz);
	return 0;
}

static char *
tqsl_strtoupper(char *str) {
        for (char *cp = str; *cp != '\0'; cp++)
                *cp = toupper(*cp);
        return str;
}


/*
 * Read a line from a cabrillo file.
 * Start with stripping whitespace, read up until any line-ending character
 * (\r \n)
 * Replaces fgets which has a fixed line ending (\n).
 */

static char *fgets_cab(char *s, int size, FILE *stream) {
	int status;
	int ws;
	while (1) {
		ws = fgetc(stream);
		if (ws == EOF) {
			return 0;
		}
		if (!isspace(ws)) {
			break;
		}
	}
	ungetc(ws, stream);					// Push that back onto the stream
	char format[20];
	snprintf(format, sizeof format, "%%%d[^\r\n]", size - 1);	// Format allows what's left up to \r or \n
	status = fscanf(stream, format, s);
	if (status == 0)
		return NULL;
	s[size-1] = '\0';
	return s;
}

static int
mode_xlat(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp) {
	static map<string, string> modes;
	static bool modes_loaded = false;

	if (!modes_loaded) {
		tqslTrace("mode_xlat", "Loading Cab modes");
		modes_loaded = true;

		// Set default modes
		modes["CW"] = "CW";
		modes["PH"] = "SSB";
		modes["FM"] = "FM";
		modes["RY"] = "RTTY";
		modes["DG"] = "DATA";

		FILE *cfile;
		char modebuf[80];
#ifdef _WIN32
        	string default_path = string(tQSL_RsrcDir) + "\\cab_modes.dat";
        	string user_path = string(tQSL_BaseDir) + "\\cab_modes.dat";
#else
        	string default_path = string(tQSL_RsrcDir) + "/cab_modes.dat";
        	string user_path = string(tQSL_BaseDir) + "/cab_modes.dat";
#endif

#ifdef _WIN32
		wchar_t* wfilename = utf8_to_wchar(default_path.c_str());
		if ((cfile = _wfopen(wfilename, L"r")) == NULL) {
#else
		if ((cfile = fopen(default_path.c_str(), "r")) == NULL) {
#endif
			tqslTrace("mode_xlat", "Can't open def mode file %s: %m", default_path.c_str());
		} else {
			while(fgets(modebuf, sizeof modebuf, cfile)) {
				for (char *p = modebuf + (strlen(modebuf) - 1); p >= modebuf; p--) {
					if (*p == '\n' || *p == '\r') {
						*p = '\0';
					} else {
						break;
					}
				}
				char *mode = strtok(modebuf, ",");
				char *map = strtok(NULL, ",");
				if (mode && map)
					modes[tqsl_strtoupper(mode)] = tqsl_strtoupper(map);
			}
		}
#ifdef _WIN32
		free(wfilename);
#endif
#ifdef _WIN32
		wfilename = utf8_to_wchar(user_path.c_str());
		if ((cfile = _wfopen(wfilename, L"r")) == NULL) {
#else
		if ((cfile = fopen(user_path.c_str(), "r")) == NULL) {
#endif
			tqslTrace("mode_xlat", "Can't open user mode file %s: %m", user_path.c_str());
		} else {
			while(fgets(modebuf, sizeof modebuf, cfile)) {
				for (char *p = modebuf + (strlen(modebuf) - 1); p >= modebuf; p--) {
					if (*p == '\n' || *p == '\r') {
						*p = '\0';
					} else {
						break;
					}
				}
				char *mode = strtok(modebuf, ",");
				char *map = strtok(NULL, ",");
				if (mode && map)
					modes[tqsl_strtoupper(mode)] = tqsl_strtoupper(map);
			}
		}
	}

	map<string, string>::iterator it;
	it = modes.find(tqsl_strtoupper(fp->value));
	if (it != modes.end()) {
		strncpy(fp->value, it->second.c_str(), sizeof fp->value);
		return 0;
	}
	return 1;	// not found
}

static int
time_fixer(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp) {
	if (strlen(fp->value) == 0)
		return 0;
	char *cp;
	for (cp = fp->value; *cp; cp++)
		if (!isdigit(*cp))
			break;
	if (*cp)
		return 1;
	snprintf(fp->value, sizeof fp->value, "%04d", static_cast<int>(strtol(fp->value, NULL, 10)));
	return 0;
}

static void
tqsl_free_cabrillo_contest(struct cabrillo_contest *c) {
		if (c->contest_name)
			free(c->contest_name);
		if (c->fields)
			free(c->fields);
		free(c);
}

static struct cabrillo_contest *
tqsl_new_cabrillo_contest(const char *contest_name, int call_field, int contest_type) {
	cabrillo_contest *c = static_cast<cabrillo_contest *>(calloc(1, sizeof(struct cabrillo_contest)));
	if (c == NULL)
		return NULL;
	if ((c->contest_name = strdup(contest_name)) == NULL) {
		tqsl_free_cabrillo_contest(c);
		return NULL;
	}
	c->type = (TQSL_CABRILLO_FREQ_TYPE)contest_type;
	if ((c->fields = (struct cabrillo_field_def *)calloc(1, sizeof cabrillo_dummy)) == NULL) {
		tqsl_free_cabrillo_contest(c);
		return NULL;
	}
	memcpy(c->fields, cabrillo_dummy, sizeof cabrillo_dummy);
	c->fields[0].loc = call_field-1;
	c->nfields = sizeof cabrillo_dummy / sizeof cabrillo_dummy[0];
	return c;
}

static void
tqsl_free_cab(struct TQSL_CABRILLO *cab) {
	if (!cab || cab->sentinel != 0x2449)
		return;
	cab->sentinel = 0;
	if (cab->filename)
		free(cab->filename);
	if (cab->fp)
		fclose(cab->fp);
	if (cab->contest)
		tqsl_free_cabrillo_contest(cab->contest);
	free(cab);
}

DLLEXPORT int CALLCONVENTION
tqsl_beginCabrillo(tQSL_Cabrillo *cabp, const char *filename) {
	tqslTrace("tqsl_beginCabrillo", "cabp=0x%lx, filename=%s", cabp, filename);
	TQSL_CABRILLO_ERROR_TYPE terrno;
	if (filename == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	struct TQSL_CABRILLO *cab;
	cab = (struct TQSL_CABRILLO *)calloc(1, sizeof(struct TQSL_CABRILLO));
	if (cab == NULL) {
		tQSL_Error = TQSL_ALLOC_ERROR;
		goto err;
	}
	cab->sentinel = 0x2449;
	cab->field_idx = -1;
#ifdef _WIN32
	wchar_t * wfilename = utf8_to_wchar(filename);
	if ((cab->fp = _wfopen(wfilename, L"rb, ccs=UTF-8")) == NULL) {
		free_wchar(wfilename);
#else
	if ((cab->fp = fopen(filename, "r")) == NULL) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_beginCabrillo", "open error, errno=%d, error=%s", errno, strerror(errno));
		goto err;
	}
#ifdef _WIN32
	free(wfilename);
#endif
	char *cp;
	terrno = TQSL_CABRILLO_NO_START_RECORD;
	while ((cp = fgets_cab(cab->rec, sizeof cab->rec, cab->fp)) != 0) {
		cab->line_no++;
		if (tqsl_parse_cabrillo_record(cab->rec) != 0
			&& strstr(cab->rec, "START-OF-LOG"))
			break;
	}
	if (cp != 0) {
		terrno = TQSL_CABRILLO_NO_CONTEST_RECORD;
		while ((cp = fgets_cab(cab->rec, sizeof cab->rec, cab->fp)) != 0) {
			cab->line_no++;
			char *vp;
			if ((vp = tqsl_parse_cabrillo_record(cab->rec)) != 0
				&& !strcmp(cab->rec, "CONTEST")
				&& strtok(vp, " \t\r\n") != 0) {
				terrno = TQSL_CABRILLO_UNKNOWN_CONTEST;
				int callfield, contest_type;
				if (tqsl_getCabrilloMapEntry(vp, &callfield, &contest_type)) {
					// No defined contest with this name.
					// callfield comes back as 0
					contest_type = TQSL_CABRILLO_UNKNOWN;
				}
				cab->contest = tqsl_new_cabrillo_contest(vp, callfield, contest_type);
				if (cab->contest == 0) {
					strncpy(errmsgdata, vp, sizeof errmsgdata);
					cp = 0;
				}
				break;
			}
		}
	}
	if (cp == 0) {
		if (ferror(cab->fp)) {
			tQSL_Error = TQSL_SYSTEM_ERROR;
			tQSL_Errno = errno;
			tqslTrace("tqsl_beginCabrillo", "read error, errno=%d, error=%s", errno, strerror(errno));
			goto err;
		}
		tQSL_Cabrillo_Error = terrno;
		tQSL_Error = TQSL_CABRILLO_ERROR;
		goto err;
	}
	if ((cab->filename = strdup(filename)) == NULL) {
		tQSL_Error = TQSL_ALLOC_ERROR;
		goto err;
	}
	*((struct TQSL_CABRILLO **)cabp) = cab;
	return 0;
 err:
	strncpy(tQSL_ErrorFile, filename, sizeof tQSL_ErrorFile);
	tQSL_ErrorFile[sizeof tQSL_ErrorFile-1] = 0;
	tqsl_free_cab(cab);
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_endCabrillo(tQSL_Cabrillo *cabp) {
	tqslTrace("tqsl_endCabrillo", "cabp=0x%lx", cabp);
	TQSL_CABRILLO *cab;
	if (cabp == 0)
		return 0;
	cab = CAST_TQSL_CABRILLO(*cabp);
	if (!cab || cab->sentinel != 0x2449)
		return 0;
	tqsl_free_cab(cab);
	*cabp = 0;
	return 0;
}

DLLEXPORT const char* CALLCONVENTION
tqsl_cabrilloGetError(TQSL_CABRILLO_ERROR_TYPE err) {
	const char *msg = 0;
	switch (err) {
		case TQSL_CABRILLO_NO_ERROR:
			msg = "Cabrillo success";
			break;
		case TQSL_CABRILLO_EOF:
			msg = "Cabrillo end-of-file";
			break;
		case TQSL_CABRILLO_EOR:
			msg = "Cabrillo end-of-record";
			break;
		case TQSL_CABRILLO_NO_START_RECORD:
			msg = "Cabrillo missing START-OF-LOG record";
			break;
		case TQSL_CABRILLO_NO_CONTEST_RECORD:
			msg = "Cabrillo missing CONTEST record";
			break;
		case TQSL_CABRILLO_UNKNOWN_CONTEST:
			snprintf(errmsgbuf, sizeof errmsgbuf, "Cabrillo unknown CONTEST: %s", errmsgdata);
			msg = errmsgbuf;
			break;
		case TQSL_CABRILLO_BAD_FIELD_DATA:
			snprintf(errmsgbuf, sizeof errmsgbuf, "Cabrillo field data error in %s field", errmsgdata);
			msg = errmsgbuf;
			break;
	}
	if (!msg) {
		snprintf(errmsgbuf, sizeof errmsgbuf, "Cabrillo unknown error: %d", err);
		if (errmsgdata[0] != '\0')
			snprintf(errmsgbuf + strlen(errmsgbuf), sizeof errmsgbuf - strlen(errmsgbuf),
				" (%s)", errmsgdata);
		msg = errmsgbuf;
	}
	tqslTrace("tqsl_cabrilloGetError", "msg=%s", msg);
	errmsgdata[0] = '\0';
	return msg;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCabrilloField(tQSL_Cabrillo cabp, tqsl_cabrilloField *field, TQSL_CABRILLO_ERROR_TYPE *error) {
	TQSL_CABRILLO *cab;
	cabrillo_field_def *fp;
	const char *fielddat;

	if ((cab = check_cabrillo(cabp)) == 0)
		return 1;
	if (field == 0 || error == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (cab->datap == 0 || cab->field_idx < 0 || cab->field_idx >= cab->contest->nfields) {
		char *cp;
		while ((cp = fgets_cab(cab->rec, sizeof cab->rec, cab->fp)) != 0) {
			cab->line_no++;
			cab->datap = tqsl_parse_cabrillo_record(cab->rec);
			if (cab->datap != 0) {
				if (!strcasecmp(cab->rec, "QSO")) {
					cab->field_idx = 0;
					char *fieldp = strtok(cab->datap, " \t\r\n");
					memset(cab->fields, 0, sizeof cab->fields);
					for (int i = 0; fieldp && i < static_cast<int>(sizeof cab->fields / sizeof cab->fields[0]); i++) {
						cab->fields[i] = fieldp;
						fieldp = strtok(0, " \t\r\n");
					}
					break;
				} else if (!strcasecmp(cab->rec, "END-OF-LOG")) {
					*error = TQSL_CABRILLO_EOF;
					return 0;
				}
			}
		}
		if (cp == 0) {
			if (ferror(cab->fp)) {
				tQSL_Error = TQSL_SYSTEM_ERROR;
				tQSL_Errno = errno;
				goto err;
			} else {
				*error = TQSL_CABRILLO_EOF;
				return 0;
			}
		}
	}
	// Record data is okay and field index is valid.

	fp = cab->contest->fields + cab->field_idx;
	strncpy(field->name, fp->name, sizeof field->name);
	if (fp->loc < 0) {  // New contest
		// try to guess which field has the 'call-worked'
		for (int i = 6; i < TQSL_CABRILLO_MAX_FIELDS && cab->fields[i]; i++) {
			char *p = cab->fields[i];
			// Simple parse: a callsign is at least 4 chars long
			// and has at least one digit and at least one letter
			// Nothing but alphnumeric and '/' allowed.


			// First, eliminate grid squares
			if (strlen(p) == 4) {
				if (isalpha(p[0]) && isalpha(p[1]) &&
				    isdigit(p[2]) && isdigit(p[3]))
					continue;
			}
			int nlet = 0, ndig = 0;
			for (; *p; p++) {
				if (isdigit(*p)) {
					 ndig++;
				} else if (isalpha(*p)) {
					 nlet++;
				} else if (*p != '/') {
					ndig = 0;
					nlet = 0;
					break;
				}
			}
			if (nlet > 0 && ndig > 0 && nlet+ndig > 3) {
				// OK, looks like a callsign. Is it possibly a gridsquare?
				if (strlen(p) == 6) {
					if ((isalpha(p[0]) && toupper(p[0]) < 'S') &&
					    (isalpha(p[1]) && toupper(p[1]) < 'S') &&
					    (isdigit(p[2]) && isdigit(p[3])) &&
					    (isalpha(p[4]) && toupper(p[4]) < 'Y') &&
					    (isalpha(p[5]) && toupper(p[5]) < 'Y'))
						continue;	// Gridsquare. Don't use it.
				}
				if (fp->loc < 0) {		// No callsign candidate yet
					fp->loc = i;
				} else {
					tQSL_Cabrillo_Error = TQSL_CABRILLO_UNKNOWN_CONTEST;
					tQSL_Error = TQSL_CABRILLO_ERROR;
					snprintf(errmsgdata, sizeof errmsgdata, "%s\nUnable to find a unique call-worked field.\n"
						"Please define a custom Cabrillo entry for this contest.\n", cab->contest->contest_name);
					goto err;
				}
			}
		}
		if (fp->loc < 0) {		// Still can't find a call. Have to bail.
			tQSL_Cabrillo_Error = TQSL_CABRILLO_UNKNOWN_CONTEST;
			tQSL_Error = TQSL_CABRILLO_ERROR;
			snprintf(errmsgdata, sizeof errmsgdata, "%s\nUnable to find a valid call-worked field.\n"
				"Please define a custom Cabrillo entry for this contest.\n", cab->contest->contest_name);
			goto err;
		}
	}
	fielddat = cab->fields[fp->loc];
	if (fielddat == 0) {
		tQSL_Cabrillo_Error = TQSL_CABRILLO_BAD_FIELD_DATA;
		tQSL_Error = TQSL_CABRILLO_ERROR;
		strncpy(errmsgdata, field->name, sizeof errmsgdata);
		goto err;
	}
	strncpy(field->value, fielddat, sizeof field->value);

	if (fp->process && fp->process(cab, field)) {
		tQSL_Cabrillo_Error = TQSL_CABRILLO_BAD_FIELD_DATA;
		tQSL_Error = TQSL_CABRILLO_ERROR;
		strncpy(errmsgdata, field->name, sizeof errmsgdata);
		goto err;
	}
	cab->field_idx++;
	if (cab->field_idx >= cab->contest->nfields)
		*error = TQSL_CABRILLO_EOR;
	else
		*error = TQSL_CABRILLO_NO_ERROR;
	return 0;
 err:
	cab->datap = NULL;		// Ignore this
	strncpy(tQSL_ErrorFile, cab->filename, sizeof tQSL_ErrorFile);
	tQSL_ErrorFile[sizeof tQSL_ErrorFile-1] = 0;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCabrilloContest(tQSL_Cabrillo cabp, char *buf, int bufsiz) {
	TQSL_CABRILLO *cab;
	if ((cab = check_cabrillo(cabp)) == 0)
		return 1;
	if (buf == 0 || bufsiz <= 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (bufsiz < static_cast<int>(strlen(cab->contest->contest_name))+1) {
		tQSL_Error = TQSL_BUFFER_ERROR;
		return 1;
	}
	strncpy(buf, cab->contest->contest_name, bufsiz);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCabrilloFreqType(tQSL_Cabrillo cabp, TQSL_CABRILLO_FREQ_TYPE *type) {
	TQSL_CABRILLO *cab;
	if ((cab = check_cabrillo(cabp)) == 0)
		return 1;
	if (type == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*type = cab->contest->type;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCabrilloLine(tQSL_Cabrillo cabp, int *lineno) {
	TQSL_CABRILLO *cab;
	if ((cab = check_cabrillo(cabp)) == 0)
		return 1;
	if (lineno == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*lineno = cab->line_no;
	return 0;
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getCabrilloRecordText(tQSL_Cabrillo cabp) {
	TQSL_CABRILLO *cab;
	if ((cab = check_cabrillo(cabp)) == 0)
		return 0;
	return cab->rec;
}
