/***************************************************************************
                          tqslconvert.cpp  -  description
                             -------------------
    begin                : Sun Nov 17 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/


#define TQSLLIB_DEF

#include "tqsllib.h"

#include "tqslconvert.h"
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include "tqslerrno.h"
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <ctype.h>
#include <set>
#ifdef USE_LMDB
#include <lmdb.h>
#define db_strerror mdb_strerror
#else
#include <db.h>
#endif

#include <locale.h>
//#include <iostream>

#ifndef _WIN32
    #include <unistd.h>
    #include <dirent.h>
#else
    #include <direct.h>
    #include "windirent.h"
#endif

#include "winstrdefs.h"

using std::set;
using std::string;
using std::vector;
using std::sort;
using std::map;

static bool checkCallSign(const string& call);

namespace tqsllib {

class TQSL_CONVERTER {
 public:
	TQSL_CONVERTER();
	~TQSL_CONVERTER();
	void clearRec();
	int sentinel;
//	FILE *file;
	tQSL_ADIF adif;
	tQSL_Cabrillo cab;
	tQSL_Cert *certs;
	int ncerts;
	tQSL_Location loc;
	TQSL_QSO_RECORD rec;
	bool rec_done;
	int cert_idx;
	int next_cert_uid;
	int cert_uid;
	int loc_uid;
	bool need_station_rec;
	int *cert_uids;
	bool allow_bad_calls;
	set <string> modes;
	set <string> bands;
	set <string> propmodes;
	set <string> satellites;
	string rec_text;
	tQSL_Date start, end;
	int location_handling;
	bool db_open;
#ifdef USE_LMDB
	MDB_dbi seendb;
	MDB_env* dbenv;
	MDB_txn* txn;
	MDB_cursor* cursor;
#else
	DB *seendb;
	DB_ENV* dbenv;
	DB_TXN* txn;
	DBC* cursor;
#endif
	char *dbpath;
	FILE* errfile;
	char serial[512];
	char callsign[64];
	bool allow_dupes;
	bool ignore_secs;
	bool need_ident_rec;
	char *appName;
	int dxcc;
	bool newstation;
	map <string, int> taglines;
	int err_tag_line;
};

#if !defined(__APPLE__) && !defined(_WIN32) && !defined(__clang__)
        #pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

inline TQSL_CONVERTER::TQSL_CONVERTER()  : sentinel(0x4445) {
//	file = 0;
	adif = 0;
	cab = 0;
	cert_idx = -1;
	dxcc = -1;
	loc_uid = 0;
	cert_uid = 0;
	next_cert_uid = 1;
	cert_uids = NULL;
	need_station_rec = false;
	rec_done = true;
	allow_bad_calls = false;
	location_handling = TQSL_LOC_UPDATE;
	allow_dupes = true; //by default, don't change existing behavior (also helps with commit)
	ignore_secs = false;	// Use full time
	memset(&rec, 0, sizeof rec);
	memset(&start, 0, sizeof start);
	memset(&end, 0, sizeof end);
	db_open = false;
#ifndef USE_LMDB
	seendb = NULL;
#endif
	dbpath = NULL;
	dbenv = NULL;
	txn = NULL;
	cursor = NULL;
	errfile = NULL;
	memset(&serial, 0, sizeof serial);
	memset(&callsign, 0, sizeof callsign);
	appName = NULL;
	need_ident_rec = true;
	// Init the band data
	const char *val;
	int n = 0;
	tqsl_getNumBand(&n);
	for (int i = 0; i < n; i++) {
		val = 0;
		tqsl_getBand(i, &val, 0, 0, 0);
		if (val)
			bands.insert(val);
	}
	// Init the mode data
	n = 0;
	tqsl_getNumMode(&n);
	for (int i = 0; i < n; i++) {
		val = 0;
		tqsl_getMode(i, &val, 0);
		if (val)
			modes.insert(val);
	}
	// Init the propagation mode data
	n = 0;
	tqsl_getNumPropagationMode(&n);
	for (int i = 0; i < n; i++) {
		val = 0;
		tqsl_getPropagationMode(i, &val, 0);
		if (val)
			propmodes.insert(val);
	}
	// Init the satellite data
	n = 0;
	tqsl_getNumSatellite(&n);
	for (int i = 0; i < n; i++) {
		val = 0;
		tqsl_getSatellite(i, &val, 0, 0, 0);
		if (val)
			satellites.insert(val);
	}
}

inline TQSL_CONVERTER::~TQSL_CONVERTER() {
	clearRec();
//	if (file)
//		fclose(file);
	tqsl_endADIF(&adif);
	if (cert_uids)
		delete[] cert_uids;
	sentinel = 0;
}

inline void TQSL_CONVERTER::clearRec() {
	memset(&rec, 0, sizeof rec);
	rec_text = "";
	err_tag_line = 0;
}

#define CAST_TQSL_CONVERTER(x) ((tqsllib::TQSL_CONVERTER *)(x))

}	// namespace tqsllib

using tqsllib::TQSL_CONVERTER;

template <class Container>
static void add_to_container(const char *str, size_t len, void *data) {
    Container *cont = static_cast<Container*>(data);
    cont->push_back(string(str, len));
}

typedef void(*split_fn)(const char *, size_t, void *);
static void split(const char *str, char sep, split_fn fun, void *data) {
    unsigned int start = 0, stop;
    for (stop = 0; str[stop]; stop++) {
	if (str[stop] == sep) {
	    fun(str + start, stop - start, data);
	    start = stop + 1;
	}
    }
    fun(str + start, stop - start, data);
}

template <class Container>
static void splitStr(const string& str, Container& cont, char delim = ' ') {
    split(str.c_str(), delim, static_cast<split_fn>(add_to_container<Container>), &cont);
}

static char * fix_freq(const char *in) {
    static char out[128];
    const char *p = in;
    bool decimal = false;
    char *o = out;
    while (*p) {
	if (*p == '.') {
		if (decimal) {
			p++;
			continue;
		}
		decimal = true;
	}
	*o++ = *p++;
    }
    *o = '\0';
    return out;
}

static char *
tqsl_strtoupper(char *str) {
	for (char *cp = str; *cp != '\0'; cp++)
		*cp = toupper(*cp);
	return str;
}

static TQSL_CONVERTER *
check_conv(tQSL_Converter conv) {
	if (tqsl_init())
		return 0;
	if (conv == 0 || CAST_TQSL_CONVERTER(conv)->sentinel != 0x4445)
		return 0;
	return CAST_TQSL_CONVERTER(conv);
}

static tqsl_adifFieldDefinitions adif_qso_record_fields[] = {
	{ "CALL", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_CALLSIGN_MAX, 0, 0, NULL },
	{ "BAND", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_BAND_MAX, 0, 0, NULL },
	{ "MODE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_MODE_MAX, 0, 0, NULL },
	{ "SUBMODE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_MODE_MAX, 0, 0, NULL },
	{ "QSO_DATE", "", TQSL_ADIF_RANGE_TYPE_NONE, 10, 0, 0, NULL },
	{ "TIME_ON", "", TQSL_ADIF_RANGE_TYPE_NONE, 10, 0, 0, NULL },
	{ "FREQ", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_FREQ_MAX, 0, 0, NULL },
	{ "FREQ_RX", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_FREQ_MAX, 0, 0, NULL },
	{ "BAND_RX", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_BAND_MAX, 0, 0, NULL },
	{ "SAT_NAME", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_SATNAME_MAX, 0, 0, NULL },
	{ "PROP_MODE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_PROPMODE_MAX, 0, 0, NULL },
	/* Fields specifying contents of the location for a QSO */
	{ "MY_CNTY", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_CNTY_MAX, 0, 0, NULL },
	{ "MY_COUNTRY", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_COUNTRY_MAX, 0, 0, NULL },
	{ "MY_CQ_ZONE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_ZONE_MAX, 0, 0, NULL },
	{ "MY_DXCC", "", TQSL_ADIF_RANGE_TYPE_NONE, 10, 0, 0, NULL },
	{ "MY_GRIDSQUARE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_GRID_MAX, 0, 0, NULL },
	{ "MY_IOTA", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_IOTA_MAX, 0, 0, NULL },
	{ "MY_ITU_ZONE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_ZONE_MAX, 0, 0, NULL },
	{ "MY_STATE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_STATE_MAX, 0, 0, NULL },
	{ "MY_VUCC_GRIDS", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_GRID_MAX, 0, 0, NULL },
	{ "OPERATOR", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_CALLSIGN_MAX, 0, 0, NULL },
	{ "STATION_CALLSIGN", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_CALLSIGN_MAX, 0, 0, NULL },
#ifdef USE_OWNER_CALLSIGN
	{ "OWNER_CALLSIGN", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_CALLSIGN_MAX, 0, 0, NULL },
#endif
	{ "eor", "", TQSL_ADIF_RANGE_TYPE_NONE, 0, 0, 0, NULL },
};

DLLEXPORT int CALLCONVENTION
tqsl_beginConverter(tQSL_Converter *convp) {
	tqslTrace("tqsl_beginConverter", NULL);
	if (tqsl_init())
		return 0;
	if (!convp) {
		tqslTrace("tqsl_beginConverter", "convp=NULL");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_CONVERTER *conv = new TQSL_CONVERTER();
	*convp = conv;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_beginADIFConverter(tQSL_Converter *convp, const char *filename, tQSL_Cert *certs,
	int ncerts, tQSL_Location loc) {
	tqslTrace("tqsl_beginADIFConverter", NULL);
	if (tqsl_init())
		return 0;
	if (!convp || !filename) {
		tqslTrace("tqsl_beginADIFConverter", "arg err convp=0x%lx filename=0x%lx certs=0x%lx", convp, filename, certs);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	tQSL_ADIF adif;
	if (tqsl_beginADIF(&adif, filename)) {
		tqslTrace("tqsl_beginADIFConverter", "tqsl_beginADIF fail %d", tQSL_Error);
		return 1;
	}
	TQSL_CONVERTER *conv = new TQSL_CONVERTER();
	conv->adif = adif;
	conv->certs = certs;
	conv->ncerts = ncerts;
	if (ncerts > 0) {
		conv->cert_uids = new int[ncerts];
		for (int i = 0; i < ncerts; i++)
			conv->cert_uids[i] = -1;
	}
	conv->loc = loc;
	*convp = conv;

	tqsl_getLocationCallSign(loc, conv->callsign, sizeof conv->callsign);
	tqsl_getLocationDXCCEntity(loc, &conv->dxcc);


	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_beginCabrilloConverter(tQSL_Converter *convp, const char *filename, tQSL_Cert *certs,
	int ncerts, tQSL_Location loc) {
	tqslTrace("tqsl_beginCabrilloConverter", NULL);

	if (tqsl_init())
		return 0;
	if (!convp || !filename) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		tqslTrace("tqsl_beginCabrilloConverter", "arg error convp=0x%lx, filename=0x%lx, certs=0x%lx", convp, filename, certs);
		return 1;
	}
	tQSL_Cabrillo cab;
	if (tqsl_beginCabrillo(&cab, filename)) {
		tqslTrace("tqsl_beginCabrilloConverter", "tqsl_beginCabrillo fail %d", tQSL_Error);
		return 1;
	}
	TQSL_CONVERTER *conv = new TQSL_CONVERTER();
	conv->cab = cab;
	conv->certs = certs;
	conv->ncerts = ncerts;
	if (ncerts > 0) {
		conv->cert_uids = new int[ncerts];
		for (int i = 0; i < ncerts; i++)
			conv->cert_uids[i] = -1;
	}
	conv->loc = loc;
	*convp = conv;

	tqsl_getLocationCallSign(loc, conv->callsign, sizeof conv->callsign);
	tqsl_getLocationDXCCEntity(loc, &conv->dxcc);

	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_endConverter(tQSL_Converter *convp) {
	tqslTrace("tqsl_endConverter", NULL);

	if (!convp || CAST_TQSL_CONVERTER(*convp) == 0)
		return 0;

	TQSL_CONVERTER* conv;

	if ((conv = check_conv(*convp))) {
#ifdef USE_LMDB
		if (conv->txn) mdb_txn_abort(conv->txn);
#else
		if (conv->txn) conv->txn->abort(conv->txn);
#endif
		if (conv->db_open) {
#ifdef USE_LMDB
			mdb_dbi_close(conv->dbenv, conv->seendb);
#else
			conv->seendb->compact(conv->seendb, NULL, NULL, NULL, NULL, 0, NULL);
			conv->seendb->close(conv->seendb, 0);
#endif
		}
		conv->db_open = false;
		if (conv->dbenv) {
#ifdef USE_LMDB
			mdb_env_close(conv->dbenv);
#else
			char **unused;
			conv->dbenv->txn_checkpoint(conv->dbenv, 0, 0, 0);
			conv->dbenv->log_archive(conv->dbenv, &unused, DB_ARCH_REMOVE);
			conv->dbenv->close(conv->dbenv, 0);
#endif
		}
		// close files and clean up converters, if any
		if (conv->adif) tqsl_endADIF(&conv->adif);
		if (conv->cab) tqsl_endCabrillo(&conv->cab);
		if (conv->dbpath) free(conv->dbpath);
		if (conv->errfile) fclose(conv->errfile);
	}

	if (conv->appName) free(conv->appName);
	if (CAST_TQSL_CONVERTER(*convp)->sentinel == 0x4445)
		delete CAST_TQSL_CONVERTER(*convp);
	*convp = 0;
	return 0;
}

static unsigned char *
adif_allocate(size_t size) {
	return new unsigned char[size];
}

static int
find_matching_cert(TQSL_CONVERTER *conv, int targetdxcc, bool *anyfound) {
	int i;
	*anyfound = false;
	for (i = 0; i < conv->ncerts; i++) {
		tQSL_Date cdate;
		char call[256];
		int dxcc;

		if (tqsl_getCertificateCallSign(conv->certs[i], call, sizeof call))
			return -1;
		if (strcasecmp(conv->callsign, call))		// Not for this call
			continue;
		if (tqsl_getCertificateDXCCEntity(conv->certs[i], &dxcc))
			return -1;
		if (dxcc != targetdxcc)
			continue;				// Not for this call and DXCC
		*anyfound = true;
		if (tqsl_getCertificateQSONotBeforeDate(conv->certs[i], &cdate))
			continue;
		if (tqsl_compareDates(&(conv->rec.date), &cdate) < 0)
			continue;
		if (tqsl_getCertificateQSONotAfterDate(conv->certs[i], &cdate))
			continue;
		if (tqsl_compareDates(&(conv->rec.date), &cdate) > 0)
			continue;
		return i;
	}
	return -1;
}

static const char *notypes[] = { "D", "T", "M", "C", "N", "S", "B", "E", "L", "" };	// "C" is ADIF 1.0 for "S"; also "I" and "G" in ADIX

static const char *
tqsl_infer_band(const char* infreq) {
	char *oldlocale = setlocale(LC_NUMERIC, "C");
	double freq = atof(infreq);
	setlocale(LC_NUMERIC, oldlocale);
	double freq_khz = freq * 1000.0;
	int nband = 0;
	tqsl_getNumBand(&nband);
	for (int i = 0; i < nband; i++) {
		const char *name;
		const char *spectrum;
		int low, high;
		if (tqsl_getBand(i, &name, &spectrum, &low, &high))
			break;
		bool match = false;
		if (!strcmp(spectrum, "HF")) {
			// Allow for cases where loggers that don't log the
			// real frequency.
			if (low == 10100) low = 10000;
			else if (low == 18068) low = 18000;
			else if (low == 24890) low = 24000;
			if (freq_khz >= low && freq_khz <= high) {
				match = true;
			}
		} else {
			if (freq >= low && freq <= high)
				match = true;
			if (freq >= low && high == 0)
				match = true;
		}
		if (match)
			 return name;
	}
	return "";
}

DLLEXPORT int CALLCONVENTION
tqsl_setADIFConverterDateFilter(tQSL_Converter convp, tQSL_Date *start, tQSL_Date *end) {
	TQSL_CONVERTER *conv;
	tqslTrace("tqsl_setADIFConverterDateFilter", NULL);

	if (!(conv = check_conv(convp)))
		return 1;
	if (start == NULL)
		conv->start.year = conv->start.month = conv->start.day = 0;
	else
		conv->start = *start;
	if (end == NULL)
		conv->end.year = conv->end.month = conv->end.day = 0;
	else
		conv->end = *end;
	return 0;
}

// Remove the dupes db files
static void
remove_db(const char *path)  {
	tqslTrace("remove_db", "path=%s", path);
#ifdef _WIN32
	wchar_t* wpath = utf8_to_wchar(path);
	_WDIR *dir = _wopendir(wpath);
	free_wchar(wpath);
#else
	DIR *dir = opendir(path);
#endif
	if (dir != NULL) {
#ifdef USE_LMDB
#ifdef _WIN32
		struct _wdirent *ent = NULL;
		while ((ent = _wreaddir(dir)) != NULL) {
			if (!wcscmp(ent->d_name, L"data.mdb") ||
			!wcscmp(ent->d_name, L"lock.mdb")) {
#else
		struct dirent *ent = NULL;
		while ((ent = readdir(dir)) != NULL) {
			if (!strcmp(ent->d_name, "data.mdb") ||
			!strcmp(ent->d_name, "lock.mdb")) {
#endif
#else // USE_LMDB
#ifdef _WIN32
		struct _wdirent *ent = NULL;
		while ((ent = _wreaddir(dir)) != NULL) {
			if (!wcscmp(ent->d_name, L"duplicates.db") ||
			!wcsncmp(ent->d_name, L"log.", 4) ||
			!wcsncmp(ent->d_name, L"__db.", 5)) {
#else
		struct dirent *ent = NULL;
		while ((ent = readdir(dir)) != NULL) {
			if (!strcmp(ent->d_name, "duplicates.db") ||
			!strncmp(ent->d_name, "log.", 4) ||
			!strncmp(ent->d_name, "__db.", 5)) {
#endif
#endif // USE_LMDB
				string fname = path;
				int rstat;
#ifdef _WIN32
				char dname[TQSL_MAX_PATH_LEN];
				wcstombs(dname, ent->d_name, TQSL_MAX_PATH_LEN);
				fname = fname + "/" + dname;
				wchar_t* wfname = utf8_to_wchar(fname.c_str());
				tqslTrace("remove_db", "unlinking %s", fname.c_str());
				rstat = _wunlink(wfname);
				free_wchar(wfname);
#else
				fname = fname + "/" + ent->d_name;
				tqslTrace("remove_db", "unlinking %s", fname.c_str());
				rstat = unlink(fname.c_str());
#endif
				if (rstat < 0) {
					tqslTrace("remove_db", "can't unlink %s: %s", fname.c_str(), strerror(errno));
				}
			}
		}
#ifdef _WIN32
		_wclosedir(dir);
#else
		closedir(dir);
#endif
	}
	return;
}
#if !defined(_WIN32) && !defined(USE_LMDB)
// Callback method for the dbenv->failchk() call
// Used to determine if the given pid/tid is
// alive.
static int isalive(DB_ENV *env, pid_t pid, db_threadid_t tid, uint32_t flags) {
	int alive = 0;

	if (pid == getpid()) {
		alive = 1;
	} else if (kill(pid, 0) == 0) {
		alive = 1;
	} else if (errno == EPERM) {
		alive = 1;
	}
	return alive;
}
#endif // _WIN32

// Open the duplicates database

#ifdef USE_LMDB
static bool open_db(TQSL_CONVERTER *conv, bool readonly) {
	bool dbinit_cleanup = false;
	int dbret;
	bool triedRemove = false;
	bool triedDelete = false;
	string fixedpath = tQSL_BaseDir; //must be first because of gotos
	size_t found = fixedpath.find('\\');

	tqslTrace("open_db", "path=%s", fixedpath.c_str());
	//bdb complains about \\s in path on windows...

	while (found != string::npos) {
		fixedpath.replace(found, 1, "/");
		found = fixedpath.find('\\');
	}

	conv->dbpath = strdup(fixedpath.c_str());

#ifndef _WIN32
	// Clean up junk in that directory
	DIR *dir = opendir(fixedpath.c_str());
	if (dir != NULL) {
		struct dirent *ent;
		while ((ent = readdir(dir)) != NULL) {
			if (ent->d_name[0] == '.')
				continue;
			struct stat s;
			// If it's a symlink pointing to itself, remove it.
			string fname = fixedpath + "/" + ent->d_name;
			if (stat(fname.c_str(), &s)) {
				if (errno == ELOOP) {
#ifdef _WIN32
					_wunlink(ConvertFromUtf8ToUtf16(fname.c_str()));
#else
					unlink(fname.c_str());
#endif
				}
			}
		}
		closedir(dir);
	}
#endif
	string logpath = fixedpath + "/dberr.log";
#ifdef _WIN32
	wchar_t* wlogpath = utf8_to_wchar(logpath.c_str());
	conv->errfile = _wfopen(wlogpath, L"wb");
	free_wchar(wlogpath);
#else
	conv->errfile = fopen(logpath.c_str(), "wb");
#endif

 reopen:

	// Try to open the database
	while (true) {
		if (!conv->dbenv) {
			// Create the database environment handle
			if ((dbret = mdb_env_create(&conv->dbenv))) {
				// can't make env handle
				tqslTrace("open_db", "mdb_env_create error %s", mdb_strerror(dbret));
				if (conv->errfile)
					fprintf(conv->errfile, "mdb_env_create error %s\n", mdb_strerror(dbret));
				dbinit_cleanup = true;
				goto dbinit_end;
			}
			tqslTrace("open_db", "dbenv=0x%lx", conv->dbenv);
		}
		mdb_env_set_maxdbs(conv->dbenv, 2);
		mdb_env_set_maxreaders(conv->dbenv, 2);
		mdb_env_set_mapsize(conv->dbenv, 1024 * 1024 * 1024);
		// Now open the database
		tqslTrace("open_db", "Opening the database at %s", conv->dbpath);
		if ((dbret = mdb_env_open(conv->dbenv, conv->dbpath, 0, 0600))) {
			tqslTrace("open_db", "dbenv->open %s error %s", conv->dbpath, mdb_strerror(dbret));
			if (conv->errfile)
				fprintf(conv->errfile, "opening DB %s returns status %s\n", conv->dbpath, mdb_strerror(dbret));
			// can't open environment - try to delete it and try again.
			tqslTrace("open_db", "Environment open fail, triedRemove=%d", triedRemove);
			if (!triedRemove) {
				// Remove the dross
				tqslTrace("open_db", "Removing environment");
				conv->dbenv = NULL;
				triedRemove = true;
				if (conv->errfile)
					fprintf(conv->errfile, "About to retry after removing the environment\n");
				tqslTrace("open_db", "About to retry after removing the environment");
				continue;
			}
			tqslTrace("open_db", "Retry attempt after removing the environment failed");
			if (conv->errfile) {
				fprintf(conv->errfile, "Retry attempt after removing the environment failed.\n");
			}
			// can't open environment and cleanup efforts failed.
			mdb_env_close(conv->dbenv);
			conv->dbenv = NULL;	// this can't be recovered
			dbinit_cleanup = true;
			tqslTrace("open_db", "can't fix. abandoning.");
			remove_db(fixedpath.c_str());
			goto dbinit_end;
		}
		break;		// Opened OK.
	}

	tqslTrace("open_db", "starting transaction, readonly=%d", readonly);
	if ((dbret = mdb_txn_begin(conv->dbenv, NULL, readonly ? MDB_RDONLY : 0, &conv->txn))) {
		// can't start a txn
		tqslTrace("open_db", "can't create txn %s", mdb_strerror(dbret));
		if (conv->errfile)
			fprintf(conv->errfile, "Can't create transaction: %s\n", mdb_strerror(dbret));
		dbinit_cleanup = true;
		goto dbinit_end;
	}

	tqslTrace("open_db", "opening database now");
	if ((dbret = mdb_dbi_open(conv->txn, NULL, 0, &conv->seendb))) {
		if (dbret == MDB_NOTFOUND) {
			tqslTrace("open_db", "DB not found, making a new one");
			dbret = mdb_dbi_open(conv->txn, NULL, MDB_CREATE, &conv->seendb);
		}
		if (dbret) {
			// can't open the db
			tqslTrace("open_db", "create failed with %s errno %d", mdb_strerror(dbret), errno);
			if (conv->errfile)
				fprintf(conv->errfile, "create failed with %s errno %d", mdb_strerror(dbret), errno);
			dbinit_cleanup = true;
			goto dbinit_end;
		}
	}

 dbinit_end:
	if (dbinit_cleanup) {
		tqslTrace("open_db", "DB open failed, triedDelete=%d", triedDelete);
		tQSL_Error = TQSL_DB_ERROR;
		tQSL_Errno = errno;
		strncpy(tQSL_CustomError, mdb_strerror(dbret), sizeof tQSL_CustomError);
		tqslTrace("open_db", "Error opening db: %s", tQSL_CustomError);
		if (conv->txn) mdb_txn_abort(conv->txn);
		conv->txn = NULL;
		if (conv->db_open) {
			mdb_dbi_close(conv->dbenv, conv->seendb);
			conv->db_open = false;
		}
		if (conv->dbenv) {
			if (conv->dbpath) {
				free(conv->dbpath);
				conv->dbpath = NULL;
			}
			mdb_drop(conv->txn, conv->seendb, 1);
			mdb_env_close(conv->dbenv);
		}
		if (conv->cursor) mdb_cursor_close(conv->cursor);
		if (conv->errfile) fclose(conv->errfile);
		conv->dbenv = NULL;
		conv->cursor = NULL;
		conv->errfile = NULL;
		// Handle case where the database is just broken
#ifndef DB_RUNRECOVERY
#define DB_RUNRECOVERY -30973
#endif
		if ((dbret == EINVAL || dbret == DB_RUNRECOVERY) && !triedDelete) {
			tqslTrace("open_db", "EINVAL/RUNRECOVERY. Removing db");
			remove_db(fixedpath.c_str());
			triedDelete = true;
			goto reopen;
		}
		conv->db_open = false;
		return false;
	}
	conv->db_open = true;
	return true;
}
#else // USE_LMDB
static bool open_db(TQSL_CONVERTER *conv, bool readonly) {
	bool dbinit_cleanup = false;
	int dbret;
	bool triedRemove = false;
	bool triedDelete = false;
	int envflags = DB_INIT_TXN|DB_INIT_LOG|DB_INIT_MPOOL|DB_RECOVER|DB_REGISTER|DB_CREATE;
	string fixedpath = tQSL_BaseDir; //must be first because of gotos
	size_t found = fixedpath.find('\\');

	tqslTrace("open_db", "path=%s", fixedpath.c_str());
	//bdb complains about \\s in path on windows...

	while (found != string::npos) {
		fixedpath.replace(found, 1, "/");
		found = fixedpath.find('\\');
	}

	conv->dbpath = strdup(fixedpath.c_str());

#ifndef _WIN32
	// Clean up junk in that directory
	DIR *dir = opendir(fixedpath.c_str());
	if (dir != NULL) {
		struct dirent *ent;
		while ((ent = readdir(dir)) != NULL) {
			if (ent->d_name[0] == '.')
				continue;
			struct stat s;
			// If it's a symlink pointing to itself, remove it.
			string fname = fixedpath + "/" + ent->d_name;
			if (stat(fname.c_str(), &s)) {
				if (errno == ELOOP) {
#ifdef _WIN32
					_wunlink(ConvertFromUtf8ToUtf16(fname.c_str()));
#else
					unlink(fname.c_str());
#endif
				}
			}
		}
		closedir(dir);
	}
#endif
	string logpath = fixedpath + "/dberr.log";
#ifdef _WIN32
	wchar_t* wlogpath = utf8_to_wchar(logpath.c_str());
	conv->errfile = _wfopen(wlogpath, L"wb");
	free_wchar(wlogpath);
#else
	conv->errfile = fopen(logpath.c_str(), "wb");
#endif

 reopen:

	// Try to open the database
	while (true) {
		if (!conv->dbenv) {
			// Create the database environment handle
			if ((dbret = db_env_create(&conv->dbenv, 0))) {
				// can't make env handle
				tqslTrace("open_db", "db_env_create error %s", db_strerror(dbret));
				dbinit_cleanup = true;
				goto dbinit_end;
			}
			tqslTrace("open_db", "dbenv=0x%lx", conv->dbenv);
			if (conv->errfile) {
				conv->dbenv->set_errfile(conv->dbenv, conv->errfile);
				conv->dbenv->set_verbose(conv->dbenv, DB_VERB_RECOVERY, 1);
			}
			// Enable stale lock removal
			conv->dbenv->set_thread_count(conv->dbenv, 8);
#ifndef _WIN32
			conv->dbenv->set_isalive(conv->dbenv, isalive);
#endif
			// Log files default to 10 Mb each. We don't need nearly that much.
			if (conv->dbenv->set_lg_max)
				conv->dbenv->set_lg_max(conv->dbenv, 256 * 1024);
			// Allocate additional locking resources - some have run out with
			// the default 1000 locks
			if (conv->dbenv->set_lk_max_locks)
				conv->dbenv->set_lk_max_locks(conv->dbenv, 20000);
			if (conv->dbenv->set_lk_max_objects)
				conv->dbenv->set_lk_max_objects(conv->dbenv, 20000);
		}
		// Now open the database
		tqslTrace("open_db", "Opening the database at %s", conv->dbpath);
		if ((dbret = conv->dbenv->open(conv->dbenv, conv->dbpath, envflags, 0600))) {
			int db_errno = errno;
			tqslTrace("open_db", "dbenv->open %s error %s", conv->dbpath, db_strerror(dbret));
			if (conv->errfile)
				fprintf(conv->errfile, "opening DB %s returns status %s\n", conv->dbpath, db_strerror(dbret));
			// Can't open the database - maybe try private?
			if ((dbret == EACCES || dbret == EROFS) || (dbret == EINVAL && errno == dbret)) {
				if (!(envflags & DB_PRIVATE)) {
					envflags |= DB_PRIVATE;
					continue;
				}
			}
			// can't open environment - try to delete it and try again.
			tqslTrace("open_db", "Environment open fail, triedRemove=%d", triedRemove);
			if (!triedRemove) {
				// Remove the dross
				tqslTrace("open_db", "Removing environment");
				conv->dbenv->remove(conv->dbenv, conv->dbpath, DB_FORCE);
				conv->dbenv = NULL;
				triedRemove = true;
				if (conv->errfile)
					fprintf(conv->errfile, "About to retry after removing the environment\n");
				tqslTrace("open_db", "About to retry after removing the environment");
				continue;
			}
			tqslTrace("open_db", "Retry attempt after removing the environment failed");
			if (conv->errfile) {
				fprintf(conv->errfile, "Retry attempt after removing the environment failed.\n");
			}
			// EINVAL means that the database is corrupted to the point
			// where it can't be opened. Remove it and try again.
			if ((dbret == EINVAL || db_errno == EINVAL) && !triedDelete) {
				tqslTrace("open_db", "EINVAL. Removing db");
				conv->dbenv->close(conv->dbenv, 0);
				conv->dbenv = NULL;
				remove_db(fixedpath.c_str());
				triedDelete = true;
				continue;
			}

			// can't open environment and cleanup efforts failed.
			conv->dbenv->close(conv->dbenv, 0);
			conv->dbenv = NULL;	// this can't be recovered
			dbinit_cleanup = true;
			tqslTrace("open_db", "can't fix. abandoning.");
			remove_db(fixedpath.c_str());
			goto dbinit_end;
		}
		break;		// Opened OK.
	}

#ifndef _WIN32		// isalive() method doesn't exist for WIN32.
	// Stale lock removal
	tqslTrace("open_db", "Removing stale locks");
	dbret = conv->dbenv->failchk(conv->dbenv, 0);
	if (dbret && conv->errfile) {
		fprintf(conv->errfile, "lock removal for DB %s returns status %s\n", conv->dbpath, db_strerror(dbret));
	}
#endif

	tqslTrace("open_db", "calling db_create");
	if ((dbret = db_create(&conv->seendb, conv->dbenv, 0))) {
		// can't create db
		dbinit_cleanup = true;
		tqslTrace("open_db", "Can't create db");
		goto dbinit_end;
	}

#ifndef DB_TXN_BULK
#define DB_TXN_BULK 0
#endif
	tqslTrace("open_db", "starting transaction, readonly=%d", readonly);
	if (!readonly && (dbret = conv->dbenv->txn_begin(conv->dbenv, NULL, &conv->txn, DB_TXN_BULK))) {
		// can't start a txn
		tqslTrace("open_db", "can't create txn %s", db_strerror(dbret));
		dbinit_cleanup = true;
		goto dbinit_end;
	}

	// Probe the database type
	tqslTrace("open_db", "opening database now");
	if ((dbret = conv->seendb->open(conv->seendb, conv->txn, "duplicates.db", NULL, DB_UNKNOWN, 0, 0600))) {
		if (dbret == ENOENT) {
			tqslTrace("open_db", "DB not found, making a new one");
			dbret = conv->seendb->open(conv->seendb, conv->txn, "duplicates.db", NULL, DB_HASH, DB_CREATE, 0600);
		}
		if (dbret) {
			// can't open the db
			tqslTrace("open_db", "create failed with %s errno %d", db_strerror(dbret), errno);
			dbinit_cleanup = true;
			goto dbinit_end;
		}
	}

	DBTYPE type;
	conv->seendb->get_type(conv->seendb, &type);
	tqslTrace("open_db", "type=%d", type);
	if (type ==  DB_BTREE) {
		tqslTrace("open_db", "BTREE type. Converting.");
		// Have to convert the database.
		string dumpfile = fixedpath + "/dupedump.txt";
#ifdef _WIN32
		wchar_t* wdumpfile = utf8_to_wchar(dumpfile.c_str());
		FILE *dmp = _wfopen(wdumpfile, L"wb+");
		free_wchar(wdumpfile);
#else
		FILE *dmp = fopen(dumpfile.c_str(), "wb+");
#endif
		if (!dmp) {
			tqslTrace("open_db", "Error opening dump file %s: %s", dumpfile.c_str(), strerror(errno));
			dbinit_cleanup = true;
			goto dbinit_end;
		}
		if (!conv->cursor) {
#ifndef DB_CURSOR_BULK
#define DB_CURSOR_BULK 0
#endif
			int err = conv->seendb->cursor(conv->seendb, conv->txn, &conv->cursor, DB_CURSOR_BULK);
			if (err) {
				strncpy(tQSL_CustomError, db_strerror(err), sizeof tQSL_CustomError);
				tQSL_Error = TQSL_DB_ERROR;
				tQSL_Errno = errno;
				tqslTrace("open_db", "Error setting cursor for old DB: %s", err);
				dbinit_cleanup = true;
				goto dbinit_end;
			}
		}

		DBT dbkey, dbdata;
		char duprec[512];
		while (1) {
			memset(&dbkey, 0, sizeof dbkey);
			memset(&dbdata, 0, sizeof dbdata);
			int status = conv->cursor->c_get(conv->cursor, &dbkey, &dbdata, DB_NEXT);
			if (DB_NOTFOUND == status) {
				break;	// No more records
			}
			if (status != 0) {
				strncpy(tQSL_CustomError, db_strerror(status), sizeof tQSL_CustomError);
				tQSL_Error = TQSL_DB_ERROR;
				tQSL_Errno = errno;
				tqslTrace("open_db", "Error reading for dump: %s", db_strerror(status));
				dbinit_cleanup = true;
				goto dbinit_end;
			}
			memcpy(duprec, dbkey.data, dbkey.size);
			duprec[dbkey.size] = '\0';
			fprintf(dmp, "%s\n", duprec);
		}
		conv->cursor->close(conv->cursor);
		if (conv->txn) conv->txn->commit(conv->txn, 0);
		conv->seendb->close(conv->seendb, 0);
		conv->db_open = false;
		conv->dbenv->remove(conv->dbenv, conv->dbpath, DB_FORCE);
		conv->dbenv->close(conv->dbenv, 0);
		conv->cursor = NULL;
		conv->seendb = NULL;
		conv->dbenv = NULL;

		// Remove the old dupe db
		tqslTrace("open_db", "Removing old format db");
		remove_db(fixedpath.c_str());

		// Now create the new database
		if ((dbret = db_env_create(&conv->dbenv, 0))) {
			// can't make env handle
			tqslTrace("open_db", "Can't make db handle: %s", db_strerror(dbret));
			dbinit_cleanup = true;
			goto dbinit_end;
		}
		if (conv->errfile)
			conv->dbenv->set_errfile(conv->dbenv, conv->errfile);
		if (conv->dbenv->set_lg_max)
			conv->dbenv->set_lg_max(conv->dbenv, 256 * 1024);
		if (conv->dbenv->set_lk_max_locks)
			conv->dbenv->set_lk_max_locks(conv->dbenv, 20000);
		if (conv->dbenv->set_lk_max_objects)
			conv->dbenv->set_lk_max_objects(conv->dbenv, 20000);
		if ((dbret = conv->dbenv->open(conv->dbenv, conv->dbpath, envflags, 0600))) {
			tqslTrace("open_db", "Error opening db: %s", db_strerror(dbret));
			if (conv->errfile)
				fprintf(conv->errfile, "opening DB %s returns status %d\n", conv->dbpath, dbret);
			dbinit_cleanup = true;
			goto dbinit_end;
		}

		if ((dbret = db_create(&conv->seendb, conv->dbenv, 0))) {
			// can't create db
			tqslTrace("open_db", "Error creating db: %s", db_strerror(dbret));
			dbinit_cleanup = true;
			goto dbinit_end;
		}

		// Create the new database
		if ((dbret = conv->seendb->open(conv->seendb, NULL, "duplicates.db", NULL, DB_HASH, DB_CREATE, 0600))) {
			// can't open the db
			tqslTrace("open_db", "Error opening new db: %s", db_strerror(dbret));
			dbinit_cleanup = true;
			goto dbinit_end;
		}
		fseek(dmp, 0, SEEK_SET);

		char d[1]= {'D'};
		memset(&dbkey, 0, sizeof dbkey);
		memset(&dbdata, 0, sizeof dbdata);
		dbdata.data = d;
		dbdata.size = 1;

		while (fgets(duprec, sizeof duprec, dmp)) {
			dbkey.data = duprec;
			dbkey.size = strlen(duprec) - 1;
			conv->seendb->put(conv->seendb, NULL, &dbkey, &dbdata, 0);
		}
		conv->seendb->close(conv->seendb, 0);
		conv->dbenv->close(conv->dbenv, 0);
		goto reopen;
	}

 dbinit_end:
	if (dbinit_cleanup) {
		tqslTrace("open_db", "DB open failed, triedDelete=%d", triedDelete);
		tQSL_Error = TQSL_DB_ERROR;
		tQSL_Errno = errno;
		strncpy(tQSL_CustomError, db_strerror(dbret), sizeof tQSL_CustomError);
		tqslTrace("open_db", "Error opening db: %s", tQSL_CustomError);
		if (conv->txn) conv->txn->abort(conv->txn);
		if (conv->seendb) conv->seendb->close(conv->seendb, 0);
		conv->db_open = false;
		if (conv->dbenv) {
			if (conv->dbpath) {
				conv->dbenv->remove(conv->dbenv,  conv->dbpath, DB_FORCE);
				free(conv->dbpath);
				conv->dbpath = NULL;
			}
			conv->dbenv->close(conv->dbenv, 0);
		}
		if (conv->cursor) conv->cursor->close(conv->cursor);
		if (conv->errfile) fclose(conv->errfile);
		conv->txn = NULL;
		conv->dbenv = NULL;
		conv->cursor = NULL;
		conv->seendb = NULL;
		conv->errfile = NULL;
		// Handle case where the database is just broken
		if (dbret == EINVAL && !triedDelete) {
			tqslTrace("open_db", "EINVAL. Removing db");
			remove_db(fixedpath.c_str());
			triedDelete = true;
			goto reopen;
		}
		return false;
	}
	conv->db_open = true;
	return true;
}
#endif // USE_LMDB

static void close_db(TQSL_CONVERTER *conv) {
	tqslTrace("close_db", NULL);

	if (conv->db_open) {
#ifdef USE_LMDB
		if (conv->txn) mdb_txn_abort(conv->txn);
#else
		if (conv->txn) conv->txn->abort(conv->txn);
#endif
#ifdef USE_LMDB
		mdb_dbi_close(conv->dbenv, conv->seendb);
#else
		conv->seendb->close(conv->seendb, 0);
#endif
		if (conv->dbenv) {
#ifdef USE_LMDB
			mdb_env_close(conv->dbenv);
#else
			conv->dbenv->close(conv->dbenv, 0);
#endif
		}
		// close files and clean up converters, if any
		if (conv->adif) tqsl_endADIF(&conv->adif);
		if (conv->cab) tqsl_endCabrillo(&conv->cab);
		if (conv->errfile) fclose(conv->errfile);
	}

	conv->db_open = false;
	return;
}

static const char* get_ident_rec(TQSL_CONVERTER *conv) {
	int major = 0, minor = 0, config_major = 0, config_minor = 0;

	tqsl_getVersion(&major, &minor);
	tqsl_getConfigVersion(&config_major, &config_minor);
	char temp[512];
	static char ident[512];
	snprintf(temp, sizeof temp, "%s Lib: V%d.%d Config: V%d.%d AllowDupes: %s",
		conv->appName ? conv->appName : "Unknown",
		major, minor, config_major, config_minor,
		conv->allow_dupes ? "true" : "false");
	temp[sizeof temp - 1] = '\0';
	int len = strlen(temp);
	snprintf(ident, sizeof ident, "<TQSL_IDENT:%d>%s\n", len, temp);
	ident[sizeof ident - 1] = '\0';
	conv->need_ident_rec = false;
	return ident;
}

static const char* get_station_rec(TQSL_CONVERTER *conv) {
	conv->need_station_rec = false;
	const char *tStation = tqsl_getGABBItSTATION(conv->loc, conv->loc_uid, conv->cert_uid);
	tqsl_getCertificateSerialExt(conv->certs[conv->cert_idx], conv->serial, sizeof conv->serial);
	tqsl_getCertificateCallSign(conv->certs[conv->cert_idx], conv->callsign, sizeof conv->callsign);
	tqsl_getCertificateDXCCEntity(conv->certs[conv->cert_idx], &conv->dxcc);
	return tStation;
}

static bool set_tagline(TQSL_CONVERTER *conv, const char *tag) {
	if (conv->taglines.find(tag) != conv->taglines.end()) {
		conv->err_tag_line = conv->taglines[tag];
		return true;
	}
	return false;
}

static void parse_adif_qso(TQSL_CONVERTER *conv, int *saveErr, TQSL_ADIF_GET_FIELD_ERROR *stat) {
	int cstat = 0;

	conv->taglines.clear();
	conv->err_tag_line = 0;
	while (1) {
		tqsl_adifFieldResults result;
		conv->taglines[result.name] = result.line_no;

		if (tqsl_getADIFField(conv->adif, &result, stat, adif_qso_record_fields, notypes, adif_allocate))
			break;
		if (*stat != TQSL_ADIF_GET_FIELD_SUCCESS && *stat != TQSL_ADIF_GET_FIELD_NO_NAME_MATCH)
			break;
		if (!strcasecmp(result.name, "eor"))
			break;
		char *resdata = reinterpret_cast<char *>(result.data);
		// Strip leading whitespace
		if (resdata) {
			while (isspace(*resdata))
				resdata++;

			// Strip trailing whitespace
			char *end = resdata + strlen(resdata) - 1;
			while (isspace(*end))
				*end-- = '\0';
		}

		if (!strcasecmp(result.name, "CALL") && resdata) {
			tqsl_strtoupper(resdata);
			conv->rec.callsign_set = true;
			strncpy(conv->rec.callsign, resdata, sizeof conv->rec.callsign);
		} else if (!strcasecmp(result.name, "BAND") && resdata) {
			conv->rec.band_set = true;
			strncpy(conv->rec.band, resdata, sizeof conv->rec.band);
		} else if (!strcasecmp(result.name, "MODE") && resdata) {
			conv->rec.mode_set = true;
			strncpy(conv->rec.mode, resdata, sizeof conv->rec.mode);
		} else if (!strcasecmp(result.name, "SUBMODE") && resdata) {
			strncpy(conv->rec.submode, resdata, sizeof conv->rec.submode);
		} else if (!strcasecmp(result.name, "FREQ") && resdata) {
			conv->rec.band_set = true;
			strncpy(conv->rec.freq, fix_freq(resdata), sizeof conv->rec.freq);
			if (atof(conv->rec.freq) == 0.0)
				conv->rec.freq[0] = '\0';
		} else if (!strcasecmp(result.name, "FREQ_RX") && resdata) {
			strncpy(conv->rec.rxfreq, fix_freq(resdata), sizeof conv->rec.rxfreq);
			if (atof(conv->rec.rxfreq) == 0.0)
				conv->rec.rxfreq[0] = '\0';
		} else if (!strcasecmp(result.name, "BAND_RX") && resdata) {
			strncpy(conv->rec.rxband, resdata, sizeof conv->rec.rxband);
		} else if (!strcasecmp(result.name, "SAT_NAME") && resdata) {
			// Two-Line Elements (TLEs) call this AO-07, LoTW wants AO-7.
			if (!strcasecmp(resdata, "AO-07"))
				strncpy(conv->rec.satname, "AO-7", sizeof conv->rec.satname);
			else
				strncpy(conv->rec.satname, resdata, sizeof conv->rec.satname);
		} else if (!strcasecmp(result.name, "PROP_MODE") && resdata) {
			strncpy(conv->rec.propmode, resdata, sizeof conv->rec.propmode);
		} else if (!strcasecmp(result.name, "QSO_DATE") && resdata) {
			conv->rec.date_set = true;
			cstat = tqsl_initDate(&(conv->rec.date), resdata);
			if (cstat)
				*saveErr = tQSL_Error;
		} else if (!strcasecmp(result.name, "TIME_ON") && resdata) {
			conv->rec.time_set = true;
			cstat = tqsl_initTime(&(conv->rec.time), resdata);
			if (cstat)
				*saveErr = tQSL_Error;
			if (conv->ignore_secs)
				conv->rec.time.second = 0;
		} else if (!strcasecmp(result.name, "MY_CNTY") && resdata) {
			char *p = strstr(resdata, ",");			// Find the comma in "VA,Fairfax"
			if (p) {
				*p++ = '\0';
				strncpy(conv->rec.my_cnty_state, resdata, sizeof conv->rec.my_cnty_state);
				while (isspace(*p)) p++;		// Skip spaces and comma
				strncpy(conv->rec.my_county, p, sizeof conv->rec.my_county);
			} else {
				strncpy(conv->rec.my_county, resdata, sizeof conv->rec.my_county);
			}
		} else if (!strcasecmp(result.name, "MY_COUNTRY") && resdata) {
			strncpy(conv->rec.my_country, resdata, sizeof conv->rec.my_country);
		} else if (!strcasecmp(result.name, "MY_CQ_ZONE") && resdata) {
			char *endptr;
			long zone = strtol(resdata, &endptr, 10);
			if (*endptr == '\0') { 	// If the conversion was correct
				snprintf(conv->rec.my_cq_zone, sizeof conv->rec.my_cq_zone, "%ld", zone);
			} else {		// It wasn't a valid number
				strncpy(conv->rec.my_cq_zone, resdata, sizeof conv->rec.my_cq_zone);
			}
		} else if (!strcasecmp(result.name, "MY_DXCC") && resdata) {
			conv->rec.my_dxcc = strtol(resdata, NULL, 10);
		} else if (!strcasecmp(result.name, "MY_GRIDSQUARE") && resdata) {
			strncpy(conv->rec.my_gridsquare, resdata, sizeof conv->rec.my_gridsquare);
		} else if (!strcasecmp(result.name, "MY_IOTA") && resdata) {
			strncpy(conv->rec.my_iota, resdata, sizeof conv->rec.my_iota);
		} else if (!strcasecmp(result.name, "MY_ITU_ZONE") && resdata) {
			char *endptr;
			long zone = strtol(resdata, &endptr, 10);
			if (*endptr == '\0') { 	// If the conversion was correct
				snprintf(conv->rec.my_itu_zone, sizeof conv->rec.my_itu_zone, "%ld", zone);
			} else {		// It wasn't a valid number
				strncpy(conv->rec.my_itu_zone, resdata, sizeof conv->rec.my_itu_zone);
			}
		} else if (!strcasecmp(result.name, "MY_STATE") && resdata) {
			strncpy(conv->rec.my_state, resdata, sizeof conv->rec.my_state);
		} else if (!strcasecmp(result.name, "MY_VUCC_GRIDS") && resdata) {
			strncpy(conv->rec.my_vucc_grids, resdata, sizeof conv->rec.my_vucc_grids);
		} else if (!strcasecmp(result.name, "OPERATOR") && resdata) {
			// Only use the OPERATOR field if it looks like a callsign
			tqsl_strtoupper(resdata);
			string op(resdata);
			if (checkCallSign(op)) {
				strncpy(conv->rec.my_operator, resdata, sizeof conv->rec.my_operator);
			}
#ifdef USE_OWNER_CALLSIGN
		} else if (!strcasecmp(result.name, "OWNER_CALLSIGN") && resdata) {
			// Only use the OWNER_CALLSIGN field if it looks like a callsign
			tqsl_strtoupper(resdata);
			string op(resdata);
			if (checkCallSign(op)) {
				strncpy(conv->rec.my_owner, resdata, sizeof conv->rec.my_owner);
			}
#endif
		} else if (!strcasecmp(result.name, "STATION_CALLSIGN") && resdata) {
			// Only use the STATION_CALLSIGN field if it looks like a callsign
			tqsl_strtoupper(resdata);
			string op(resdata);
			if (checkCallSign(op)) {
				strncpy(conv->rec.my_call, resdata, sizeof conv->rec.my_call);
			}
		} else {
			tqslTrace("parse_adif_qso", "Unknown ADIF field %s", result.name);
		}

		if (*stat == TQSL_ADIF_GET_FIELD_SUCCESS) {
			conv->rec_text += string(reinterpret_cast<char *>(result.name)) + ": ";
			if (resdata)
				conv->rec_text += string(resdata);
			conv->rec_text += "\n";
		}
		if (result.data)
			delete[] result.data;
	}
	return;
}

static int check_station(TQSL_CONVERTER *conv, const char *field, char *my, size_t len, const char *errfmt, bool update) {
//
// UPDATE is a boolean that when a change is made, that change
// is propagated to the downstream values. STATE -> COUNTY and STATE->ZONES
//
	char val[256];
	char label[256];
	bool provinceFixed = false;
	bool oblastFixed = false;
	// CA_PROVINCE can be QC but TQSL lookup expects PQ
	if (!strcasecmp(field, "CA_PROVINCE") && !strcasecmp(my, "QC")) {
		provinceFixed = true;
		strncpy(my, "PQ", len);
	}

	// RU_OBLAST can be YR but TQSL lookup expects JA
	if (!strcasecmp(field, "RU_OBLAST") && !strcasecmp(my, "YR")) {
		oblastFixed = true;
		strncpy(my, "JA", len);
	}
	// RU_OBLAST can be YN but TQSL lookup expects JN
	if (!strcasecmp(field, "RU_OBLAST") && !strcasecmp(my, "YN")) {
		oblastFixed = true;
		strncpy(my, "JN", len);
	}

	if (my[0] && !tqsl_getLocationField(conv->loc, field, val, sizeof val) &&
		     !tqsl_getLocationFieldLabel(conv->loc, field, label, sizeof label)) {
		if (!strcasecmp(my, label)) {			// Label is correct, ADIF is not
			strncpy(my, val, len);		// So use the value
		}
		if (strcasecmp(my, val)) {
			if (conv->location_handling == TQSL_LOC_UPDATE) {
				int res = tqsl_setLocationField(conv->loc, field, my);
				// -1 means trying to set a value that is not in the enumeration
				if (res == -1) {
					conv->rec_done = true;
					snprintf(tQSL_CustomError, sizeof tQSL_CustomError, errfmt, my, val);
					tQSL_Error = TQSL_LOCATION_MISMATCH | 0x1000;
					set_tagline(conv, field);
					return 1;
				}
				// -2 means that the label matched, so use that
				if (res == -2) {
					strncpy(my, tQSL_CustomError, len);
				}
				if (update) tqsl_updateStationLocationCapture(conv->loc);
				conv->newstation = true;
			} else if (strlen(val) > 0) {
				conv->rec_done = true;
				if (provinceFixed) {
					strncpy(my, "QC", len);
				}
				if (oblastFixed) {
					strncpy(my, "YR", len);
				}
				snprintf(tQSL_CustomError, sizeof tQSL_CustomError, errfmt, val, my);
				tQSL_Error = TQSL_LOCATION_MISMATCH;
				set_tagline(conv, field);
				return 1;
			} else {
				tqsl_setLocationField(conv->loc, field, my);
				if (update) tqsl_updateStationLocationCapture(conv->loc);
				conv->newstation = true;
			}
		}
	}
	return 0;	// OK
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getConverterGABBI(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;
	char signdata[1024];
	int cstat = 0;

	if (!(conv = check_conv(convp)))
		return 0;

	if (conv->need_ident_rec) {
		return get_ident_rec(conv);
	}

	if (!conv->allow_dupes && !conv->db_open) {
		if (!open_db(conv, false)) {	// If can't open dupes DB
			return 0;
		}
	}

	TQSL_ADIF_GET_FIELD_ERROR stat;

	if (conv->rec_done) {
		conv->rec_done = false;
		conv->clearRec();
		int saveErr = 0;
		if (conv->adif) {
			parse_adif_qso(conv, &saveErr, &stat);
			if (saveErr) {
				tQSL_Error = saveErr;
				conv->rec_done = true;
				return 0;
			}
			if (stat == TQSL_ADIF_GET_FIELD_EOF)
				return 0;
			if (stat != TQSL_ADIF_GET_FIELD_SUCCESS) {
				tQSL_ADIF_Error = stat;
				tQSL_Error = TQSL_ADIF_ERROR;
				return 0;
			}
			conv->err_tag_line = 0;

			// ADIF record is complete. See if we need to infer the BAND fields.
			if (conv->rec.band[0] == 0)
				strncpy(conv->rec.band, tqsl_infer_band(conv->rec.freq), sizeof conv->rec.band);
			if (conv->rec.rxband[0] == 0)
				strncpy(conv->rec.rxband, tqsl_infer_band(conv->rec.rxfreq), sizeof conv->rec.rxband);
			// Normalize the DXCC country
			if (conv->rec.my_country[0] != 0) {
				int num_dxcc = 0;
				tqsl_getNumDXCCEntity(&num_dxcc);
				const char *entity;
				int ent_num;
				for (int i = 0; i < num_dxcc; i++) {
					tqsl_getDXCCEntity(i, &ent_num, &entity);
					if (strcasecmp(entity, conv->rec.my_country) == 0) {
						// Consistent DXCC ?
						if (conv->rec.my_dxcc == 0) {
							conv->rec.my_dxcc = ent_num;
						} else {
							// MY_DXCC and MY_COUNTRY do not match. Report this.
							if (conv->rec.my_dxcc != ent_num) {
								conv->rec_done = true;
								const char *d1;
								tqsl_getDXCCEntityName(conv->rec.my_dxcc, &d1);

								snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "DXCC Entity|%s (%d)|%s (%d)", d1, conv->rec.my_dxcc, conv->rec.my_country, i);
								tQSL_Error = TQSL_CERT_MISMATCH;
								return 0;
							}
						}
						break;
					}
				}
			}
			// Normalize the grids
			if (conv->rec.my_vucc_grids[0] != 0) {
				strncpy(conv->rec.my_gridsquare, conv->rec.my_vucc_grids, TQSL_GRID_MAX);
			}
			// Normalize callsign
			// Priority - STATION_CALLSIGN, then OPERATOR, then OWNER_CALLSIGN
			// my_call has STATION_CALLSIGN already.
			if (conv->rec.my_call[0] == '\0' && conv->rec.my_operator[0] != 0) {		// OPERATOR set
				strncpy(conv->rec.my_call, conv->rec.my_operator, TQSL_CALLSIGN_MAX);
			}
#ifdef USE_OWNER_CALLSIGN
			if (conv->rec.my_call[0] == '\0' && conv->rec.my_owner[0] != 0) {		// OWNER_CALLSIGN set
				strncpy(conv->rec.my_call, conv->rec.my_owner, TQSL_CALLSIGN_MAX);
			}
#endif
			if (conv->location_handling == TQSL_LOC_UPDATE && conv->rec.my_call[0] != '\0') {						// If any of these
				strncpy(conv->callsign, conv->rec.my_call, sizeof conv->callsign);	// got a callsign
			}
		} else if (conv->cab) {
			TQSL_CABRILLO_ERROR_TYPE stat;
			do {
				tqsl_cabrilloField field;
				if (tqsl_getCabrilloField(conv->cab, &field, &stat))
					return 0;
				if (stat == TQSL_CABRILLO_NO_ERROR || stat == TQSL_CABRILLO_EOR) {
					// Field found
					if (!strcasecmp(field.name, "CALL")) {
						conv->rec.callsign_set = true;
						strncpy(conv->rec.callsign, field.value, sizeof conv->rec.callsign);
					} else if (!strcasecmp(field.name, "BAND")) {
						conv->rec.band_set = true;
						strncpy(conv->rec.band, field.value, sizeof conv->rec.band);
					} else if (!strcasecmp(field.name, "MODE")) {
						conv->rec.mode_set = true;
						strncpy(conv->rec.mode, field.value, sizeof conv->rec.mode);
					} else if (!strcasecmp(field.name, "FREQ")) {
						conv->rec.band_set = true;
						strncpy(conv->rec.freq, field.value, sizeof conv->rec.freq);
					} else if (!strcasecmp(field.name, "QSO_DATE")) {
						conv->rec.date_set = true;
						cstat = tqsl_initDate(&(conv->rec.date), field.value);
						if (cstat)
							saveErr = tQSL_Error;
					} else if (!strcasecmp(field.name, "TIME_ON")) {
						conv->rec.time_set = true;
						cstat = tqsl_initTime(&(conv->rec.time), field.value);
						if (conv->ignore_secs)
							conv->rec.time.second = 0;
						if (cstat)
							saveErr = tQSL_Error;
					} else if (!strcasecmp(field.name, "MYCALL")) {
						strncpy(conv->rec.my_call, field.value, sizeof conv->rec.my_call);
						tqsl_strtoupper(conv->rec.my_call);
					}
					if (conv->rec_text != "")
						conv->rec_text += "\n";
					conv->rec_text += string(field.name) + ": " + field.value;
				}
			} while (stat == TQSL_CABRILLO_NO_ERROR);
			if (saveErr)
				tQSL_Error = saveErr;
			if (saveErr || stat != TQSL_CABRILLO_EOR) {
				conv->rec_done = true;
				return 0;
			}
		} else {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, "Converter not initialized", sizeof tQSL_CustomError);
			tqslTrace("tqsl_getConverterGABBI", "Converter not initialized");
			return 0;
		}
	}
	// Does the QSO have the basic required elements?
	if (!conv->rec.callsign_set) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid contact - QSO does not specify a Callsign");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (!conv->rec.band_set) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid contact - QSO does not specify a band or frequency");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (!conv->rec.mode_set) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid contact - QSO does not specify a mode");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (!conv->rec.date_set) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid contact - QSO does not specify a date");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (!conv->rec.time_set) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid contact - QSO does not specify a time");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}

	// Check QSO date against user-specified date range.
	if (tqsl_isDateValid(&(conv->rec.date))) {
		if (tqsl_isDateValid(&(conv->start)) && tqsl_compareDates(&(conv->rec.date), &(conv->start)) < 0) {
			conv->rec_done = true;
			tQSL_Error = TQSL_DATE_OUT_OF_RANGE;
			set_tagline(conv, "QSO_DATE");
			return 0;
		}
		if (tqsl_isDateValid(&(conv->end)) && tqsl_compareDates(&(conv->rec.date), &(conv->end)) > 0) {
			conv->rec_done = true;
			tQSL_Error = TQSL_DATE_OUT_OF_RANGE;
			set_tagline(conv, "QSO_DATE");
			return 0;
		}
	}

	// Do field value mapping
	tqsl_strtoupper(conv->rec.callsign);
	if (!conv->allow_bad_calls) {
		if (!checkCallSign(conv->rec.callsign)) {
			conv->rec_done = true;
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid amateur CALL (%s)", conv->rec.callsign);
			set_tagline(conv, "CALL");
			tQSL_Error = TQSL_CUSTOM_ERROR;
			return 0;
		}
	}
	tqsl_strtoupper(conv->rec.band);
	tqsl_strtoupper(conv->rec.rxband);
	tqsl_strtoupper(conv->rec.mode);
	tqsl_strtoupper(conv->rec.submode);
	char val[256];
	val[0] = '\0';

	// Try to find the GABBI mode several ways.
	if (conv->rec.submode[0] != '\0') {
		char modeSub[256];
		strncpy(modeSub, conv->rec.mode, sizeof modeSub);
		size_t left = sizeof modeSub - strlen(modeSub);
		strncat(modeSub, "%", left);
		left = sizeof modeSub - strlen(modeSub);
		strncat(modeSub, conv->rec.submode, left);
		if (tqsl_getADIFMode(modeSub, val, sizeof val)) {	// mode%submode lookup failed
			// Try just the submode.
			if (tqsl_getADIFMode(conv->rec.submode, val, sizeof val)) { // bare submode failed
				if (tqsl_getADIFMode(conv->rec.mode, val, sizeof val)) {
					val[0] = '\0';
				}
			}
		}
	} else {
		// Just a mode, no submode. Look that up.
		tqsl_getADIFMode(conv->rec.mode, val, sizeof val);
	}
	if (val[0] != '\0')
		strncpy(conv->rec.mode, val, sizeof conv->rec.mode);
	// Check field validities
	if (conv->modes.find(conv->rec.mode) == conv->modes.end()) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid MODE (%s)", conv->rec.mode);
		set_tagline(conv, "MODE");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (conv->bands.find(conv->rec.band) == conv->bands.end()) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid BAND (%s)", conv->rec.band);
		set_tagline(conv, "BAND");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (conv->rec.rxband[0] && (conv->bands.find(conv->rec.rxband) == conv->bands.end())) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid RX BAND (%s)", conv->rec.rxband);
		set_tagline(conv, "BAND_RX");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (conv->rec.freq[0] && strcmp(conv->rec.band, "SUBMM") && strcmp(conv->rec.band, tqsl_infer_band(conv->rec.freq))) {
		// Have a BAND and FREQ.
		// Frequency is not in that band - ignore it.
		conv->rec.freq[0] = '\0';
	}
	if (conv->rec.rxfreq[0] && strcmp(conv->rec.rxband, "SUBMM") && strcmp(conv->rec.rxband, tqsl_infer_band(conv->rec.rxfreq))) {
		// Have a RX_BAND and RX_FREQ. Frequency is not in that band - ignore it.
		conv->rec.rxfreq[0] = '\0';
	}
	if (conv->rec.propmode[0] != '\0'
		&& conv->propmodes.find(conv->rec.propmode) == conv->propmodes.end()) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid PROP_MODE (%s)", conv->rec.propmode);
		set_tagline(conv, "PROP_MODE");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (conv->rec.satname[0] != '\0'
		&& conv->satellites.find(conv->rec.satname) == conv->satellites.end()) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid SAT_NAME (%s)", conv->rec.satname);
		set_tagline(conv, "SAT_NAME");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (!strcmp(conv->rec.propmode, "SAT") && conv->rec.satname[0] == '\0') {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "PROP_MODE = 'SAT' but no SAT_NAME");
		set_tagline(conv, "PROP_MODE");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (strcmp(conv->rec.propmode, "SAT") && conv->rec.satname[0] != '\0') {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "SAT_NAME set but PROP_MODE is not 'SAT'");
		set_tagline(conv, "SAT_NAME");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}

	// Check cert
	if (conv->location_handling != TQSL_LOC_UPDATE && conv->ncerts <= 0) {
		conv->rec_done = true;
		tQSL_Error = TQSL_CERT_NOT_FOUND;
		return 0;
	}

	if (conv->location_handling == TQSL_LOC_UPDATE) {
		// Is the call right?
		if (conv->rec.my_call[0]) {
			strncpy(conv->callsign, conv->rec.my_call, sizeof conv->callsign);
		}
	}

	// For check-only case, need to check callsign now.
	if (conv->location_handling == TQSL_LOC_REPORT) {
		// Is the call right?
		if (conv->rec.my_call[0]) {		// Update case handled above when switching certs
			if (strcmp(conv->rec.my_call, conv->callsign)) {
				conv->rec_done = true;
				snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Callsign|%s|%s", conv->callsign, conv->rec.my_call);
				if (!set_tagline(conv, "STATION_CALLSIGN"))
					set_tagline(conv, "OPERATOR");
				tQSL_Error = TQSL_CERT_MISMATCH;
				return 0;
			}
		}
	}

	// Lookup cert - start with conv->dxcc
	int targetdxcc = conv->dxcc;

	// If we're in update mode, use the DXCC from the log
	if (conv->location_handling == TQSL_LOC_UPDATE) {
		if (conv->rec.my_dxcc != 0) {
			targetdxcc = conv->rec.my_dxcc;
		}
	}

	bool anyfound;
	int cidx = find_matching_cert(conv, targetdxcc, &anyfound);
	if (cidx < 0) {
		conv->rec_done = true;
		const char *entName;
		tqsl_getDXCCEntityName(targetdxcc, &entName);
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "%s|%s", conv->callsign, entName);
		tQSL_Error = TQSL_CERT_NOT_FOUND | 0x1000;
		if (anyfound) {
			tQSL_Error = TQSL_CERT_DATE_MISMATCH;
			set_tagline(conv, "QSO_DATE");
		}
		if (conv->location_handling == TQSL_LOC_UPDATE) {
			if (conv->rec.my_call[0]) {
				snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "%s|%s", conv->rec.my_call, entName);
			}
		}
		return 0;
	}
	if (cidx != conv->cert_idx) {
		// Switching certs
		if (conv->dxcc != -1) {
			conv->dxcc = targetdxcc;
			tqsl_setLocationCallSign(conv->loc, conv->callsign, conv->dxcc);	// Set callsign and DXCC
        		tqsl_setStationLocationCapturePage(conv->loc, 1);	// Update to relevant fields
			tqsl_updateStationLocationCapture(conv->loc);
		}
		conv->cert_idx = cidx;
		if (conv->cert_uids[conv->cert_idx] == -1) {
			// Need to output tCERT, tSTATION
			conv->need_station_rec = true;		// Need a new station record
			conv->cert_uid = conv->cert_uids[conv->cert_idx] = conv->next_cert_uid;
			conv->next_cert_uid++;
			return tqsl_getGABBItCERT(conv->certs[conv->cert_idx], conv->cert_uid);
		} else {
			conv->cert_uid = conv->cert_uids[conv->cert_idx];
		}
	}

	if (conv->location_handling != TQSL_LOC_IGNORE) { // Care about MY_* fields
		// At this point, conv->certs[conv->cert_idx] has the certificate
		// conv->loc has the location.
		// First, refresh the certificate data
		tqsl_getCertificateSerialExt(conv->certs[conv->cert_idx], conv->serial, sizeof conv->serial);
		tqsl_getCertificateCallSign(conv->certs[conv->cert_idx], conv->callsign, sizeof conv->callsign);
		tqsl_getCertificateDXCCEntity(conv->certs[conv->cert_idx], &conv->dxcc);

		// Is the call right?
		if (conv->rec.my_call[0]) {		// Update case handled above when switching certs
			if (strcmp(conv->rec.my_call, conv->callsign)) {
				conv->rec_done = true;
				snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Callsign|%s|%s", conv->callsign, conv->rec.my_call);
				if (!set_tagline(conv, "STATION_CALLSIGN"))
					set_tagline(conv, "OPERATOR");
				tQSL_Error = TQSL_CERT_MISMATCH;
				return 0;
			}
		}

		// Is the DXCC right?
		if (conv->rec.my_dxcc) {
			if (conv->rec.my_dxcc != conv->dxcc) {
				if (conv->location_handling == TQSL_LOC_UPDATE) { // Care about MY_* fields
					tqsl_setLocationField(conv->loc, "CALL", conv->callsign);
					tqsl_updateStationLocationCapture(conv->loc);
				} else {
					conv->rec_done = true;
					const char *d1, *d2;
					tqsl_getDXCCEntityName(conv->dxcc, &d1);
					tqsl_getDXCCEntityName(conv->rec.my_dxcc, &d2);

					snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "DXCC Entity|%s (%d)|%s (%d)", d1, conv->dxcc, d2, conv->rec.my_dxcc);
					set_tagline(conv, "MY_DXCC");
					tQSL_Error = TQSL_CERT_MISMATCH;
					return 0;
				}
			}
		}

		conv->newstation = false;
		/*
		 * Gridsquare handling - if the four-character grid matches the station loc
		 * then don't complain; this is common for FT8/FT4 which have four char grids
		 * and we don't want to reject every WSJT-X QSO just because the station
		 * location has a higher precision grid. Similarly, if the Station Location has
		 * a six-character grid and the log has 8, then just compare 6 for report mode.
		 */

		tqsl_getLocationField(conv->loc, "GRIDSQUARE", val, sizeof val);
		if (conv->rec.my_gridsquare[0] && !tqsl_getLocationField(conv->loc, "GRIDSQUARE", val, sizeof val)) {
			bool okgrid = true;
			unsigned int stnLen = strlen(val);
			unsigned int logLen = strlen(conv->rec.my_gridsquare);
			unsigned int compareLen = (stnLen < logLen ? stnLen : logLen);
			if (strstr(val, ",") || strstr(conv->rec.my_gridsquare, ",")) {	// If it's a corner/edge
				vector<string>stngrids;
				vector<string>qsogrids;
				splitStr(val, stngrids, ',');
				splitStr(conv->rec.my_gridsquare, qsogrids, ',');
				size_t nstn = stngrids.size();
				size_t nqso = qsogrids.size();
				if (nstn != nqso) {
				    okgrid = false;
				} else {
					sort(stngrids.begin(), stngrids.end());
					sort(qsogrids.begin(), qsogrids.end());
					for (size_t i = 0; i < nstn; i++) {
						if (stngrids[i] != qsogrids[i]) {
							compareLen = 99; // Doesn't match, so error out if appropriate
							break;
						}
					}
					compareLen = 0;			// Matches.
				}
			}
			if (conv->location_handling == TQSL_LOC_UPDATE) {
				okgrid = (strcasecmp(conv->rec.my_gridsquare, val) == 0);
			} else {
				okgrid = (compareLen == 0 || strncasecmp(conv->rec.my_gridsquare, val, compareLen) == 0);
			}
			if (!okgrid) {
				if (conv->location_handling == TQSL_LOC_UPDATE) {
					tqsl_setLocationField(conv->loc, "GRIDSQUARE", conv->rec.my_gridsquare);
					conv->newstation = true;
				} else {
					if (val[0] == '\0') {		// If station location has an empty grid
						tqsl_setLocationField(conv->loc, "GRIDSQUARE", conv->rec.my_gridsquare);
						conv->newstation = true;
					} else {
						conv->rec_done = true;
						snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Gridsquare|%s|%s", val, conv->rec.my_gridsquare);
						set_tagline(conv, "GRIDSQUARE");
						tQSL_Error = TQSL_LOCATION_MISMATCH;
						return 0;
					}
				}
			}
		}

		switch (conv->dxcc) {
			case 6:		// Alaska
			case 110:	// Hawaii
			case 291:	// Cont US
				if (check_station(conv, "US_STATE", conv->rec.my_state, sizeof conv->rec.my_state, "US State|%s|%s", true)) return 0;
				if (check_station(conv, "US_COUNTY", conv->rec.my_county, sizeof conv->rec.my_county, "US County|%s|%s", false)) return 0;
				break;
			case 1:		// Canada
				if (check_station(conv, "CA_PROVINCE", conv->rec.my_state, sizeof conv->rec.my_state, "CA Province|%s|%s", true)) return 0;
				break;
			case 15:	// Asiatic Russia
			case 54:	// European Russia
			case 61:	// FJL
			case 125:	// Juan Fernandez
			case 151:	// Malyj Vysotskij
				if (check_station(conv, "RU_OBLAST", conv->rec.my_state, sizeof conv->rec.my_state, "RU Oblast|%s|%s", true)) return 0;
				break;
			case 318:	// China
				if (check_station(conv, "CN_PROVINCE", conv->rec.my_state, sizeof conv->rec.my_state, "CN Province|%s|%s", true)) return 0;
				break;
			case 150:	// Australia
				if (check_station(conv, "AU_STATE", conv->rec.my_state, sizeof conv->rec.my_state, "AU State|%s|%s", true)) return 0;
				break;
			case 339:	// Japan
				if (check_station(conv, "JA_PREFECTURE", conv->rec.my_state, sizeof conv->rec.my_state, "JA Prefecture|%s|%s", true)) return 0;
				if (check_station(conv, "JA_CITY_GUN_KU", conv->rec.my_county, sizeof conv->rec.my_county, "JA City/Gun/Ku|%s|%s", false)) return 0;
				break;
			case 5:		// Finland
				if (check_station(conv, "FI_KUNTA", conv->rec.my_state, sizeof conv->rec.my_state, "FI Kunta|%s|%s", true)) return 0;
				break;
		}

		if (check_station(conv, "ITUZ", conv->rec.my_itu_zone, sizeof conv->rec.my_itu_zone, "ITU Zone|%s|%s", false)) return 0;
		if (check_station(conv, "CQZ", conv->rec.my_cq_zone, sizeof conv->rec.my_cq_zone, "CQ Zone|%s|%s", false)) return 0;
		if (check_station(conv, "IOTA", conv->rec.my_iota, sizeof conv->rec.my_iota, "IOTA|%s|%s", false)) return 0;

		if (conv->rec.my_cnty_state[0] != '\0') {
			char locstate[5];
			tqsl_getLocationField(conv->loc, "US_STATE", locstate, sizeof locstate);
			if (strcasecmp(conv->rec.my_cnty_state, locstate)) {		// County does not match state
				conv->rec_done = true;
				snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "US County State|%s|%s", conv->rec.my_cnty_state, locstate);
				set_tagline(conv, "US_STATE");
				tQSL_Error = TQSL_LOCATION_MISMATCH | 0x1000;
				return 0;
			}
		}
		if (conv->newstation) {
			conv->newstation = false;
			conv->loc_uid++;
			return get_station_rec(conv);
		}
	}	// if ignoring MY_ fields

	if (conv->need_station_rec) {
		conv->loc_uid++;
		return get_station_rec(conv);
	}

	const char *grec = tqsl_getGABBItCONTACTData(conv->certs[conv->cert_idx], conv->loc, &(conv->rec),
		conv->loc_uid, signdata, sizeof(signdata));
	if (grec) {
		conv->rec_done = true;
		if (!conv->allow_dupes) {
			char stnloc[128];
			char qso[128];
			if (tqsl_getLocationStationDetails(conv->loc, stnloc, sizeof stnloc)) {
				stnloc[0] = '\0';
			}
			if (tqsl_getLocationQSODetails(conv->loc, qso, sizeof qso)) {
				qso[0] = '\0';
			}
			// Old-style Lookup uses signdata and cert serial number
#ifdef USE_LMDB
			MDB_val dbkey, dbdata;
#else
			DBT dbkey, dbdata;
			memset(&dbkey, 0, sizeof dbkey);
			memset(&dbdata, 0, sizeof dbdata);
#endif
			// append signing key serial
			strncat(signdata, conv->serial, sizeof(signdata) - strlen(signdata)-1);
			// Updated dupe database entry. Key is formed from
			// local callsign concatenated with the QSO details
			char dupekey[128];
			snprintf(dupekey, sizeof dupekey, "%s%s", conv->callsign, qso);
#ifdef USE_LMDB
			dbkey.mv_size = strlen(signdata);
			dbkey.mv_data = signdata;
			int dbget_err = mdb_get(conv->txn, conv->seendb, &dbkey, &dbdata);
#else
			dbkey.size = strlen(signdata);
			dbkey.data = signdata;
			int dbget_err = conv->seendb->get(conv->seendb, conv->txn, &dbkey, &dbdata, 0);
#endif
			if (0 == dbget_err) {
				//lookup was successful; thus this is a duplicate.
				tqslTrace("tqsl_getConverterGABBI", "Duplicate QSO signdata=%s", signdata);
				tQSL_Error = TQSL_DUPLICATE_QSO;
				tQSL_CustomError[0] = '\0';
				// delete the old record

				int dbput_err;
#ifdef USE_LMDB
				mdb_del(conv->txn, conv->seendb, &dbkey, &dbdata);
				// Update this to the current format
				dbkey.mv_size = strlen(dupekey);
				dbkey.mv_data = dupekey;
				dbdata.mv_data = stnloc;
				dbdata.mv_size = strlen(stnloc);
				dbput_err = mdb_put(conv->txn, conv->seendb, &dbkey, &dbdata, 0);
#else
				conv->seendb->del(conv->seendb, conv->txn, &dbkey, 0);
				// Update this to the current format
				memset(&dbkey, 0, sizeof dbkey);
				dbkey.size = strlen(dupekey);
				dbkey.data = dupekey;
				memset(&dbdata, 0, sizeof dbdata);
				dbdata.data = stnloc;
				dbdata.size = strlen(stnloc);
				dbput_err = conv->seendb->put(conv->seendb, conv->txn, &dbkey, &dbdata, 0);
#endif
				if (0 != dbput_err) {
					strncpy(tQSL_CustomError, db_strerror(dbput_err), sizeof tQSL_CustomError);
					if (dbput_err == EINVAL) {
						close_db(conv);
						remove_db(conv->dbpath);
						free(conv->dbpath);
					}
					tQSL_Error = TQSL_DB_ERROR;
					return 0;
				}
				return 0;
#ifdef USE_LMDB
			} else if (dbget_err != MDB_NOTFOUND) {
#else
			} else if (dbget_err != DB_NOTFOUND) {
#endif
				//non-zero return, but not "not found" - thus error
				strncpy(tQSL_CustomError, db_strerror(dbget_err), sizeof tQSL_CustomError);
				if (dbget_err == EINVAL) {
					close_db(conv);
					remove_db(conv->dbpath);
					free(conv->dbpath);
				}
				tQSL_Error = TQSL_DB_ERROR;
				return 0;
				// could be more specific but there's very little the user can do at this point anyway
			}
#ifdef USE_LMDB
			dbkey.mv_size = strlen(dupekey);
			dbkey.mv_data = dupekey;
			dbget_err = mdb_get(conv->txn, conv->seendb, &dbkey, &dbdata);
#else
			memset(&dbkey, 0, sizeof dbkey);
			memset(&dbdata, 0, sizeof dbdata);

			dbkey.size = strlen(dupekey);
			dbkey.data = dupekey;
			dbget_err = conv->seendb->get(conv->seendb, conv->txn, &dbkey, &dbdata, 0);
#endif
			if (0 == dbget_err) {
				//lookup was successful; thus this is a duplicate.
				tqslTrace("tqsl_getConverterGABBI", "Duplicate QSO dupekey=%s", dupekey);
				tQSL_Error = TQSL_DUPLICATE_QSO;
				// Save the original and new station location details so those can be provided
				// with an error by the caller
#ifdef USE_LMDB
				char *olddup = reinterpret_cast<char *> (malloc(dbdata.mv_size + 2));
				memcpy(olddup, dbdata.mv_data, dbdata.mv_size);
				olddup[dbdata.mv_size] = '\0';
#else
				char *olddup = reinterpret_cast<char *> (malloc(dbdata.size + 2));
				memcpy(olddup, dbdata.data, dbdata.size);
				olddup[dbdata.size] = '\0';
#endif
				// here olddup = "GRIDSQUARE: ML01OX", stnloc "GRIDSQUARE: MLO2oa".
				// Station loc details like "CQZ: 5, GRIDSQUARE: FM18ju, ITUZ: 8, US_COUNTY: Fairfax, US_STATE: VA"

				// If the same, it's just a dupe.
				if (!strcmp(olddup, stnloc)) {
					snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "%s|%s", olddup, stnloc);
					free(olddup);
					return 0;
				}
				// Strip spaces
				string olds = olddup;
				size_t found = olds.find(' ');
				while (found != string::npos) {
					olds.replace(found, 1, "");
					found = olds.find(' ');
				}
				string news = stnloc;
				found = news.find(' ');
				while (found != string::npos) {
					news.replace(found, 1, "");
					found = news.find(' ');
				}
				// Iterate the previous and current station locations.
				vector<string>oldstn;
				vector<string>qsostn;
				splitStr(olds, oldstn, ',');
				splitStr(news, qsostn, ',');
				// What we have now is the vectors having "GRIDSQUARE:ML10X" in each entry. Look for changes.
				bool changed = false;
				for (size_t i = 0; i < oldstn.size(); i++) {
					size_t cp = oldstn[i].find(":");
					string key = oldstn[i].substr(0, cp+1);
					for (size_t j = 0; j < qsostn.size(); j++) {
						cp = qsostn[j].find(":");
						string qsokey = qsostn[j].substr(0, cp+1);
						// Finally - is the key the same?
						if (key == qsokey) {
							if (oldstn[i] != qsostn[j] && key != oldstn[i]) {
								changed = true;
							}
							break;
						}
					}
					if (changed)
						break;
				}
				if (changed) {
					snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "%s|%s", olddup, stnloc);
					free(olddup);
					return 0;
				}
				free(olddup);
				// This is a valid update, delete the old one and let it update.
#ifdef USE_LMDB
				dbkey.mv_size = strlen(dupekey);
				dbkey.mv_data = dupekey;
				int dbdel_err = mdb_del(conv->txn, conv->seendb, &dbkey, &dbdata);
			} else if (dbget_err != MDB_NOTFOUND) {
#else
				memset(&dbkey, 0, sizeof dbkey);
				dbkey.size = strlen(dupekey);
				dbkey.data = dupekey;
				conv->seendb->del(conv->seendb, conv->txn, &dbkey, 0);
			} else if (dbget_err != DB_NOTFOUND) {
#endif
				//non-zero return, but not "not found" - thus error
				strncpy(tQSL_CustomError, db_strerror(dbget_err), sizeof tQSL_CustomError);
				if (dbget_err == EINVAL) {
					close_db(conv);
					remove_db(conv->dbpath);
					free(conv->dbpath);
				}
				tQSL_Error = TQSL_DB_ERROR;
				return 0;
				// could be more specific but there's very little the user can do at this point anyway
			}

			int dbput_err;
#ifdef USE_LMDB
			dbdata.mv_data = stnloc;
			dbdata.mv_size = strlen(stnloc);
			dbput_err = mdb_put(conv->txn, conv->seendb, &dbkey, &dbdata, 0);
#else
			memset(&dbdata, 0, sizeof dbdata);
			dbdata.data = stnloc;
			dbdata.size = strlen(stnloc);
			dbput_err = conv->seendb->put(conv->seendb, conv->txn, &dbkey, &dbdata, 0);
#endif
			if (0 != dbput_err) {
				strncpy(tQSL_CustomError, db_strerror(dbput_err), sizeof tQSL_CustomError);
				if (dbput_err == EINVAL) {
					close_db(conv);
					remove_db(conv->dbpath);
					free(conv->dbpath);
				}
				tQSL_Error = TQSL_DB_ERROR;
				return 0;
			}
		}
	}
	return grec;
} // NOLINT(readability/fn_size)

DLLEXPORT int CALLCONVENTION
tqsl_getConverterCert(tQSL_Converter convp, tQSL_Cert *certp) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	if (certp == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*certp = conv->certs[conv->cert_idx];
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getConverterLine(tQSL_Converter convp, int *lineno) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	if (lineno == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (conv->err_tag_line) {
		*lineno = conv->err_tag_line;
		return 0;
	}
	if (conv->cab)
		return tqsl_getCabrilloLine(conv->cab, lineno);
	else if (conv->adif)
		return tqsl_getADIFLine(conv->adif, lineno);
	*lineno = 0;
	return 0;
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getConverterRecordText(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 0;
	return conv->rec_text.c_str();
}

DLLEXPORT int CALLCONVENTION
tqsl_setConverterAllowBadCall(tQSL_Converter convp, int allow) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	conv->allow_bad_calls = (allow != 0);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setConverterAllowDuplicates(tQSL_Converter convp, int allow) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	conv->allow_dupes = (allow != 0);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setConverterIgnoreSeconds(tQSL_Converter convp, int ignore) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	conv->ignore_secs = (ignore != 0);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setConverterAppName(tQSL_Converter convp, const char *app) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	if (!app) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	conv->appName = strdup(app);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setConverterQTHDetails(tQSL_Converter convp, int logverify) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	conv->location_handling = logverify;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_converterRollBack(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;

	tqslTrace("tqsl_converterRollBack", NULL);
	if (!(conv = check_conv(convp)))
		return 1;
	if (!conv->db_open)
		return 0;
	if (conv->txn)
#ifdef USE_LMDB
		mdb_txn_abort(conv->txn);
#else
		conv->txn->abort(conv->txn);
#endif
	conv->txn = NULL;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_converterCommit(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;

	tqslTrace("tqsl_converterCommit", NULL);
	if (!(conv = check_conv(convp)))
		return 1;
	if (!conv->db_open)
		return 0;
	if (conv->txn)
#ifdef USE_LMDB
		mdb_txn_commit(conv->txn);
#else
		conv->txn->commit(conv->txn, 0);
#endif
	conv->txn = NULL;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDuplicateRecords(tQSL_Converter convp, char *key, char *data, int keylen) {
	TQSL_CONVERTER *conv;

	if (!(conv = check_conv(convp)))
		return 1;

	if (!conv->db_open) {
		if (!open_db(conv, true)) {	// If can't open dupes DB
			return 1;
		}
	}
	if (!conv->cursor) {
#ifdef USE_LMDB
		int err = mdb_cursor_open(conv->txn, conv->seendb, &conv->cursor);
#else
		int err = conv->seendb->cursor(conv->seendb, conv->txn, &conv->cursor, DB_CURSOR_BULK);
#endif
		if (err) {
			strncpy(tQSL_CustomError, db_strerror(err), sizeof tQSL_CustomError);
			tQSL_Error = TQSL_DB_ERROR;
			tQSL_Errno = errno;
			return 1;
		}
	}

#ifdef USE_LMDB
	MDB_val dbkey, dbdata;
	int status = mdb_cursor_get(conv->cursor, &dbkey, &dbdata, MDB_NEXT);
	if (MDB_NOTFOUND == status) {
#else
	DBT dbkey, dbdata;
	memset(&dbkey, 0, sizeof dbkey);
	memset(&dbdata, 0, sizeof dbdata);
	int status = conv->cursor->c_get(conv->cursor, &dbkey, &dbdata, DB_NEXT);
	if (DB_NOTFOUND == status) {
#endif
		return -1;	// No more records
	}
	if (status != 0) {
		strncpy(tQSL_CustomError, db_strerror(status), sizeof tQSL_CustomError);
		tQSL_Error = TQSL_DB_ERROR;
		tQSL_Errno = errno;
		return 1;
	}
#ifdef USE_LMDB
	memcpy(key, dbkey.mv_data, dbkey.mv_size);
	key[dbkey.mv_size] = '\0';

	if (dbdata.mv_size > 9) dbdata.mv_size = 9;
	memcpy(data, dbdata.mv_data, dbdata.mv_size);
	data[dbdata.mv_size] = '\0';
#else
	memcpy(key, dbkey.data, dbkey.size);
	key[dbkey.size] = '\0';

	if (dbdata.size > 9) dbdata.size = 9;
	memcpy(data, dbdata.data, dbdata.size);
	data[dbdata.size] = '\0';
#endif
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDuplicateRecordsV2(tQSL_Converter convp, char *key, char *data, int keylen) {
	TQSL_CONVERTER *conv;

	if (!(conv = check_conv(convp)))
		return 1;

	if (!conv->db_open) {
		if (!open_db(conv, true)) {	// If can't open dupes DB
			return 1;
		}
	}
	if (!conv->cursor) {
#ifdef USE_LMDB
		int err = mdb_cursor_open(conv->txn, conv->seendb, &conv->cursor);
#else
		int err = conv->seendb->cursor(conv->seendb, conv->txn, &conv->cursor, DB_CURSOR_BULK);
#endif
		if (err) {
			strncpy(tQSL_CustomError, db_strerror(err), sizeof tQSL_CustomError);
			tQSL_Error = TQSL_DB_ERROR;
			tQSL_Errno = errno;
			return 1;
		}
	}

#ifdef USE_LMDB
	MDB_val dbkey, dbdata;
	int status = mdb_cursor_get(conv->cursor, &dbkey, &dbdata, MDB_NEXT);
	if (MDB_NOTFOUND == status) {
#else
	DBT dbkey, dbdata;
	memset(&dbkey, 0, sizeof dbkey);
	memset(&dbdata, 0, sizeof dbdata);
	int status = conv->cursor->c_get(conv->cursor, &dbkey, &dbdata, DB_NEXT);
	if (DB_NOTFOUND == status) {
#endif
		return -1;	// No more records
	}
	if (status != 0) {
		strncpy(tQSL_CustomError, db_strerror(status), sizeof tQSL_CustomError);
		tQSL_Error = TQSL_DB_ERROR;
		tQSL_Errno = errno;
		return 1;
	}
#ifdef USE_LMDB
	memcpy(key, dbkey.mv_data, dbkey.mv_size);
	key[dbkey.mv_size] = '\0';
	if (dbdata.mv_size > 255) dbdata.mv_size = 255;
	memcpy(data, dbdata.mv_data, dbdata.mv_size);
	data[dbdata.mv_size] = '\0';
#else
	memcpy(key, dbkey.data, dbkey.size);
	key[dbkey.size] = '\0';
	if (dbdata.size > 255) dbdata.size = 255;
	memcpy(data, dbdata.data, dbdata.size);
	data[dbdata.size] = '\0';
#endif
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_putDuplicateRecord(tQSL_Converter convp, const char *key, const char *data, int keylen) {
	TQSL_CONVERTER *conv;

	if (!(conv = check_conv(convp)))
		return 0;

	if (!conv->db_open) {
		if (!open_db(conv, false)) {	// If can't open dupes DB
			return 0;
		}
	}
#ifdef USE_LMDB
	MDB_val dbkey, dbdata;
	dbkey.mv_size = keylen;
	dbkey.mv_data = const_cast<char *>(key);

	dbdata.mv_size = strlen(data);
	dbdata.mv_data = const_cast<char *>(data);

	int status = mdb_put(conv->txn, conv->seendb, &dbkey, &dbdata, 0);

	if (MDB_KEYEXIST == status) {
		return -1;	// OK, but already there
	}

#else
	DBT dbkey, dbdata;
	memset(&dbkey, 0, sizeof dbkey);
	memset(&dbdata, 0, sizeof dbdata);
	dbkey.size = keylen;
	dbkey.data = const_cast<char *>(key);

	dbdata.size = strlen(data);
	dbdata.data = const_cast<char *>(data);

	int status = conv->seendb->put(conv->seendb, conv->txn, &dbkey, &dbdata, 0);

	if (DB_KEYEXIST == status) {
		return -1;	// OK, but already there
	}
#endif

	if (status != 0) {
		strncpy(tQSL_CustomError, db_strerror(status), sizeof tQSL_CustomError);
		tQSL_Error = TQSL_DB_ERROR;
		tQSL_Errno = errno;
		return 1;
	}
	return 0;
}

static bool
hasValidCallSignChars(const string& call) {
	// Check for invalid characters
	if (call.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/") != string::npos)
		return false;
	// Need at least one letter
	if (call.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == string::npos)
		return false;
	// Need at least one number
	if (call.find_first_of("0123456789") == string::npos)
		return false;
	// Invalid callsign patterns
	// Starting with 0, Q
	// 1x other than 1A, 1M, 1S
	string first = call.substr(0, 1);
	string second = call.substr(1, 1);
	if (first == "0" || first == "Q" ||
#ifdef MARK_C7_4Y_INVALID
	    (first == "C" && second == "7") ||
	    (first == "4" && second == "Y") ||
#endif
	    (first == "1" && second != "A" && second != "M" && second != "S"))
		return false;

	return true;
}

static bool
checkCallSign(const string& call) {
	if (!hasValidCallSignChars(call))
		return false;
	if (call.length() < 3)
		return false;
	string::size_type idx, newidx;
	for (idx = 0; idx != string::npos; idx = newidx+1) {
		string s;
		newidx = call.find('/', idx);
		if (newidx == string::npos)
			s = call.substr(idx);
		else
			s = call.substr(idx, newidx - idx);
		if (s.length() == 0)
			return false;	// Leading or trailing '/' is bad, bad!
		if (newidx == string::npos)
			break;
	}
	return true;
}
