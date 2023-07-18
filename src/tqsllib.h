/***************************************************************************
                          tqsllib.h  -  description
                             -------------------
    begin                : Mon May 20 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: tqsllib.h,v 1.14 2013/03/01 13:26:44 k1mu Exp $
 ***************************************************************************/

#ifndef TQSLLIB_H
#define TQSLLIB_H

#if defined(_WIN32) && !defined(TQSL_NODLL)
	#ifdef TQSLLIB_DEF
		#define DLLEXPORT __declspec(dllexport)
		#define DLLEXPORTDATA __declspec(dllexport)
		#define CALLCONVENTION __stdcall
	#else
		#define DLLEXPORT __declspec(dllimport)
		#define DLLEXPORTDATA __declspec(dllimport)
		#define CALLCONVENTION __stdcall
	#endif
#else
	#define DLLEXPORT	///< Symbol exports - Windows only
	#define DLLEXPORTDATA	///< Symbol exports - Windows only
	#define CALLCONVENTION	///< Symbol exports - Windows only
	#include <limits.h>
#endif

#include "adif.h"
#include "cabrillo.h"

/** \file
  * tQSL library functions.
  */

#ifndef PATH_MAX				// Should be set by <limits.h>
#define PATH_MAX 4096
#endif

/* Sizes */
#define TQSL_MAX_PATH_LEN            PATH_MAX 	///< Max length of a FS path
#define TQSL_PASSWORD_MAX            80		///< Max password length
#define TQSL_NAME_ELEMENT_MAX        256	///< Max Org name length
#define TQSL_CALLSIGN_MAX            20		///< Max callsign length
#define TQSL_CRQ_NAME_MAX            60		///< Max length of request name
#define TQSL_CRQ_ADDR_MAX            80		///< Max length of request addr
#define TQSL_CRQ_CITY_MAX            80		///< Max length of request city
#define TQSL_CRQ_STATE_MAX           80		///< Max length of request state
#define TQSL_CRQ_POSTAL_MAX          20		///< Max length of request zip
#define TQSL_CRQ_COUNTRY_MAX         80		///< Max length of req entity
#define TQSL_CRQ_EMAIL_MAX           180	///< Max length of req email
#define TQSL_BAND_MAX                6		///< Max length of a band name
#define TQSL_MODE_MAX                16		///< Max length of a mode name
#define TQSL_FREQ_MAX                20		///< Max length of a frequency
#define TQSL_SATNAME_MAX             20		///< Max length of a sat name
#define TQSL_PROPMODE_MAX            20		///< Max length of a prop mode
#define TQSL_STATE_MAX		     30		///< Max length of a state name
#define TQSL_GRID_MAX		     30		///< Max length of a grid set
#define TQSL_CNTY_MAX		     30		///< Max length of a county name
#define TQSL_COUNTRY_MAX	     60		///< Max length of a country name
#define TQSL_ZONE_MAX		     5		///< Max length of a zone number
#define TQSL_IOTA_MAX		     10		///< Max length of a IOTA identifier

#define TQSL_CERT_CB_USER            0		///< Callback is for user cert
#define TQSL_CERT_CB_CA              1		///< Callback is for CA cert
#define TQSL_CERT_CB_ROOT            2		///< Callback is for root cert
#define TQSL_CERT_CB_PKEY            3		///< Callback is for private key
#define TQSL_CERT_CB_CONFIG          4		///< Callback for config file
#define TQSL_CERT_CB_CERT_TYPE(x)    ((x) & 0xf) ///< Type of the cert
#define TQSL_CERT_CB_MILESTONE       0		///< New certificate
#define TQSL_CERT_CB_RESULT          0x10	///< Cert import result
#define TQSL_CERT_CB_CALL_TYPE(x)    ((x) & TQSL_CERT_CB_RESULT) ///< Callback type
#define TQSL_CERT_CB_PROMPT          0		///< Callback prompt
#define TQSL_CERT_CB_DUPLICATE       0x100	///< Dupe cert callback
#define TQSL_CERT_CB_ERROR           0x200	///< Error import callback
#define TQSL_CERT_CB_LOADED          0x300	///< Cert loaded callback
#define TQSL_CERT_CB_RESULT_TYPE(x)  ((x) & 0x0f00) ///< Result type mask

typedef void * tQSL_Cert;		///< Opaque certificate type
typedef void * tQSL_Location;		///< Opaque location type
typedef char * tQSL_StationDataEnc;	///< Opaque station data type

/** Struct that holds y-m-d */
typedef struct {
	int year;	///< Numeric year
	int month;	///< Numeric month
	int day;	///< Numeric day
} tQSL_Date;

/** Struct that holds h-m-s */
typedef struct {
	int hour;	///< Time hour field
	int minute;	///< Time minute field
	int second;	///< Time seconds field
} tQSL_Time;

/** Certificate provider data */
typedef struct tqsl_provider_st {
	char organizationName[TQSL_NAME_ELEMENT_MAX+1];	///< Provider name
	char organizationalUnitName[TQSL_NAME_ELEMENT_MAX+1]; ///< Provider unit
	char emailAddress[TQSL_NAME_ELEMENT_MAX+1];	///< Provider e-mail
	char url[TQSL_NAME_ELEMENT_MAX+1];	///< Provider URL
} TQSL_PROVIDER;

/** Certificate request data */
typedef struct tqsl_cert_req_st {		///< Cert request data
	char providerName[TQSL_NAME_ELEMENT_MAX+1];	///< Provider name
	char providerUnit[TQSL_NAME_ELEMENT_MAX+1];	///< Provider unit
	char callSign[TQSL_CALLSIGN_MAX+1];	///< Callsign
	char name[TQSL_CRQ_NAME_MAX+1];		///< Name
	char address1[TQSL_CRQ_ADDR_MAX+1];	///< Address 1
	char address2[TQSL_CRQ_ADDR_MAX+1];	///< Address 2
	char city[TQSL_CRQ_CITY_MAX+1];		///< City
	char state[TQSL_CRQ_STATE_MAX+1];	///< State
	char postalCode[TQSL_CRQ_POSTAL_MAX+1];	///< Postal Code
	char country[TQSL_CRQ_COUNTRY_MAX+1];	///< Country
	char emailAddress[TQSL_CRQ_EMAIL_MAX+1];	///< e-mail
	int dxccEntity;				///< DXCC Entity code
	tQSL_Date qsoNotBefore;			///< QSOs not before date
	tQSL_Date qsoNotAfter;			///< QSOs not after date
	char password[TQSL_PASSWORD_MAX+1];	///< Password
	tQSL_Cert signer;			///< Signing cert
	char renew;				///< Rewewal reference
} TQSL_CERT_REQ;

/** QSO data */
typedef struct {
	char callsign[TQSL_CALLSIGN_MAX+1];	///< QSO callsign
	char band[TQSL_BAND_MAX+1];		///< QSO band
	char mode[TQSL_MODE_MAX+1];		///< QSO mode
	char submode[TQSL_MODE_MAX+1];		///< QSO submode
	tQSL_Date date;				///< QSO date
	tQSL_Time time;				///< QSO time
	char freq[TQSL_FREQ_MAX+1];		///< QSO frequency
	char rxfreq[TQSL_FREQ_MAX+1];		///< QSO receive frequency
	char rxband[TQSL_BAND_MAX+1];		///< QSO RX band
	char propmode[TQSL_PROPMODE_MAX+1];	///< QSO prop mode
	char satname[TQSL_SATNAME_MAX+1];	///< QSO satellite name
	bool callsign_set;			///< QSO specifies a call worked
	bool mode_set;				///< QSO specifies a mode
	bool band_set;				///< QSO specifies a band or frequency
	bool date_set;				///< QSO specifies a date
	bool time_set;				///< QSO specifies a time
	char my_state[TQSL_STATE_MAX+1];	///< QSO specifies MY_STATE
	char my_gridsquare[TQSL_GRID_MAX+1];	///< QSO specifies MY_GRIDSQUARE
	char my_vucc_grids[TQSL_GRID_MAX+1];	///< QSO specifies MY_VUCC_GRIDS
	char my_county[TQSL_CNTY_MAX+1];	///< QSO specifies MY_CNTY
	char my_cnty_state[TQSL_STATE_MAX+1];	///< QSO specifies a state with MY_CNTY
	char my_country[TQSL_COUNTRY_MAX+1];	///< QSO specifies MY_COUNTRY
	char my_cq_zone[TQSL_ZONE_MAX+1];	///< QSO specifies MY_CQ_ZONE
	char my_itu_zone[TQSL_ZONE_MAX+1];	///< QSO specifies MY_ITU_ZONE
	int my_dxcc;				///< QSO specifies MY_DXCC
	char my_call[TQSL_CALLSIGN_MAX+1];	///< Station Callsign
#ifdef USE_OWNER_CALLSIGN
	char my_owner[TQSL_CALLSIGN_MAX+1];	///< Station Owner Callsign
#endif
	char my_operator[TQSL_CALLSIGN_MAX+1];	///< Operator's callsign
	char my_iota[TQSL_IOTA_MAX+1];		///< QSO specifies MY_IOTA_
} TQSL_QSO_RECORD;

/// Base directory for tQSL library working files.
DLLEXPORTDATA extern const char *tQSL_BaseDir;
/// Directory for resources bundled with tqsl executable
DLLEXPORTDATA extern const char *tQSL_RsrcDir;

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup Util Utility API
  */
/** @{ */

/// Error code from most recent tQSL library call.
/**
  * The values for the error code are defined in tqslerrno.h */
DLLEXPORTDATA extern int tQSL_Error;
/// The ADIF error code
DLLEXPORTDATA extern TQSL_ADIF_GET_FIELD_ERROR tQSL_ADIF_Error;
/// The ADIF error code
DLLEXPORTDATA extern TQSL_CABRILLO_ERROR_TYPE tQSL_Cabrillo_Error;
/// File name of file giving error. (May be empty.)
DLLEXPORTDATA extern char tQSL_ErrorFile[TQSL_MAX_PATH_LEN];
/// Custom error message string
DLLEXPORTDATA extern char tQSL_CustomError[256];
/// System errno - stored when tQSL_Error == TQSL_SYSTEM_ERROR
DLLEXPORTDATA extern int tQSL_Errno;
/// Callsign used in import - used for missing public key error
DLLEXPORTDATA extern char tQSL_ImportCall[256];
/// Serial number of recent certificate import
DLLEXPORTDATA extern long tQSL_ImportSerial;
/// Diagnostic log file
DLLEXPORTDATA extern FILE* tQSL_DiagFile;

/** Initialize the tQSL library
  *
  * This function should be called prior to calling any other library functions.
  */
DLLEXPORT int CALLCONVENTION tqsl_init();

/** Set the directory where the TQSL files are kept.
  * May be called either before of after tqsl_init(), but should be called
  * before calling any other functions in the library.
  *
  * Note that this is purely optional. The library will figure out an
  * appropriate directory if tqsl_setDirectory isn't called. Unless there is
  * some particular need to set the directory explicitly, programs should
  * refrain from doing so.
  */
DLLEXPORT int CALLCONVENTION tqsl_setDirectory(const char *dir);

/** Gets the error string for the current tQSL library error and resets the error status.
  * See tqsl_getErrorString_v().
  */
DLLEXPORT const char* CALLCONVENTION tqsl_getErrorString();

/** Gets the error string corresponding to the given error number.
  * The error string is available only until the next call to
  * tqsl_getErrorString_v or tqsl_getErrorString.
  */
DLLEXPORT const char* CALLCONVENTION tqsl_getErrorString_v(int err);

/** Encode a block of data into Base64 text.
  *
  * \li \c data = block of data to encode
  * \li \c datalen = length of \c data in bytes
  * \li \c output = pointer to output buffer
  * \li \c outputlen = size of output buffer in bytes
  */
DLLEXPORT int CALLCONVENTION tqsl_encodeBase64(const unsigned char *data, int datalen, char *output, int outputlen);

/** Decode Base64 text into binary data.
  *
  * \li \c input = NUL-terminated text string of Base64-encoded data
  * \li \c data = pointer to output buffer
  * \li \c datalen = pointer to int containing the size of the output buffer in bytes
  *
  * Places the number of resulting data bytes into \c *datalen.
  */
DLLEXPORT int CALLCONVENTION tqsl_decodeBase64(const char *input, unsigned char *data, int *datalen);

/** Initialize a tQSL_Date object from a date string.
  *
  * The date string must be YYYY-MM-DD or YYYYMMDD format.
  *
  * Returns 0 on success, nonzero on failure
  */
DLLEXPORT int CALLCONVENTION tqsl_initDate(tQSL_Date *date, const char *str);

/** Initialize a tQSL_Time object from a time string.
  *
  * The time string must be HH[:]MM[[:]SS] format.
  *
  * Returns 0 on success, nonzero on failure
  */
DLLEXPORT int CALLCONVENTION tqsl_initTime(tQSL_Time *time, const char *str);

/** Compare two tQSL_Date objects.
  *
  * Returns:
  * - -1 if \c a < \c b
  *
  * - 0 if \c a == \c b
  *
  * - 1 if \c a > \c b
  */
DLLEXPORT int CALLCONVENTION tqsl_compareDates(const tQSL_Date *a, const tQSL_Date *b);

/** Calculate the number of days between two tQSL_Date objects.
  * 
  * Returns a positive result if the first date is earlier, otherwise
  * negative.
  */
DLLEXPORT int CALLCONVENTION tqsl_subtractDates(const tQSL_Date *a, const tQSL_Date *b, int *diff);

/** Converts a tQSL_Date object to a YYYY-MM-DD string.
  *
  * Returns a pointer to \c buf or NULL on error
  */
DLLEXPORT char* CALLCONVENTION tqsl_convertDateToText(const tQSL_Date *date, char *buf, int bufsiz);

/** Test whether a tQSL_Date contains a valid date value
  *
  * Returns 1 if the date is valid
  */
DLLEXPORT int CALLCONVENTION tqsl_isDateValid(const tQSL_Date *d);

/** Test whether a tQSL_Date is empty (contains all zeroes)
  *
  * Returns 1 if the date is null
  */
DLLEXPORT int CALLCONVENTION tqsl_isDateNull(const tQSL_Date *d);

/** Test whether a tQSL_Time contains a valid time value
  *
  * Returns 1 if the time is valid
  */
DLLEXPORT int CALLCONVENTION tqsl_isTimeValid(const tQSL_Time *t);

/** Converts a tQSL_Time object to a HH:MM:SSZ string.
  *
  * Returns a pointer to \c buf or NULL on error
  */
DLLEXPORT char* CALLCONVENTION tqsl_convertTimeToText(const tQSL_Time *time, char *buf, int bufsiz);

/** Returns the library version. \c major and/or \c minor may be NULL.
  */
DLLEXPORT int CALLCONVENTION tqsl_getVersion(int *major, int *minor);

/** Returns the configuration-file version. \c major and/or \c minor may be NULL.
  */
DLLEXPORT int CALLCONVENTION tqsl_getConfigVersion(int *major, int *minor);

/** @} */


/** \defgroup CertStuff Certificate Handling API
  *
  * Certificates are managed by manipulating \c tQSL_Cert objects. A \c tQSL_Cert
  * contains:
  *
  * \li The identity of the organization that issued the certificate (the "issuer").
  * \li The name and call sign of the amateur radio operator (ARO).
  * \li The DXCC entity number for which this certificate is valid.
  * \li The range of QSO dates for which this certificate can be used.
  * \li The resources needed to digitally sign and verify QSO records.
  *
  * The certificate management process consists of:
  *
  * \li <B>Applying for a certificate.</b> Certificate requests are produced via the
  *  tqsl_createCertRequest() function, which produces a certificate-request
  * file to send to the issuer.
  * \li <B>Importing the certificate</B> file received from the issuer into the local
  * "certificate store," a directory managed by the tQSL library, via
  * tqsl_importTQSLFile().
  * \li <B>Selecting an appropriate certificate</B> to use to sign a QSO record via
  * tqsl_selectCertificates().
  */

/** @{ */

#define TQSL_SELECT_CERT_WITHKEYS 1	///< Private keys only (no cert)
#define TQSL_SELECT_CERT_EXPIRED 2	///< Include expired certs
#define TQSL_SELECT_CERT_SUPERCEDED 4	///< Include superseded certs

/** Get a list of certificates
  *
  * Selects a set of certificates from the user's certificate store
  * based on optional selection criteria. The function produces a
  * list of tQSL_Cert objects.
  *
  * \li \c certlist - Pointer to a variable that is set by the
  * function to point to the list of tQSL_Cert objects.
  * \li \c ncerts - Pointer to an int that is set to the number
  * of objects in the \c certlist list.
  * \li \c callsign - Optional call sign to match.
  * \li \c date - Optional QSO date string in ISO format. Only certs
  * that have a QSO date range that encompasses this date will be
  * returned.
  * \li \c issuer - Optional issuer (DN) string to match.
  * \li \c flag - OR of \c TQSL_SELECT_CERT_EXPIRED (include expired
  * certs), \c TQSL_SELECT_CERT_SUPERCEDED and \c TQSL_SELECT_CERT_WITHKEYS
  * (keys that don't have associated certs will be returned).
  *
  * Returns 0 on success, nonzero on failure.
  *
  * Each of the tQSL_Cert objects in the list should be freed
  * by calling tqsl_freeCertificate(). tqsl_freeCertificateList() is a better
  * function to use for that as it also frees the allocated array that
  * holds the certificate pointers.
  *
  */
DLLEXPORT int CALLCONVENTION tqsl_selectCertificates(tQSL_Cert **certlist, int *ncerts,
	const char *callsign, int dxcc, const tQSL_Date *date, const TQSL_PROVIDER *issuer, int flag);

/** Get a list of authority certificates
  *
  * Selects a set of certificates from the root or authorities certificate stores
  * The function produces a list of tQSL_Cert objects.
  *
  * Each of the tQSL_Cert objects in the list should be freed
  * by calling tqsl_freeCertificate(). tqsl_freeCertificateList() is a better
  * function to use for that as it also frees the allocated array that
  * holds the certificate pointers.
  *
  */
DLLEXPORT int CALLCONVENTION tqsl_selectCACertificates(tQSL_Cert **certlist, int *ncerts, const char *type);

/** Get a particulat certificate from the list returnded by
  * tqsl_selectCertificates. This function exists principally
  * to make it easier for VB programs to access the list of
  * certificates.
  *
  * It is the caller's responsibility to ensure that 0 <= idx < ncerts
  * (where ncerts is the value returned by tqsl_selectCertificates)
  */
DLLEXPORT int CALLCONVENTION tqsl_getSelectedCertificate(tQSL_Cert *cert, const tQSL_Cert **certlist,
	int idx);

/** Find out if the "certificate" is expired
  */
DLLEXPORT int CALLCONVENTION tqsl_isCertificateExpired(tQSL_Cert cert, int *status);

/** Find out if the "certificate" is superceded
  */
DLLEXPORT int CALLCONVENTION tqsl_isCertificateSuperceded(tQSL_Cert cert, int *status);

/** Find out if the "certificate" is just a key pair.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateKeyOnly(tQSL_Cert cert, int *keyonly);

/** Get the encoded certificate for inclusion in a GABBI file.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateEncoded(tQSL_Cert cert, char *buf, int bufsiz);

/** Get the encoded private key for inclusion in a backup file.
  */
DLLEXPORT int CALLCONVENTION tqsl_getKeyEncoded(tQSL_Cert cert, char *buf, int bufsiz);

/** Import a base64 encoded certificate and private key from a backup file.
  */
DLLEXPORT int CALLCONVENTION tqsl_importKeyPairEncoded(const char *callsign, const char *type, const char *keybuf, const char *certbuf);

/** Get the issuer's serial number of the certificate.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateSerial(tQSL_Cert cert, long *serial);

/** Get the issuer's serial number of the certificate as a hexadecimal string.
  * Needed for certs with long serial numbers (typically root certs).
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateSerialExt(tQSL_Cert cert, char *serial, int serialsiz);

/** Get the length of the issuer's serial number of the certificate as it will be
  * returned by tqsl_getCertificateSerialExt.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateSerialLength(tQSL_Cert cert);

/** Get the issuer (DN) string from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateIssuer(tQSL_Cert cert, char *buf, int bufsiz);

/** Get the issuer's organization name from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateIssuerOrganization(tQSL_Cert cert, char *buf, int bufsiz);
/** Get the issuer's organizational unit name from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateIssuerOrganizationalUnit(tQSL_Cert cert, char *buf, int bufsiz);
/** Get the ARO call sign string from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateCallSign(tQSL_Cert cert, char *buf, int bufsiz);
/** Get the ARO name string from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateAROName(tQSL_Cert cert, char *buf, int bufsiz);

/** Get the email address from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateEmailAddress(tQSL_Cert cert, char *buf, int bufsiz);

/** Get the QSO not-before date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateQSONotBeforeDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the QSO not-after date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateQSONotAfterDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the certificate's not-before date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateNotBeforeDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the certificate's not-after date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateNotAfterDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the DXCC entity number from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c dxcc - Pointer to an int to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateDXCCEntity(tQSL_Cert cert, int *dxcc);

/** Get the first address line from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateRequestAddress1(tQSL_Cert cert, char *str, int bufsiz);

/** Get the second address line from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateRequestAddress2(tQSL_Cert cert, char *str, int bufsiz);

/** Get the city from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateRequestCity(tQSL_Cert cert, char *str, int bufsiz);

/** Get the state from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateRequestState(tQSL_Cert cert, char *str, int bufsiz);

/** Get the postal (ZIP) code from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateRequestPostalCode(tQSL_Cert cert, char *str, int bufsiz);

/** Get the country from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateRequestCountry(tQSL_Cert cert, char *str, int bufsiz);

#define TQSL_PK_TYPE_ERR	0	///< Error retrieving private key
#define TQSL_PK_TYPE_NONE	1	///< No private key
#define TQSL_PK_TYPE_UNENC	2	///< Private key is not encrypted
#define TQSL_PK_TYPE_ENC	3	///< Private key is encrypted

/** Determine the nature of the private key associated with a
  * certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  *
  * Returns one of the following values:
  *
  * \li \c TQSL_PK_TYPE_ERR - An error occurred. Use tqsl_getErrorString() to examine.
  * \li \c TQSL_PK_TYPE_NONE - No matching private key was found.
  * \li \c TQSL_PK_TYPE_UNENC - The matching private key is unencrypted.
  * \li \c TQSL_PK_TYPE_ENC - The matching private key is encrypted
  * (password protected).
  */
DLLEXPORT int CALLCONVENTION tqsl_getCertificatePrivateKeyType(tQSL_Cert cert);


/** Free the memory used by the tQSL_Cert. Once this function is called,
  * \c cert should not be used again in any way.
  */
DLLEXPORT void CALLCONVENTION tqsl_freeCertificate(tQSL_Cert cert);

/** Free the memory used by a certificate list. The allocated list
 * of tQSL_Certs are freed and the pointer array is freed.
 * Once this function is called, the \c list or the \c cert 
 * should not be used again in any way.
 */
DLLEXPORT void CALLCONVENTION tqsl_freeCertificateList(tQSL_Cert* list, int ncerts);

#define TQSL_CERT_STATUS_UNK	0	///< Status is unknown
#define TQSL_CERT_STATUS_SUP	1	///< Certificate is superceded
#define TQSL_CERT_STATUS_EXP	2	///< Certificate is expired
#define TQSL_CERT_STATUS_OK	3	///< Certificate is valid
#define TQSL_CERT_STATUS_INV	4	///< Invalid serial number

/** Determine the status of a callsign certificate
  * \li \c serial - the serial number of the certificate
  * tqsl_selectCertificates()
  * \li \c status - an integer to receive the certificate status
  *
  * Returns one of the following values:
  *
  * \li \c TQSL_CERT_STATUS_UNK - An error occurred and the status is unknown
  * \li \c TQSL_CERT_STATUS_SUP - The certificate has been superceded
  * \li \c TQSL_CERT_STATUS_EXP - The certificate has expired
  * \li \c TQSL_CERT_STATUS_OK  - The certificate is valid
  * \li \c TQSL_CERT_STATUS_INV	- The serial number supplied is invalid
  *
 */
DLLEXPORT int CALLCONVENTION tqsl_getCertificateStatus(long serial);

/** Store the status of a callsign certificate
  * \li \c serial - serial number of the certificate
  * \li \c status - the status value to store.
 */
DLLEXPORT int CALLCONVENTION tqsl_setCertificateStatus(long serial, const char *status);

/* int tqsl_checkCertificate(tQSL_Cert); */

/** Import a Gabbi cert file received from a CA
  *
  * The callback, \c cb, will be called whenever a certificate is ready
  * to be imported:
  *
  *    cb(type, message);
  *
  * \c type has several fields that can be accessed via macros:
  *
  *  \c TQSL_CERT_CB_CALL_TYPE(type) := \c TQSL_CERT_CB_MILESTONE | \c TQSL_CERT_CB_RESULT
  *
  *  \c TQSL_CERT_CB_CERT_TYPE(type) := \c TQSL_CERT_CB_ROOT | \c TQSL_CERT_CB_CA | \c TQSL_CERT_CB_USER
  *
  *  \c TQSL_CERT_CB_RESULT_TYPE(type) := \c TQSL_CERT_CB_PROMPT | \c TQSL_CERT_CB_WARNING | \c TQSL_CERT_CB_ERROR
  *
  *  \c TQSL_CERT_CB_RESULT_TYPE() is meaningful only if \c TQSL_CERT_CB_CALL_TYPE() == \c TQSL_CERT_CB_RESULT
  */
DLLEXPORT int CALLCONVENTION tqsl_importTQSLFile(const char *file, int(*cb)(int type, const char *message, void *userdata), void *user);

/** Get the serial for the first user cert from a .tq6 file
  * used to support asking the user to save their cert after import
  * \li \c file is the path to the file
  * \li \c serial is where the serial number is returned
  *
  * Returns 0 on success, nonzero on failure.
  *
  */
DLLEXPORT int CALLCONVENTION tqsl_getSerialFromTQSLFile(const char *file, long *serial);

/** Get the number of certificate providers known to tqsllib.
  */
DLLEXPORT int CALLCONVENTION tqsl_getNumProviders(int *n);

/** Get the information for a certificate provider.
  *
  * \li \c idx is the index, 0 <= idx < tqsl_getNumProviders()
  */
DLLEXPORT int CALLCONVENTION tqsl_getProvider(int idx, TQSL_PROVIDER *provider);

/** Create a certificate-request Gabbi file.
  *
  * The \c req parameter must be properly populated with the required fields.
  *
  * If \c req->password is NULL and \c cb is not NULL, the callback will be
  * called to acquire the password. Otherwise \c req->password will be used as
  * the password.  If the password is NULL or an empty string the generated
  * private key will be stored unencrypted.
  *
  * If req->signer is not zero and the signing certificate requires a password,
  * the password may be in req->signer_password, else signer_pwcb is called.
  */
DLLEXPORT int CALLCONVENTION tqsl_createCertRequest(const char *filename, TQSL_CERT_REQ *req,
	int(*pwcb)(char *pwbuf, int pwsize, void *userdata), void *user);

/** Save a key pair and certificates to a file in PKCS12 format.
  *
  * The tQSL_Cert must be initialized for signing (see tqsl_beginSigning())
  * if the user certificate is being exported.
  *
  * The supplied \c p12password is used to encrypt the PKCS12 data.
  */
DLLEXPORT int CALLCONVENTION tqsl_exportPKCS12File(tQSL_Cert cert, const char *filename, const char *p12password);

/** Save a key pair and certificates to a Base64 string in PKCS12 format.
  *
  * The tQSL_Cert must be initialized for signing (see tqsl_beginSigning())
  * if the user certificate is being exported.
  *
  * The supplied \c p12password is used to encrypt the PKCS12 data.
  */

DLLEXPORT int CALLCONVENTION tqsl_exportPKCS12Base64(tQSL_Cert cert,  char *base64, int b64len, const char *p12password);

/** Load certificates and a private key from a PKCS12 file.
  */
DLLEXPORT int CALLCONVENTION tqsl_importPKCS12File(const char *filename, const char *p12password, const char *password,
	int (*pwcb)(char *buf, int bufsiz, void *userdata), int(*cb)(int type , const char *message, void *userdata), void *user);

/** Load certificates and a private key from a Base64 encoded PKCS12 string.
  */
DLLEXPORT int CALLCONVENTION tqsl_importPKCS12Base64(const char *base64, const char *p12password, const char *password,
	int (*pwcb)(char *buf, int bufsiz, void *userdata), int(*cb)(int type , const char *message, void *userdata), void *user);

/** Get the list of restorable station locations. */
DLLEXPORT int CALLCONVENTION tqsl_getDeletedCallsignCertificates(char ***calls, int *ncall, const char *filter);

/** Free the list of restorable Callsign Certificates. */
DLLEXPORT void CALLCONVENTION tqsl_freeDeletedCertificateList(char **list, int nloc);

/** Restore a deleted callsign certificate by callsign. */
DLLEXPORT int CALLCONVENTION tqsl_restoreCallsignCertificate(const char *callsign);

/** Delete a certificate and private key
  */
DLLEXPORT int CALLCONVENTION tqsl_deleteCertificate(tQSL_Cert cert);

/** @} */

/** \defgroup Sign Signing API
  *
  * The Signing API uses a tQSL_Cert (see \ref CertStuff) to digitally
  * sign a block of data.
  */
/** @{ */

/** Initialize the tQSL_Cert object for use in signing.
  *
  * This produces an unencrypted copy of the private key in memory.
  *
  * if \c password is not NULL, it must point to the password to use to decrypt
  * the private key. If \c password is NULL and \c pwcb is not NULL, \c pwcb
  * is called to get the password. If the private key is encrypted and both
  * \c password and \c pwcb are NULL, or if the supplied password fails to
  * decrypt the key, a TQSL_PASSWORD_ERROR error is returned.
  *
  * \c pwcb parameters: \c pwbuf is a pointer to a buffer of \c pwsize chars.
  * The buffer should be NUL-terminated.
  */
DLLEXPORT int CALLCONVENTION tqsl_beginSigning(tQSL_Cert cert, char *password,  int(*pwcb)(char *pwbuf, int pwsize, void *userdata), void *user);

/** Test whether the tQSL_Cert object is initialized for signing.
  *
  * Returns 0 if initialized. Sets tQSL_Error to TQSL_SIGNINIT_ERROR if not.
  */
DLLEXPORT int CALLCONVENTION tqsl_checkSigningStatus(tQSL_Cert cert);

/** Get the maximum size of a signature block that will be produced
  * when the tQSL_Cert is used to sign data. (Note that the size of the
  * signature block is unaffected by the size of the data block being signed.)
  */
DLLEXPORT int CALLCONVENTION tqsl_getMaxSignatureSize(tQSL_Cert cert, int *sigsize);

/** Sign a data block.
  *
  * tqsl_beginSigning() must have been called for
  * the tQSL_Cert object before calling this function.
  */
DLLEXPORT int CALLCONVENTION tqsl_signDataBlock(tQSL_Cert cert, const unsigned char *data, int datalen, unsigned char *sig, int *siglen);

/** Verify a signed data block.
  *
  * tqsl_beginSigning() need \em not have been called.
  */
DLLEXPORT int CALLCONVENTION tqsl_verifyDataBlock(tQSL_Cert cert, const unsigned char *data, int datalen, unsigned char *sig, int siglen);

/** Sign a single QSO record
  *
  * tqsl_beginSigning() must have been called for
  * the tQSL_Cert object before calling this function.
  *
  * \c loc must be a valid tQSL_Location object. See \ref Data.
  */
DLLEXPORT int CALLCONVENTION tqsl_signQSORecord(tQSL_Cert cert, tQSL_Location loc, TQSL_QSO_RECORD *rec, unsigned char *sig, int *siglen);

/** Terminate signing operations for this tQSL_Cert object.
  *
  * This zero-fills the unencrypted private key in memory.
  */
DLLEXPORT int CALLCONVENTION tqsl_endSigning(tQSL_Cert cert);

/** @} */

/** \defgroup Data Data API
  *
  * The Data API is used to form data into TrustedQSL records. A TrustedQSL record
  * consists of a station record and a QSO record. Together, the two records
  * fully describe one station's end of the QSO -- just as a paper QSL card does.
  *
  * The station record contains the callsign and geographic location of the
  * station submitting the QSO record. The library manages the station records.
  * The tqsl_xxxStationLocationCapture functions are used to generate and save
  * a station record. The intent is to provide an interface that makes a step-by-step
  * system (such as a GUI "wizard") easily implemented.
  *
  * The tqsl_getStationLocation() function is used to retrieve station records.
  *
  * With the necessary station location available, a signed GABBI output file can
  * be generated using the tqsl_getGABBIxxxxx functions:
  *
  * \li tqsl_getGABBItCERT() - Returns a GABBI tCERT record for the given tQSL_Cert
  * \li tqsl_getGABBItSTATION() - Returns a GABBI tSTATION record for the given
  *    tQSL_Location
  * \li tqsl_getGABBItCONTACT() - Returns a GABBI tCONTACT record for the given
  *    TQSL_QSO_RECORD, using the given tQSL_Cert and tQSL_Location.
  * \li tqsl_getGABBItCONTACTData() - Returns a GABBI tCONTACT record and the
  * SIGNDATA for the given TQSL_QSO_RECORD, using the given tQSL_Cert and
  * tQSL_Location.
  *
  * The GABBI format requires that the tCERT record contain an integer identifier
  * that is unique within the GABBI file. Similarly, each tSTATION record must
  * contain a unique identifier. Additionally, the tSTATION record must reference
  * the identifier of a preceding tCERT record. Finally, each tCONTACT record must
  * reference a preceding tSTATION record. (A GABBI processor uses these identifiers
  * and references to tie the station and contact records together and to verify
  * their signature via the certificate.) It is the responsibility of the caller
  * to supply these identifiers and to ensure that the supplied references match
  * the tQSL_Cert and tQSL_Location used to create the referenced GABBI records.
  *
  * Station Location Generation
  *
  * The station-location generation process involves determining the values
  * for a number of station-location parameters. Normally this
  * will be done by prompting the user for the values. The responses given
  * by the user may determine which later fields are required. For example,
  * if the user indicates that the DXCC entity is UNITED STATES, a later
  * field would ask for the US state. This field would not be required if the
  * DXCC entity were not in the US.
  *
  * To accommodate the dynamic nature of the field requirements, the fields
  * are ordered such that dependent fields are queried after the field(s)
  * on which they depend. To make this process acceptable in a GUI
  * system, the fields are grouped into pages, where multiple fields may
  * be displayed on the same page. The grouping is such that which fields
  * are within the page is not dependent on any of the values of the
  * fields within the page. That is, a page of fields contains the same
  * fields no matter what value any of the fields contains. (However,
  * the \em values of fields within the page can depend on the values
  * of fields that precede them in the page.)
  *
  * Here is a brief overview of the sequence of events involved in
  * generating a station location interactively, one field at a time:
  *
  * 1) Call tqsl_initStationLocationCapture() (new location) or tqsl_getStationLocation()
  * (existing location).
  *
  * 2) For \c field from 0 to tqsl_getNumLocationField():
  * \li Display the field label [tqsl_getLocationFieldDataLabel()]
  * \li Get the field content from the user. This can be a selection
  * from a list, an entered integer or an entered character string,
  * depending on the value returned by tqsl_getLocationFieldInputType().
  *
  * 3) If tqsl_hasNextStationLocationCapture() returns 1, call
  * tqsl_nextStationLocationCapture() and go back to step 2.
  *
  * In the case of a GUI system, you'll probably want to display the
  * fields in pages. The sequence of events is a bit different:
  *
  * 1) Call tqsl_initStationLocationCapture() (new location) or tqsl_getStationLocation()
  * (existing location).
  *
  * 2) For \c field from 0 to tqsl_getNumLocationField(),
  * \li Display the field label [tqsl_getLocationFieldDataLabel()]
  * \li Display the field-input control This can be a list-selection
  * or an entered character string or integer, depending on the value
  * returned by tqsl_getLocationFieldInputType().
  *
  * 3) Each time the user changes a field, call tqsl_updateStationLocationCapture().
  * This may change the allowable selection for fields that follow the field
  * the user changed, so the control for each of those fields should be updated
  * as in step 2.
  *
  * 4) Once the user has completed entries for the page, if
  * tqsl_hasNextStationLocationCapture() returns 1, call
  * tqsl_nextStationLocationCapture() and go back to step 2.
  *
  * N.B. The first two fields in the station-location capture process are
  * always call sign and DXCC entity, in that order. As a practical matter, these
  * two fields must match the corresponding fields in the available certificates.
  * The library will therefore constrain the values of these fields to match
  * what's available in the certificate store. See \ref CertStuff.
  */

/** @{ */

/* Location field input types */

#define TQSL_LOCATION_FIELD_TEXT	1	///< Text type input field
#define TQSL_LOCATION_FIELD_DDLIST	2	///< Dropdown list input field
#define TQSL_LOCATION_FIELD_LIST	3	///< List type input field
#define TQSL_LOCATION_FIELD_BADZONE	4	///< Used to return zone selection errors

/* Location field data types */
#define TQSL_LOCATION_FIELD_CHAR 1	///< Character field
#define TQSL_LOCATION_FIELD_INT 2	///< Integer field

/** Begin the process of generating a station record */
DLLEXPORT int CALLCONVENTION tqsl_initStationLocationCapture(tQSL_Location *locp);

/** Release the station-location resources. This should be called for
  * any tQSL_Location that was initialized via tqsl_initStationLocationCapture()
  * or tqsl_getStationLocation()
  */
DLLEXPORT int CALLCONVENTION tqsl_endStationLocationCapture(tQSL_Location *locp);

/** Update the pages based on the currently selected settings. */
DLLEXPORT int CALLCONVENTION tqsl_updateStationLocationCapture(tQSL_Location loc);

/** Return the number of station location capture pages. */

DLLEXPORT int CALLCONVENTION tqsl_getNumStationLocationCapturePages(tQSL_Location loc, int *npages);

/** Get the current page number */
DLLEXPORT int CALLCONVENTION tqsl_getStationLocationCapturePage(tQSL_Location loc, int *page);

/** Set the current page number.
  * Typically, the page number will be 1 (the starting page) or a value
  * obtained from tqsl_getStationLocationCapturePage().
  */
DLLEXPORT int CALLCONVENTION tqsl_setStationLocationCapturePage(tQSL_Location loc, int page);

/** Set the certificate flags used in a location page.
  * This is used to enable expired certs (or disable).
  */
DLLEXPORT int CALLCONVENTION tqsl_setStationLocationCertFlags(tQSL_Location loc, int flags);

/** Advance the page to the next one in the page sequence */
DLLEXPORT int CALLCONVENTION tqsl_nextStationLocationCapture(tQSL_Location loc);

/** Return the next page to in the page sequence */
DLLEXPORT int CALLCONVENTION tqsl_getNextStationLocationCapturePage(tQSL_Location loc, int *page);

/** Return the page to the previous one in the page sequence. */
DLLEXPORT int CALLCONVENTION tqsl_prevStationLocationCapture(tQSL_Location loc);

/** Return the previous page in the page sequence. */
DLLEXPORT int CALLCONVENTION tqsl_getPrevStationLocationCapturePage(tQSL_Location loc, int *page);

/** Return the current page in the page sequence. */
DLLEXPORT int CALLCONVENTION tqsl_getCurrentStationLocationCapturePage(tQSL_Location loc, int *page);

/** Returns 1 (in rval) if there is a next page */
DLLEXPORT int CALLCONVENTION tqsl_hasNextStationLocationCapture(tQSL_Location loc, int *rval);

/** Returns 1 (in rval) if there is a previous page */
DLLEXPORT int CALLCONVENTION tqsl_hasPrevStationLocationCapture(tQSL_Location loc, int *rval);

/** Save the station location data. Note that the name must have been
  * set via tqsl_setStationLocationCaptureName if this is a new
  * station location. If the \c overwrite parameter is zero and a
  * station location of that name is already in existence, an error
  * occurs with tQSL_Error set to TQSL_NAME_EXISTS.
  */
DLLEXPORT int CALLCONVENTION tqsl_saveStationLocationCapture(tQSL_Location loc, int overwrite);

/** Get the name of the station location */
DLLEXPORT int CALLCONVENTION tqsl_getStationLocationCaptureName(tQSL_Location loc, char *namebuf, int bufsiz);

/** Set the name of the station location */
DLLEXPORT int CALLCONVENTION tqsl_setStationLocationCaptureName(tQSL_Location loc, const char *name);

/** Get the number of saved station locations */
DLLEXPORT int CALLCONVENTION tqsl_getNumStationLocations(tQSL_Location loc, int *nloc);

/** Get the name of the specified (by \c idx) saved station location */
DLLEXPORT int CALLCONVENTION tqsl_getStationLocationName(tQSL_Location loc, int idx, char *buf, int bufsiz);

/** Get the call sign from the station location */
DLLEXPORT int CALLCONVENTION tqsl_getStationLocationCallSign(tQSL_Location loc, int idx, char *buf, int bufsiz);

/** Get a named field from the station location */
DLLEXPORT int CALLCONVENTION tqsl_getStationLocationField(tQSL_Location locp, const char *name, char *namebuf, int bufsize);

/** Retrieve a saved station location.
  * Once finished with the station location, tqsl_endStationLocationCapture()
  * should be called to release resources.
  */
DLLEXPORT int CALLCONVENTION tqsl_getStationLocation(tQSL_Location *loc, const char *name);

/** Get any errors returned from parsing the selected station location.
  * This should be called after tqsl_getStationLocation to determine if
  * any of the existing fields failed validation. Currently only zone
  * data is validated here, but future validations for things like
  * properly formatted grid squares is likely.
  */
DLLEXPORT int CALLCONVENTION tqsl_getStationLocationErrors(tQSL_Location loc, char *buf, int bufsiz);

/** Return the contents of the station data file as a byte stream.
  * The caller is required to tqsl_freeStationDataEnc() this pointer when done with it.
  */
DLLEXPORT int CALLCONVENTION tqsl_getStationDataEnc(tQSL_StationDataEnc *sdata);

/** Free the pointer returned by tqsl_getStationDataEnc(tQSL_StationDataEnc*)
  */
DLLEXPORT int CALLCONVENTION tqsl_freeStationDataEnc(tQSL_StationDataEnc sdata);

/** Merge saved location data with existing */
DLLEXPORT int CALLCONVENTION tqsl_mergeStationLocations(const char *locdata);

/** Remove the stored station location by name. */
DLLEXPORT int CALLCONVENTION tqsl_deleteStationLocation(const char *name);

/** Restore the deleted station location by name. */
DLLEXPORT int CALLCONVENTION tqsl_restoreStationLocation(const char *name);

/** Get the list of restorable station locations. */
DLLEXPORT int CALLCONVENTION tqsl_getDeletedStationLocations(char ***locp, int *nloc);

/** Free the list of restorable station locations. */
DLLEXPORT void CALLCONVENTION tqsl_freeDeletedLocationList(char **list, int nloc);

/** Get the number of fields on the current station location page */
DLLEXPORT int CALLCONVENTION tqsl_getNumLocationField(tQSL_Location loc, int *numf);

/** Get the number of characters in the label for the specified field */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldDataLabelSize(tQSL_Location loc, int field_num, int *rval);

/** Get the label for the specified field */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldDataLabel(tQSL_Location loc, int field_num, char *buf, int bufsiz);

/** Get the size of the GABBI name of the specified field */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldDataGABBISize(tQSL_Location loc, int field_num, int *rval);

/** Get the GABBI name of the specified field */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldDataGABBI(tQSL_Location loc, int field_num, char *buf, int bufsiz);

/** Get the input type of the input field.
  *
  * \c type will be one of TQSL_LOCATION_FIELD_TEXT, TQSL_LOCATION_FIELD_DDLIST
  * or TQSL_LOCATION_FIELD_LIST
  */
/** Get the number of fields on the current station location page */
DLLEXPORT int CALLCONVENTION tqsl_getNumLocationField(tQSL_Location loc, int *numf);

/** Get the number of characters in the label for the specified field */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldDataLabelSize(tQSL_Location loc, int field_num, int *rval);

/** Get the label for the specified field */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldDataLabel(tQSL_Location loc, int field_num, char *buf, int bufsiz);

/** Get the size of the GABBI name of the specified field */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldDataGABBISize(tQSL_Location loc, int field_num, int *rval);

/** Get the GABBI name of the specified field */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldDataGABBI(tQSL_Location loc, int field_num, char *buf, int bufsiz);

/** Get the input type of the input field.
  *
  * \c type will be one of TQSL_LOCATION_FIELD_TEXT, TQSL_LOCATION_FIELD_DDLIST
  * or TQSL_LOCATION_FIELD_LIST
  */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldInputType(tQSL_Location loc, int field_num, int *type);

/** Get the data type of the input field.
  *
  * \c type will be either TQSL_LOCATION_FIELD_CHAR or TQSL_LOCATION_FIELD_INT
  */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldDataType(tQSL_Location loc, int field_num, int *type);

/** Get the flags for the input field.
  *
  * \c flags  will be either
  * TQSL_LOCATION_FIELD_UPPER		Field is to be uppercased on input
  * TQSL_LOCATION_FIELD_MUSTSEL		Value must be selected
  * TQSL_LOCATION_FIELD_SELNXT		Value must be selected to allow Next/Finish
  *
  */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldFlags(tQSL_Location loc, int field_num, int *flags);

/** Get the length of the input field data. */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldDataLength(tQSL_Location loc, int field_num, int *rval);

/** Get the character data from the specified field.
  *
  * If the field input type (see tqsl_getLocationFieldInputType()) is
  * TQSL_LOCATION_FIELD_DDLIST or TQSL_LOCATION_FIELD_LIST, this will
  * return the text of the selected item.
  */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldCharData(tQSL_Location loc, int field_num, char *buf, int bufsiz);

/** Get the integer data from the specified field.
  *
  * This is only meaningful if the field data type (see tqsl_getLocationFieldDataType())
  * is TQSL_LOCATION_FIELD_INT.
  */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldIntData(tQSL_Location loc, int field_num, int *dat);

/** If the field input type (see tqsl_getLocationFieldInputType()) is
  * TQSL_LOCATION_FIELD_DDLIST or TQSL_LOCATION_FIELD_LIST, gets the
  * index of the selected list item.
  */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldIndex(tQSL_Location loc, int field_num, int *dat);

/** Get the number of items in the specified field's pick list. */
DLLEXPORT int CALLCONVENTION tqsl_getNumLocationFieldListItems(tQSL_Location loc, int field_num, int *rval);

/** Get the text of a specified item of a specified field */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldListItem(tQSL_Location loc, int field_num, int item_idx, char *buf, int bufsiz);

/** Set the text data of a specified field. */
DLLEXPORT int CALLCONVENTION tqsl_setLocationFieldCharData(tQSL_Location loc, int field_num, const char *buf);

/** Set the integer data of a specified field.
  */
DLLEXPORT int CALLCONVENTION tqsl_setLocationFieldIntData(tQSL_Location loc, int field_num, int dat);

/** If the field input type (see tqsl_getLocationFieldInputType()) is
  * TQSL_LOCATION_FIELD_DDLIST or TQSL_LOCATION_FIELD_LIST, sets the
  * index of the selected list item.
  */
DLLEXPORT int CALLCONVENTION tqsl_setLocationFieldIndex(tQSL_Location loc, int field_num, int dat);

/** Get the \e changed status of a field. The changed flag is set to 1 if the
  * field's pick list was changed during the last call to tqsl_updateStationLocationCapture
  * or zero if the list was not changed.
  */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldChanged(tQSL_Location loc, int field_num, int *changed);

/** Get the call sign from the station location. */
DLLEXPORT int CALLCONVENTION tqsl_getLocationCallSign(tQSL_Location loc, char *buf, int bufsiz);

/** Set the call sign for the station location. */
DLLEXPORT int CALLCONVENTION tqsl_setLocationCallSign(tQSL_Location loc, const char *buf, int dxcc);

/** Get a field from the station location. */
DLLEXPORT int CALLCONVENTION tqsl_getLocationField(tQSL_Location locp, const char *field, char *buf, int bufsiz);

/** Get a field label from the station location. */
DLLEXPORT int CALLCONVENTION tqsl_getLocationFieldLabel(tQSL_Location locp, const char *field, char *buf, int bufsiz);

/** Set a field in a station location. */
DLLEXPORT int CALLCONVENTION tqsl_setLocationField(tQSL_Location locp, const char *field, const char *buf);

/** Get the DXCC entity from the station location. */
DLLEXPORT int CALLCONVENTION tqsl_getLocationDXCCEntity(tQSL_Location loc, int *dxcc);

/** Get the QSO details in canonical form. */
DLLEXPORT int CALLCONVENTION tqsl_getLocationQSODetails(tQSL_Location locp, char *buf, int buflen);

/** Get the station location details in canonical form. */
DLLEXPORT int CALLCONVENTION tqsl_getLocationStationDetails(tQSL_Location locp, char *buf, int buflen);

/** Save the json results for a given callsign location Detail. */
DLLEXPORT int CALLCONVENTION tqsl_saveCallsignLocationInfo(const char *callsign, const char *json);

/** Retrieve the json results for a given callsign location Detail. */
DLLEXPORT int CALLCONVENTION tqsl_getCallsignLocationInfo(const char *callsign, char **buf);

/** Get the number of DXCC entities in the primary DXCC list.
  */
DLLEXPORT int CALLCONVENTION tqsl_getNumDXCCEntity(int *number);

/** Get a DXCC entity from the list of DXCC entities by its index.
  */
DLLEXPORT int CALLCONVENTION tqsl_getDXCCEntity(int index, int *number, const char **name);

/** Get the name of a DXCC Entity by its DXCC number.
  */
DLLEXPORT int CALLCONVENTION tqsl_getDXCCEntityName(int number, const char **name);

/** Get the zonemap  of a DXCC Entity by its DXCC number.
  */
DLLEXPORT int CALLCONVENTION tqsl_getDXCCZoneMap(int number, const char **zonemap);

/** Get the start date  of a DXCC Entity by its DXCC number.
  */
DLLEXPORT int CALLCONVENTION tqsl_getDXCCStartDate(int number, tQSL_Date *d);

/** Get the end date  of a DXCC Entity by its DXCC number.
  */
DLLEXPORT int CALLCONVENTION tqsl_getDXCCEndDate(int number, tQSL_Date *d);

/** Get the deleted status of a DXCC Entity by its DXCC number.
  */
DLLEXPORT int CALLCONVENTION tqsl_getDXCCDeleted(int number, int *deleted);

/** Get the number of Band entries in the Band list */
DLLEXPORT int CALLCONVENTION tqsl_getNumBand(int *number);

/** Get a band by its index.
  *
  * \c name - The GAABI name of the band.
  * \c spectrum - HF | VHF | UHF
  * \c low - The low end of the band in kHz (HF) or MHz (VHF/UHF)
  * \c high - The low high of the band in kHz (HF) or MHz (VHF/UHF)
  *
  * Note: \c spectrum, \c low and/or \c high may be NULL.
  */
DLLEXPORT int CALLCONVENTION tqsl_getBand(int index, const char **name, const char **spectrum, int *low, int *high);

/** Get the number of Mode entries in the Mode list */
DLLEXPORT int CALLCONVENTION tqsl_getNumMode(int *number);

/** Get a mode by its index.
  *
  * \c mode - The GAABI mode name
  * \c group - CW | PHONE | IMAGE | DATA
  *
  * Note: \c group may be NULL.
  */
DLLEXPORT int CALLCONVENTION tqsl_getMode(int index, const char **mode, const char **group);

/** Get the number of ADIF Mode entries in the Mode list */
DLLEXPORT int CALLCONVENTION tqsl_getNumADIFMode(int *number);

/** Get an ADIF mode by its index.
  *
  * \c mode - The ADIF mode name
  *
  */
DLLEXPORT int CALLCONVENTION tqsl_getADIFModeEntry(int index, const char **mode);

/** Get the number of Propagation Mode entries in the Propagation Mode list */
DLLEXPORT int CALLCONVENTION tqsl_getNumPropagationMode(int *number);

/** Get a propagation mode by its index.
  *
  * \c name - The GAABI propagation mode name
  * \c descrip - Text description of the propagation mode
  *
  * Note: \c descrip may be NULL.
  */
DLLEXPORT int CALLCONVENTION tqsl_getPropagationMode(int index, const char **name, const char **descrip);

/** Get the number of Satellite entries in the Satellite list */
DLLEXPORT int CALLCONVENTION tqsl_getNumSatellite(int *number);

/** Get a satellite by its index.
  *
  * \c name - The GAABI satellite name
  * \c descrip - Text description of the satellite
  * \c start - The date the satellite entered service
  * \c end - The last date the satellite was in service
  *
  * Note: \c descrip, start and/or end may be NULL.
  */
DLLEXPORT int CALLCONVENTION tqsl_getSatellite(int index, const char **name, const char **descrip,
	tQSL_Date *start, tQSL_Date *end);

/** Clear the map of Cabrillo contests.
  */
DLLEXPORT int CALLCONVENTION tqsl_clearCabrilloMap();

/** Set the mapping of a Cabrillo contest name (as found in the
  * CONTEST line of a Cabrillo file) to the QSO line call-worked field number
  * and the contest type.
  *
  * \c field can have a value of TQSL_MIN_CABRILLO_MAP_FIELD (cabrillo.h)
  * or greater. Field number starts at 1.
  *
  * \c contest_type must be TQSL_CABRILLO_HF or TQSL_CABRILLO_VHF,
  * defined in cabrillo.h
  */
DLLEXPORT int CALLCONVENTION tqsl_setCabrilloMapEntry(const char *contest, int field, int contest_type);

/** Get the mapping of a Cabrillo contest name (as found in the
  * CONTEST line of a Cabrillo file) to a call-worked field number
  * and the contest type.
  *
  * \c fieldnum will be set to 0 if the contest name isn't in the Cabrillo
  * map. Otherwise it is set to the QSO line field number of the call-worked
  * field, with field counting starting at 1.
  *
  * \c contest_type may be NULL. If not, it is set to the Cabrillo contest
  * type (TQSL_CABRILLO_HF or TQSL_CABRILLO_VHF), defined in cabrillo.h.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCabrilloMapEntry(const char *contest, int *fieldnum, int *contest_type);

/** Clear the map of ADIF modes
  */
DLLEXPORT int CALLCONVENTION tqsl_clearADIFModes();

/** Set the mapping of an ADIF mode to a TQSL mode.
*/
DLLEXPORT int CALLCONVENTION tqsl_setADIFMode(const char *adif_item, const char *mode);

/** Map an ADIF mode to its TQSL equivalent.
  */
DLLEXPORT int CALLCONVENTION tqsl_getADIFMode(const char *adif_item, char *mode, int nmode);

/** Map a GABBI mode to its mode/submode pair.
  */
DLLEXPORT int CALLCONVENTION tqsl_getADIFSubMode(const char *adif_item, char *mode, char *submode, int nmode);

/** Get a GABBI record that contains the certificate.
  *
  * \c uid is the value for the CERT_UID field
  *
  * Returns the NULL pointer on error.
  *
  * N.B. On systems that distinguish text-mode files from binary-mode files,
  * notably Windows, the GABBI records should be written in binary mode.
  */
DLLEXPORT const char* CALLCONVENTION tqsl_getGABBItCERT(tQSL_Cert cert, int uid);

/** Get a GABBI record that contains the Station Location data.
  *
  * \li \c uid is the value for the STATION_UID field.
  * \li \c certuid is the value of the associated CERT_UID field.
  *
  * Returns the NULL pointer on error.
  *
  * N.B. On systems that distinguish text-mode files from binary-mode files,
  * notably Windows, the GABBI records should be written in binary mode.
  */
DLLEXPORT const char* CALLCONVENTION tqsl_getGABBItSTATION(tQSL_Location loc, int uid, int certuid);

/** Get a GABBI record that contains the QSO data.
  *
  * \li \c stationuid is the value of the associated STATION_UID field.
  *
  * N.B.: If \c cert is not initialized for signing (see tqsl_beginSigning())
  * the function will return with a TQSL_SIGNINIT_ERROR error.
  *
  * Returns the NULL pointer on error.
  *
  * N.B. On systems that distinguish text-mode files from binary-mode files,
  * notably Windows, the GABBI records should be written in binary mode.
  */
DLLEXPORT const char* CALLCONVENTION tqsl_getGABBItCONTACT(tQSL_Cert cert, tQSL_Location loc, TQSL_QSO_RECORD *qso,
	int stationuid);

/** Get a GABBI record that contains the QSO data along with the associated
  * signdata (QSO data signed to validate the QSO).
  *
  * \li \c stationuid is the value of the associated STATION_UID field.
  *
  * N.B.: If \c cert is not initialized for signing (see tqsl_beginSigning())
  * the function will return with a TQSL_SIGNINIT_ERROR error.
  *
  * Returns the NULL pointer on error.
  *
  * N.B. On systems that distinguish text-mode files from binary-mode files,
  * notably Windows, the GABBI records should be written in binary mode.
  */
DLLEXPORT const char* CALLCONVENTION tqsl_getGABBItCONTACTData(tQSL_Cert cert, tQSL_Location loc, TQSL_QSO_RECORD *qso,
	int stationuid, char *signdata, int sdlen);

/** @} */

/** Output to a diagnostic trace file (if one is opened.
 *
 * \li \c name is the name of the function being executed
 */
DLLEXPORT void CALLCONVENTION tqslTrace(const char *name, const char *format, ...);
/** Close the diagnostic trace file (if it is open)
 */
DLLEXPORT void CALLCONVENTION tqsl_closeDiagFile(void);
/** Close the diagnostic trace file (if it is open)
 */
DLLEXPORT int  CALLCONVENTION tqsl_diagFileOpen(void);
/** Returns true if the log file is open
 *
 */
DLLEXPORT int  CALLCONVENTION tqsl_openDiagFile(const char* file);

#ifdef _WIN32
DLLEXPORT wchar_t* CALLCONVENTION utf8_to_wchar(const char* str);
DLLEXPORT char*    CALLCONVENTION wchar_to_utf8(const wchar_t* str, bool forceUTF8);
DLLEXPORT void     CALLCONVENTION free_wchar(wchar_t* ptr);
#endif

#ifdef __cplusplus
}
#endif

/* Useful defines */
#define TQSL_MAX_PW_LENGTH         32     ///< Password buffer length

#endif /* TQSLLIB_H */
