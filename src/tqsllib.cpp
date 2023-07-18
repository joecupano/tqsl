/***************************************************************************
                          tqsllib.c  -  description
                             -------------------
    begin                : Mon May 20 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#define TQSLLIB_DEF

#include "tqsllib.h"
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
    #include <io.h>
    #include <windows.h>
    #include <direct.h>
    #include <Shlobj.h>
#else
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <CoreFoundation/CFBundle.h>
#endif
#include <string>
using std::string;

#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/evp.h>
#if OPENSSL_VERSION_MAJOR >= 3
#include <openssl/provider.h>
#endif

#include "tqslerrno.h"
#include "adif.h"
#include "winstrdefs.h"

#ifdef _WIN32
#define MKDIR(x, y) _wmkdir((x))
#else
#define MKDIR(x, y) mkdir((x), (y))
#endif

DLLEXPORTDATA int tQSL_Error = 0;
DLLEXPORTDATA int tQSL_Errno = 0;
DLLEXPORTDATA TQSL_ADIF_GET_FIELD_ERROR tQSL_ADIF_Error;
DLLEXPORTDATA const char *tQSL_BaseDir = 0;
DLLEXPORTDATA const char *tQSL_RsrcDir = 0;
DLLEXPORTDATA char tQSL_ErrorFile[TQSL_MAX_PATH_LEN];
DLLEXPORTDATA char tQSL_CustomError[256];
DLLEXPORTDATA char tQSL_ImportCall[256];
DLLEXPORTDATA long tQSL_ImportSerial = 0;
DLLEXPORTDATA FILE* tQSL_DiagFile = 0;

#define TQSL_OID_BASE "1.3.6.1.4.1.12348.1."
#define TQSL_OID_CALLSIGN TQSL_OID_BASE "1"
#define TQSL_OID_QSO_NOT_BEFORE TQSL_OID_BASE "2"
#define TQSL_OID_QSO_NOT_AFTER TQSL_OID_BASE "3"
#define TQSL_OID_DXCC_ENTITY TQSL_OID_BASE "4"
#define TQSL_OID_SUPERCEDED_CERT TQSL_OID_BASE "5"
#define TQSL_OID_CRQ_ISSUER_ORGANIZATION TQSL_OID_BASE "6"
#define TQSL_OID_CRQ_ISSUER_ORGANIZATIONAL_UNIT TQSL_OID_BASE "7"
#define TQSL_OID_CRQ_EMAIL TQSL_OID_BASE "8"
#define TQSL_OID_CRQ_ADDRESS1 TQSL_OID_BASE "9"
#define TQSL_OID_CRQ_ADDRESS2 TQSL_OID_BASE "10"
#define TQSL_OID_CRQ_CITY TQSL_OID_BASE "11"
#define TQSL_OID_CRQ_STATE TQSL_OID_BASE "12"
#define TQSL_OID_CRQ_POSTAL TQSL_OID_BASE "13"
#define TQSL_OID_CRQ_COUNTRY TQSL_OID_BASE "14"

static const char *custom_objects[][3] = {
	{ TQSL_OID_CALLSIGN, "AROcallsign", "AROcallsign" },
	{ TQSL_OID_QSO_NOT_BEFORE, "QSONotBeforeDate", "QSONotBeforeDate" },
	{ TQSL_OID_QSO_NOT_AFTER, "QSONotAfterDate", "QSONotAfterDate" },
	{ TQSL_OID_DXCC_ENTITY, "dxccEntity", "dxccEntity" },
	{ TQSL_OID_SUPERCEDED_CERT, "supercededCertificate", "supercededCertificate" },
	{ TQSL_OID_CRQ_ISSUER_ORGANIZATION, "tqslCRQIssuerOrganization", "tqslCRQIssuerOrganization" },
	{ TQSL_OID_CRQ_ISSUER_ORGANIZATIONAL_UNIT,
			"tqslCRQIssuerOrganizationalUnit", "tqslCRQIssuerOrganizationalUnit" },
	{ TQSL_OID_CRQ_EMAIL, "tqslCRQEmail", "tqslCRQEmail" },
	{ TQSL_OID_CRQ_ADDRESS1, "tqslCRQAddress1", "tqslCRQAddress1" },
	{ TQSL_OID_CRQ_ADDRESS2, "tqslCRQAddress2", "tqslCRQAddress2" },
	{ TQSL_OID_CRQ_CITY, "tqslCRQCity", "tqslCRQCity" },
	{ TQSL_OID_CRQ_STATE, "tqslCRQState", "tqslCRQState" },
	{ TQSL_OID_CRQ_POSTAL, "tqslCRQPostal", "tqslCRQPostal" },
	{ TQSL_OID_CRQ_COUNTRY, "tqslCRQCountry", "tqslCRQCountry" },
};

static const char *error_strings[] = {
	"Memory allocation failure",				/* TQSL_ALLOC_ERROR */
	"Unable to initialize random number generator",		/* TQSL_RANDOM_ERROR */
	"Invalid argument",					/* TQSL_ARGUMENT_ERROR */
	"Operator aborted operation",				/* TQSL_OPERATOR_ABORT */
	"No Certificate Request matches the selected Callsign Certificate",/* TQSL_NOKEY_ERROR */
	"Buffer too small",					/* TQSL_BUFFER_ERROR */
	"Invalid date format",					/* TQSL_INVALID_DATE */
	"Certificate not initialized for signing",		/* TQSL_SIGNINIT_ERROR */
	"Passphrase not correct",				/* TQSL_PASSWORD_ERROR */
	"Expected name",					/* TQSL_EXPECTED_NAME */
	"Name exists",						/* TQSL_NAME_EXISTS */
	"Data for this DXCC entity could not be found",		/* TQSL_NAME_NOT_FOUND */
	"Invalid time format",					/* TQSL_INVALID_TIME */
	"QSO date is not within the date range specified on your Callsign Certificate",	/* TQSL_CERT_DATE_MISMATCH */
	"Certificate provider not found",			/* TQSL_PROVIDER_NOT_FOUND */
	"No callsign certificate for key",			/* TQSL_CERT_KEY_ONLY */
	"Configuration file cannot be opened",			/* TQSL_CONFIG_ERROR */
	"The private key for this Callsign Certificate is not present on this computer; you can obtain it by loading a .tbk or .p12 file",
								/* TQSL_CERT_NOT_FOUND */
	"PKCS#12 file not TQSL compatible",			/* TQSL_PKCS12_ERROR */
	"Callsign Certificate not TQSL compatible",		/* TQSL_CERT_TYPE_ERROR */
	"Date out of range",					/* TQSL_DATE_OUT_OF_RANGE */
	"Already Uploaded QSO suppressed",			/* TQSL_DUPLICATE_QSO */
	"Database error",					/* TQSL_DB_ERROR */
	"The selected station location could not be found",	/* TQSL_LOCATION_NOT_FOUND */
	"The selected callsign could not be found",		/* TQSL_CALL_NOT_FOUND */
	"The TQSL configuration file cannot be parsed",		/* TQSL_CONFIG_SYNTAX_ERROR */
	"This file can not be processed due to a system error",	/* TQSL_FILE_SYSTEM_ERROR */
	"The format of this file is incorrect.",		/* TQSL_FILE_SYNTAX_ERROR */
	"This Callsign Certificate could not be installed", 	/* TQSL_CERT_ERROR */
	"Callsign Certificate does not match QSO details", 	/* TQSL_CERT_MISMATCH */
	"Station Location does not match QSO details", 		/* TQSL_LOCATION_MISMATCH */
	/* note - dupe table in wxutil.cpp */
};


#if !defined(__APPLE__) && !defined(_WIN32) && !defined(__clang__)
        #pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

const char* tqsl_openssl_error(void);

#if defined(_WIN32)
static int pmkdir(const wchar_t *path, int perm) {
	wchar_t dpath[TQSL_MAX_PATH_LEN];
	wchar_t npath[TQSL_MAX_PATH_LEN];
	wchar_t *cp;
	char *p = wchar_to_utf8(path, true);
	tqslTrace("pmkdir", "path=%s", p);
	free(p);
	int nleft = (sizeof npath / 2) - 1;
	wcsncpy(dpath, path, (sizeof dpath / 2));
	cp = wcstok(dpath, L"/\\");
	npath[0] = 0;
	while (cp) {
		if (wcslen(cp) > 0 && cp[wcslen(cp)-1] != ':') {
			wcsncat(npath, L"\\", nleft);
			nleft--;
			wcsncat(npath, cp, nleft);
			nleft -= wcslen(cp);
			if (MKDIR(npath, perm) != 0 && errno != EEXIST) {
				tqslTrace("pmkdir", "Error creating %s: %s", npath, strerror(errno));
				return 1;
			}
		} else {
			wcsncat(npath, cp, nleft);
			nleft -= wcslen(cp);
		}
		cp = wcstok(NULL, L"/\\");
	}
	return 0;
}

#else // defined(_WIN32)
static int pmkdir(const char *path, int perm) {
	char dpath[TQSL_MAX_PATH_LEN];
	char npath[TQSL_MAX_PATH_LEN];
	char *cp;
	tqslTrace("pmkdir", "path=%s", path);
	int nleft = sizeof npath - 1;
	strncpy(dpath, path, sizeof dpath);
	cp = strtok(dpath, "/\\");
	npath[0] = 0;
	while (cp) {
		if (strlen(cp) > 0 && cp[strlen(cp)-1] != ':') {
			strncat(npath, "/", nleft);
			nleft--;
			strncat(npath, cp, nleft);
			nleft -= strlen(cp);
			if (MKDIR(npath, perm) != 0 && errno != EEXIST) {
				tqslTrace("pmkdir", "Error creating %s: %s", npath, strerror(errno));
				return 1;
			}
		} else {
			strncat(npath, cp, nleft);
			nleft -= strlen(cp);
		}
		cp = strtok(NULL, "/\\");
	}
	return 0;
}
#endif // defined(_WIN32)

static void
tqsl_get_rsrc_dir() {
	tqslTrace("tqsl_get_rsrc_dir", NULL);

#ifdef _WIN32
	HKEY hkey;
	DWORD dtype;
	char wpath[TQSL_MAX_PATH_LEN];
	DWORD bsize = sizeof wpath;
	int wval;
	if ((wval = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		"Software\\TrustedQSL", 0, KEY_READ, &hkey)) == ERROR_SUCCESS) {
		wval = RegQueryValueEx(hkey, "InstallPath", 0, &dtype, (LPBYTE)wpath, &bsize);
		RegCloseKey(hkey);
		if (wval == ERROR_SUCCESS) {
			string p = string(wpath);
			if (p[p.length() -1] == '\\')
				p = p.substr(0, p.length() - 1);
			tQSL_RsrcDir = strdup(p.c_str());
		}
	}
#elif defined(__APPLE__)
	// Get path to config.xml resource from bundle
	CFBundleRef tqslBundle = CFBundleGetMainBundle();
	CFURLRef configXMLURL = CFBundleCopyResourceURL(tqslBundle, CFSTR("config"), CFSTR("xml"), NULL);
	if (configXMLURL) {
		CFStringRef pathString = CFURLCopyFileSystemPath(configXMLURL, kCFURLPOSIXPathStyle);
		CFRelease(configXMLURL);

		// Convert CFString path to config.xml to string object
		CFIndex maxStringLengthInBytes = CFStringGetMaximumSizeForEncoding(CFStringGetLength(pathString), kCFStringEncodingUTF8);
		char *pathCString = static_cast<char *>(malloc(maxStringLengthInBytes));
		if (pathCString) {
			CFStringGetCString(pathString, pathCString, maxStringLengthInBytes, kCFStringEncodingASCII);
			CFRelease(pathString);
			size_t p;
			string path = string(pathCString);
			if ((p = path.find("/config.xml")) != string::npos) {
				path = path.substr(0, p);
			}
			tQSL_RsrcDir = strdup(path.c_str());
			free(pathCString);
		}
	}
#else
	// Get the base directory
	string p = CONFDIR;
	// Strip trailing "/"
	if (p[p.length() - 1] == '/')
		p = p.substr(0, p.length() - 1);

	// Check if running as an AppImage
	char *appdir = getenv("APPDIR");
	if (appdir == NULL) {
		tQSL_RsrcDir = strdup(p.c_str());
	} else {
		string p1 = appdir;
		// Strip trailing "/"
		if (p1[p1.length() - 1] == '/')
			p1 = p1.substr(0, p1.length() - 1);
		p1 = p1 + p;

		// Assume APPDIR is probably not an AppImage root
		tQSL_RsrcDir = strdup(p.c_str());
		// See if it's likely to be an AppImage
		struct stat s;
                if (stat(p1.c_str(), &s) == 0) {
                        if (S_ISDIR(s.st_mode)) {
				tQSL_RsrcDir = strdup(p1.c_str());
			}
		}
	}
#endif
	tqslTrace("tqsl_get_rsrc_dir", "rsrc_path=%s", tQSL_RsrcDir);
}

DLLEXPORT int CALLCONVENTION
tqsl_init() {
	static char semaphore = 0;
	unsigned int i;

	ERR_clear_error();
	tqsl_getErrorString();	/* Clear the error status */
	if (semaphore)
		return 0;
#ifdef _WIN32
	static wchar_t path[TQSL_MAX_PATH_LEN * 2];
	// lets cin/out/err work in windows
	// AllocConsole();
	// freopen("CONIN$", "r", stdin);
	// freopen("CONOUT$", "w", stdout);
	// freopen("CONOUT$", "w", stderr);
	// not used DWORD bsize = sizeof path;
	int wval;
#else
	static char path[TQSL_MAX_PATH_LEN];
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
// Work around ill-considered decision by Fedora to stop allowing
// certificates with MD5 signatures
	setenv("OPENSSL_ENABLE_MD5_VERIFY", "1", 0);
#endif

	/* OpenSSL API tends to change between minor version numbers, so make sure
	 * we're using the right version */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	unsigned long SSLver = OpenSSL_version_num();
#else
	long SSLver = SSLeay();
#endif
	int SSLmajor = (SSLver >> 28) & 0xff;
	int SSLminor = (SSLver >> 20) & 0xff;
	int TQSLmajor = (OPENSSL_VERSION_NUMBER >> 28) & 0xff;
	int TQSLminor =  (OPENSSL_VERSION_NUMBER >> 20) & 0xff;

	if (SSLmajor != TQSLmajor ||
		(SSLminor != TQSLminor &&
		(SSLmajor != 9 && SSLminor != 7 && TQSLminor == 6))) {
		tqslTrace("tqsl_init", "version error - ssl %d.%d", SSLmajor, SSLminor);
		tQSL_Error = TQSL_OPENSSL_VERSION_ERROR;
		return 1;
	}
#if OPENSSL_VERSION_MAJOR >= 3
//
//	OpenSSL 3.x moved several algorithms to "legacy" status and doesn't
//	enable them by default.  Initialize the legacy provider to enable these.
//	Then enable the default provider.
//
	OSSL_PROVIDER *def;
	OSSL_PROVIDER *legacy;

	legacy = OSSL_PROVIDER_load(NULL, "legacy");
	if (legacy == NULL) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	def = OSSL_PROVIDER_load(NULL, "default");
	if (def == NULL) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
#endif
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
#endif
	for (i = 0; i < (sizeof custom_objects / sizeof custom_objects[0]); i++) {
		if (OBJ_create(custom_objects[i][0], custom_objects[i][1], custom_objects[i][2]) == 0) {
			tqslTrace("tqsl_init", "Error making custom objects: %s", tqsl_openssl_error());
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 1;
		}
	}
	if (tQSL_RsrcDir == NULL) {
		tqsl_get_rsrc_dir();
	}
	if (tQSL_BaseDir == NULL) {
#if defined(_WIN32)
		wchar_t *wcp;
		if ((wcp = _wgetenv(L"TQSLDIR")) != NULL && *wcp != '\0') {
			wcsncpy(path, wcp, sizeof path);
#else
		char *cp;
		if ((cp = getenv("TQSLDIR")) != NULL && *cp != '\0') {
			strncpy(path, cp, sizeof path);
#endif
		} else {
#if defined(_WIN32)
			wval = SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path);
			if (wval != ERROR_SUCCESS)
				wcsncpy(path, L"C:", sizeof path);
			wcsncat(path, L"\\TrustedQSL", sizeof path - wcslen(path) - 1);
#elif defined(LOTW_SERVER)
			strncpy(path, "/var/lotw/tqsl", sizeof path);
#else  // some unix flavor
			if (getenv("HOME") != NULL) {
				strncpy(path, getenv("HOME"), sizeof path);
				strncat(path, "/", sizeof path - strlen(path)-1);
				strncat(path, ".tqsl", sizeof path - strlen(path)-1);
			} else {
				strncpy(path, ".tqsl", sizeof path);
			}
#endif
		}
		if (pmkdir(path, 0700)) {
#if defined(_WIN32)
			char *p = wchar_to_utf8(path, false);
			strncpy(tQSL_ErrorFile, p, sizeof tQSL_ErrorFile);
#else
			strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
#endif
			tQSL_Error = TQSL_SYSTEM_ERROR;
			tQSL_Errno = errno;
#if defined(_WIN32)
			tqslTrace("tqsl_init", "Error creating working path %s: %s", p, strerror(errno));
			free(p);
#else
			tqslTrace("tqsl_init", "Error creating working path %s: %s", path, strerror(errno));
#endif
			return 1;
		}
		FILE *test;
#if defined(_WIN32)
		tQSL_BaseDir = wchar_to_utf8(path, true);
		wcsncat(path, L"\\tmp.tmp", sizeof path - wcslen(path) - 1);
		if ((test = _wfopen(path, L"wb")) == NULL) {
			tQSL_Errno = errno;
			char *p = wchar_to_utf8(path, false);
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Unable to create files in the TQSL working directory (%s): %m", p);
			tQSL_Error = TQSL_CUSTOM_ERROR;
			return 1;
		}
		fclose(test);
		_wunlink(path);
#else
		if (tQSL_BaseDir) free (const_cast<char *>(tQSL_BaseDir));
		tQSL_BaseDir = strdup(path);
		strncat(path, "/tmp.tmp", sizeof path -strlen(path) - 1);
		if ((test = fopen(path, "wb")) == NULL) {
			tQSL_Errno = errno;
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Unable to create files in the TQSL working directory (%s): %m", tQSL_BaseDir);
			tQSL_Error = TQSL_CUSTOM_ERROR;
			return 1;
		}
		fclose(test);
		unlink(path);
#endif
	}
	semaphore = 1;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setDirectory(const char *dir) {
	static char path[TQSL_MAX_PATH_LEN];
	if (strlen(dir) >= TQSL_MAX_PATH_LEN) {
		tQSL_Error = TQSL_BUFFER_ERROR;
		return 1;
	}
	strncpy(path, dir, sizeof path);
	tQSL_BaseDir = path;
	return 0;
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getErrorString_v(int err) {
	static char buf[512];
	unsigned long openssl_err;
	int adjusted_err;

	if (err == 0)
		return "NO ERROR";
	if (err == TQSL_CUSTOM_ERROR) {
		if (tQSL_CustomError[0] == 0) {
			return "Unknown custom error";
		} else {
			strncpy(buf, tQSL_CustomError, sizeof buf);
			return buf;
		}
	}
	if (err == TQSL_DB_ERROR && tQSL_CustomError[0] != 0) {
		snprintf(buf, sizeof buf, "Database Error: %s", tQSL_CustomError);
		return buf;
	}

	if (err == TQSL_SYSTEM_ERROR || err == TQSL_FILE_SYSTEM_ERROR) {
		if (strlen(tQSL_ErrorFile) > 0) {
			snprintf(buf, sizeof buf, "System error: %s : %s",
				tQSL_ErrorFile, strerror(tQSL_Errno));
			tQSL_ErrorFile[0] = '\0';
		} else {
			snprintf(buf, sizeof buf, "System error: %s",
				strerror(tQSL_Errno));
		}
		return buf;
	}
	if (err == TQSL_FILE_SYNTAX_ERROR) {
		tqslTrace("SyntaxError", "File (partial) content '%s'", tQSL_CustomError);
		if (strlen(tQSL_ErrorFile) > 0) {
			snprintf(buf, sizeof buf, "File syntax error: %s",
				tQSL_ErrorFile);
			tQSL_ErrorFile[0] = '\0';
		} else {
			strncpy(buf, "File syntax error", sizeof buf);
		}
		return buf;
	}
	if (err == TQSL_OPENSSL_ERROR) {
		openssl_err = ERR_get_error();
		strncpy(buf, "OpenSSL error: ", sizeof buf);
		if (openssl_err)
			ERR_error_string_n(openssl_err, buf + strlen(buf), sizeof buf - strlen(buf)-1);
		else
			strncat(buf, "[error code not available]", sizeof buf - strlen(buf)-1);
		return buf;
	}
	if (err == TQSL_ADIF_ERROR) {
		buf[0] = 0;
		if (strlen(tQSL_ErrorFile) > 0) {
			snprintf(buf, sizeof buf, "%s: %s",
				tQSL_ErrorFile, tqsl_adifGetError(tQSL_ADIF_Error));
			tQSL_ErrorFile[0] = '\0';
		} else {
			snprintf(buf, sizeof buf, "%s",
				tqsl_adifGetError(tQSL_ADIF_Error));
		}
		return buf;
	}
	if (err == TQSL_CABRILLO_ERROR) {
		buf[0] = 0;
		if (strlen(tQSL_ErrorFile) > 0) {
			snprintf(buf, sizeof buf, "%s: %s",
				tQSL_ErrorFile, tqsl_cabrilloGetError(tQSL_Cabrillo_Error));
			tQSL_ErrorFile[0] = '\0';
		} else {
			snprintf(buf, sizeof buf, "%s",
				tqsl_cabrilloGetError(tQSL_Cabrillo_Error));
		}
		return buf;
	}
	if (err == TQSL_OPENSSL_VERSION_ERROR) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
		unsigned long SSLver = OpenSSL_version_num();
#else
		long SSLver = SSLeay();
#endif
		snprintf(buf, sizeof buf,
			"Incompatible OpenSSL Library version %d.%d.%d; expected %d.%d.%d",
			static_cast<int>(SSLver >> 28) & 0xff,
			static_cast<int>(SSLver >> 20) & 0xff,
			static_cast<int>(SSLver >> 12) & 0xff,
			static_cast<int>(OPENSSL_VERSION_NUMBER >> 28) & 0xff,
			static_cast<int>(OPENSSL_VERSION_NUMBER >> 20) & 0xff,
			static_cast<int>(OPENSSL_VERSION_NUMBER >> 12) & 0xff);
		return buf;
	}
	if (err == TQSL_CERT_NOT_FOUND && tQSL_ImportCall[0] != '\0') {
		snprintf(buf, sizeof buf,
			"The private key for callsign %s serial %ld is not present on this computer; you can obtain it by loading a .tbk or .p12 file",
			tQSL_ImportCall, tQSL_ImportSerial);
		tQSL_ImportCall[0] = '\0';
		return buf;
	}
	adjusted_err = (err - TQSL_ERROR_ENUM_BASE) & ~0x1000;
	if (adjusted_err < 0 ||
	    adjusted_err >=
		static_cast<int>(sizeof error_strings / sizeof error_strings[0])) {
		snprintf(buf, sizeof buf, "Invalid error code: %d", err);
		return buf;
	}
	if (err == TQSL_CERT_MISMATCH || err == TQSL_LOCATION_MISMATCH) {
		const char *fld, *cert, *qso;
		fld = strtok(tQSL_CustomError, "|");
		cert = strtok(NULL, "|");
		qso = strtok(NULL, "|");
		if (qso == NULL) {		// Nothing in the cert
			qso = cert;
			cert = "none";
		}
		snprintf(buf, sizeof buf, "%s\nThe %s '%s' has value '%s' while QSO has '%s'",
			error_strings[adjusted_err],
			err == TQSL_LOCATION_MISMATCH ? "Station Location" : "Callsign Certificate",
			fld, cert, qso);
		return buf;
	}
	if (err == (TQSL_LOCATION_MISMATCH | 0x1000)) {
		const char *fld, *val;
		fld = strtok(tQSL_CustomError, "|");
		val = strtok(NULL, "|");
		snprintf(buf, sizeof buf, "This log has invalid QSO information.\nThe log being signed has '%s' set to value '%s' which is not valid", fld, val);
		return buf;
	}
	if (err == (TQSL_CERT_NOT_FOUND | 0x1000)) {
		const char *call, *ent;
		err = TQSL_CERT_NOT_FOUND;
		call = strtok(tQSL_CustomError, "|");
		ent = strtok(NULL, "|");
		snprintf(buf, sizeof buf, "There is no valid callsign certificate for %s in entity %s available. This QSO cannot be signed", call, ent);
		return buf;
	}
	return error_strings[adjusted_err];
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getErrorString() {
	const char *cp;
	cp = tqsl_getErrorString_v(tQSL_Error);
	tQSL_Error = TQSL_NO_ERROR;
	tQSL_Errno = 0;
	tQSL_ErrorFile[0] = 0;
	tQSL_CustomError[0] = 0;
	return cp;
}

DLLEXPORT int CALLCONVENTION
tqsl_encodeBase64(const unsigned char *data, int datalen, char *output, int outputlen) {
	BIO *bio = NULL, *bio64 = NULL;
	int n;
	char *memp;
	int rval = 1;

	if (data == NULL || output == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		tqslTrace("tqsl_encodeBase64", "arg err data=0x%lx, output=0x%lx", data, output);
		return rval;
	}
	if ((bio = BIO_new(BIO_s_mem())) == NULL) {
		tqslTrace("tqsl_encodeBase64", "BIO_new err %s", tqsl_openssl_error());
		goto err;
	}
	if ((bio64 = BIO_new(BIO_f_base64())) == NULL) {
		tqslTrace("tqsl_encodeBase64", "BIO_new64 err %s", tqsl_openssl_error());
		goto err;
	}
	bio = BIO_push(bio64, bio);
	if (BIO_write(bio, data, datalen) < 1) {
		tqslTrace("tqsl_encodeBase64", "BIO_write err %s", tqsl_openssl_error());
		goto err;
	}
	if (BIO_flush(bio) != 1) {
		tqslTrace("tqsl_encodeBase64", "BIO_flush err %s", tqsl_openssl_error());
		goto err;
	}
	n = BIO_get_mem_data(bio, &memp);
	if (n > outputlen-1) {
		tqslTrace("tqsl_encodeBase64", "buffer has %d, avail %d", n, outputlen);
		tQSL_Error = TQSL_BUFFER_ERROR;
		goto end;
	}
	memcpy(output, memp, n);
	output[n] = 0;
	BIO_free_all(bio);
	bio = NULL;
	rval = 0;
	goto end;

 err:
	tQSL_Error = TQSL_OPENSSL_ERROR;
 end:
	if (bio != NULL)
		BIO_free_all(bio);
	return rval;
}

DLLEXPORT int CALLCONVENTION
tqsl_decodeBase64(const char *input, unsigned char *data, int *datalen) {
	BIO *bio = NULL, *bio64 = NULL;
	int n;
	int rval = 1;

	if (input == NULL || data == NULL || datalen == NULL) {
		tqslTrace("tqsl_decodeBase64", "arg error input=0x%lx, data=0x%lx, datalen=0x%lx", input, data, datalen);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return rval;
	}
	if ((bio = BIO_new_mem_buf(const_cast<char *>(input), strlen(input))) == NULL) {
		tqslTrace("tqsl_decodeBase64", "BIO_new_mem_buf err %s", tqsl_openssl_error());
		goto err;
	}
	BIO_set_mem_eof_return(bio, 0);
	if ((bio64 = BIO_new(BIO_f_base64())) == NULL) {
		tqslTrace("tqsl_decodeBase64", "BIO_new err %s", tqsl_openssl_error());
		goto err;
	}
	bio = BIO_push(bio64, bio);
	n = BIO_read(bio, data, *datalen);
	if (n < 0) {
		tqslTrace("tqsl_decodeBase64", "BIO_read error %s", tqsl_openssl_error());
		goto err;
	}
	if (BIO_ctrl_pending(bio) != 0) {
		tqslTrace("tqsl_decodeBase64", "ctrl_pending err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_BUFFER_ERROR;
		goto end;
	}
	*datalen = n;
	rval = 0;
	goto end;

 err:
	tQSL_Error = TQSL_OPENSSL_ERROR;
 end:
	if (bio != NULL)
		BIO_free_all(bio);
	return rval;
}

/* Convert a tQSL_Date field to an ISO-format date string
 */
DLLEXPORT char* CALLCONVENTION
tqsl_convertDateToText(const tQSL_Date *date, char *buf, int bufsiz) {
	char lbuf[10];
	int len;
	char *cp = buf;
	int bufleft = bufsiz-1;

	if (date == NULL || buf == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		if (buf) buf[0] = '\0';
		return NULL;
	}
	if (date->year < 1 || date->year > 9999 || date->month < 1
		|| date->month > 12 || date->day < 1 || date->day > 31) {
		buf[0] = '\0';
		return NULL;
	}
	len = snprintf(lbuf, sizeof lbuf, "%04d-", date->year);
	strncpy(cp, lbuf, bufleft);
	cp += len;
	bufleft -= len;
	len = snprintf(lbuf, sizeof lbuf, "%02d-", date->month);
	if (bufleft > 0)
		strncpy(cp, lbuf, bufleft);
	cp += len;
	bufleft -= len;
	len = snprintf(lbuf, sizeof lbuf, "%02d", date->day);
	if (bufleft > 0)
		strncpy(cp, lbuf, bufleft);
	bufleft -= len;
	if (bufleft < 0)
		return NULL;
	buf[bufsiz-1] = '\0';
	return buf;
}

DLLEXPORT int CALLCONVENTION
tqsl_isDateValid(const tQSL_Date *d) {
	static int mon_days[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if (d == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 0;
	}
	if (d->year < 1 || d->year > 9999)
		return 0;
	if (d->month < 1 || d->month > 12)
		return 0;
	if (d->day < 1 || d->day > 31)
		return 0;
	mon_days[2] = ((d->year % 4) == 0 &&
		      ((d->year % 100) != 0 || (d->year % 400) == 0))
		? 29 : 28;
	if (d->day > mon_days[d->month])
		return 0;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_isDateNull(const tQSL_Date *d) {
	if (d == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	return (d->year == 0 && d->month == 0 && d->day == 0) ? 1 : 0;
}

/* Convert a tQSL_Time field to an ISO-format date string
 */
DLLEXPORT char* CALLCONVENTION
tqsl_convertTimeToText(const tQSL_Time *time, char *buf, int bufsiz) {
	char lbuf[10];
	int len;
	char *cp = buf;
	int bufleft = bufsiz-1;

	if (time == NULL || buf == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return NULL;
	}
	if (!tqsl_isTimeValid(time))
		return NULL;
	len = snprintf(lbuf, sizeof lbuf, "%02d:", time->hour);
	strncpy(cp, lbuf, bufleft);
	cp += len;
	bufleft -= len;
	len = snprintf(lbuf, sizeof lbuf, "%02d:", time->minute);
	if (bufleft > 0)
		strncpy(cp, lbuf, bufleft);
	cp += len;
	bufleft -= len;
	len = snprintf(lbuf, sizeof lbuf, "%02d", time->second);
	if (bufleft > 0)
		strncpy(cp, lbuf, bufleft);
	cp += len;
	bufleft -= len;
	if (bufleft > 0)
		strncpy(cp, "Z", bufleft);
	bufleft -= 1;
	if (bufleft < 0)
		return NULL;
	buf[bufsiz-1] = '\0';
	return buf;
}

DLLEXPORT int CALLCONVENTION
tqsl_isTimeValid(const tQSL_Time *t) {
	if (t == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 0;
	}
	if (t->hour < 0 || t->hour > 23)
		return 0;
	if (t->minute < 0 || t->minute > 59)
		return 0;
	if (t->second < 0 || t->second > 59)
		return 0;
	return 1;
}

/* Compare two tQSL_Date values, returning -1, 0, 1
 */
DLLEXPORT int CALLCONVENTION
tqsl_compareDates(const tQSL_Date *a, const tQSL_Date *b) {
	if (a == NULL || b == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (a->year < b->year)
		return -1;
	if (a->year > b->year)
		return 1;
	if (a->month < b->month)
		return -1;
	if (a->month > b->month)
		return 1;
	if (a->day < b->day)
		return -1;
	if (a->day > b->day)
		return 1;
	return 0;
}

// Return the number of days for a given year/month (January=1)
static int
days_per_month(int year, int month) {
	switch (month) {
		case 2:
			if ((((year % 4) == 0) && ((year % 100) != 0)) || ((year % 400) == 0)) {
				return 29;
			} else {
				return 28;
			}
		case 4:
		case 6:
		case 9:
		case 11:
			return 30;
		default:
			return 31;
	}
	return 0;
}

// Return the julian day number for a given date.
// One-based year/month/day
static int
julian_day(int year, int month, int day) {
	int jday = 0;
	for (int mon = 1; mon < month; mon ++) {
		jday += days_per_month(year, mon);
	}
	jday += day;
	return jday;
}

/* Calculate the difference between two tQSL_Date values
 */
DLLEXPORT int CALLCONVENTION
tqsl_subtractDates(const tQSL_Date *a, const tQSL_Date *b, int *diff) {
	if (a == NULL || b == NULL || diff == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	tQSL_Date first = *a;
	tQSL_Date last = *b;
	int mult = 1;
	// Ensure that the first is earliest
	if (tqsl_compareDates(&last, &first) < 0) {
		first = *b;
		last = *a;
		mult = -1;
	}
	int delta = 0;
	for (; first.year < last.year; first.year++) {
		int fday = julian_day(first.year, first.month, first.day);
		int fend = julian_day(first.year, 12, 31);
		delta += (fend - fday + 1);  // days until next 1 Jan
		first.month = 1;
		first.day = 1;
	}
	// Now the years are the same - calculate delta
	int fjulian = julian_day(first.year, first.month, first.day);
	int ljulian = julian_day(last.year, last.month, last.day);

	delta += (ljulian - fjulian);
	*diff = (delta * mult);			// Swap sign if necessary
	return 0;
}

/* Fill a tQSL_Date struct with the date from a text string
 */
DLLEXPORT int CALLCONVENTION
tqsl_initDate(tQSL_Date *date, const char *str) {
	const char *cp;

	if (date == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (str == NULL) {
		date->year = date->month = date->day = 0;
		return 0;
	}
	if ((cp = strchr(str, '-')) != NULL) {
		/* Parse YYYY-MM-DD */
		date->year = strtol(str, NULL, 10);
		cp++;
		date->month = strtol(cp, NULL, 10);
		cp = strchr(cp, '-');
		if (cp == NULL)
			goto err;
		cp++;
		date->day = strtol(cp, NULL, 10);
	} else if (strlen(str) == 8) {
		/* Parse YYYYMMDD */
		char frag[10];
		strncpy(frag, str, 4);
		frag[4] = 0;
		date->year = strtol(frag, NULL, 10);
		strncpy(frag, str+4, 2);
		frag[2] = 0;
		date->month = strtol(frag, NULL, 10);
		date->day = strtol(str+6, NULL, 10);
	} else {	/* Invalid ISO date string */
		goto err;
	}
	if (date->year < 1 || date->year > 9999)
		goto err;
	if (date->month < 1 || date->month > 12)
		goto err;
	if (date->day < 1 || date->day > 31)
		goto err;
	return 0;
 err:
	tQSL_Error = TQSL_INVALID_DATE;
		return 1;
}

/* Fill a tQSL_Time struct with the time from a text string
 */
DLLEXPORT int CALLCONVENTION
tqsl_initTime(tQSL_Time *time, const char *str) {
	const char *cp;
	int parts[3];
	int i;

	if (time == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	time->hour = time->minute = time->second = 0;
	if (str == NULL)
		return 0;
	if (strlen(str) < 3) {
		tQSL_Error = TQSL_INVALID_TIME;
		return 1;
	}
	parts[0] = parts[1] = parts[2] = 0;
	for (i = 0, cp = str; i < static_cast<int>(sizeof parts / sizeof parts[0]); i++) {
		if (strlen(cp) < 2)
			break;
		if (!isdigit(*cp) || !isdigit(*(cp+1)))
			goto err;
		if (i == 0 && strlen(str) == 3) {
			// Special case: HMM -- no colons, one-digit hour
			parts[i] = *cp - '0';
			++cp;
		} else {
			parts[i] = (*cp - '0') * 10 + *(cp+1) - '0';
			cp += 2;
		}
		if (*cp == ':')
			cp++;
	}

	if (parts[0] < 0 || parts[0] > 23)
		goto err;
	if (parts[1] < 0 || parts[1] > 59)
		goto err;
	if (parts[2] < 0 || parts[2] > 59)
		goto err;
	time->hour = parts[0];
	time->minute = parts[1];
	time->second = parts[2];
	return 0;
 err:
	tQSL_Error = TQSL_INVALID_TIME;
		return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getVersion(int *major, int *minor) {
	if (major)
		*major = TQSLLIB_VERSION_MAJOR;
	if (minor)
		*minor = TQSLLIB_VERSION_MINOR;
	return 0;
}
#ifdef _WIN32
DLLEXPORT wchar_t* CALLCONVENTION
utf8_to_wchar(const char* str) {
	wchar_t* buffer;
	int needed = MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0);
	buffer = static_cast<wchar_t *>(malloc(needed*sizeof(wchar_t) + 4));
	if (!buffer)
		return NULL;
	MultiByteToWideChar(CP_UTF8, 0, str, -1, &buffer[0], needed);
	return buffer;
}

DLLEXPORT char* CALLCONVENTION
wchar_to_utf8(const wchar_t* str, bool forceUTF8) {
	char* buffer;
	int needed = WideCharToMultiByte(forceUTF8 ? CP_UTF8 : CP_ACP, 0, str, -1, 0, 0, NULL, NULL);
	buffer = static_cast<char *>(malloc(needed + 2));
	if (!buffer)
		return NULL;
	WideCharToMultiByte(forceUTF8 ? CP_UTF8 : CP_ACP, 0, str, -1, &buffer[0], needed, NULL, NULL);
	return buffer;
}

DLLEXPORT void CALLCONVENTION
free_wchar(wchar_t* ptr) {
	free(ptr);
}
#endif

DLLEXPORT void CALLCONVENTION
tqslTrace(const char *name, const char *format, ...) {
	va_list ap;
	if (!tQSL_DiagFile) return;

	time_t t = time(0);
	char timebuf[50];
	strncpy(timebuf, ctime(&t), sizeof timebuf);
	timebuf[strlen(timebuf) - 1] = '\0';		// Strip the newline
	if (!format) {
		fprintf(tQSL_DiagFile, "%s %s\r\n", timebuf, name);
		fflush(tQSL_DiagFile);
		return;
	} else {
		if (name) {
			fprintf(tQSL_DiagFile, "%s %s: ", timebuf, name);
		}
	}
	va_start(ap, format);
	vfprintf(tQSL_DiagFile, format, ap);
	va_end(ap);
	fprintf(tQSL_DiagFile, "\r\n");
	fflush(tQSL_DiagFile);
}

DLLEXPORT void CALLCONVENTION
tqsl_closeDiagFile(void) {
	if (tQSL_DiagFile)
		fclose(tQSL_DiagFile);
	tQSL_DiagFile = NULL;
}

DLLEXPORT int CALLCONVENTION
tqsl_diagFileOpen(void) {
	return tQSL_DiagFile != NULL;
}

DLLEXPORT int CALLCONVENTION
tqsl_openDiagFile(const char *fname) {
#ifdef _WIN32
	wchar_t* lfn = utf8_to_wchar(fname);
	tQSL_DiagFile = _wfopen(lfn, L"wb");
	free_wchar(lfn);
#else
	tQSL_DiagFile = fopen(fname, "wb");
#endif
	return (tQSL_DiagFile == NULL);
}

const char*
tqsl_openssl_error(void) {
	static char buf[256];
	unsigned long openssl_err;

	openssl_err = ERR_peek_error();
	if (openssl_err)
		ERR_error_string_n(openssl_err, buf, sizeof buf);
	else
		strncpy(buf, "[error code not available]", sizeof buf);
	return buf;
}

