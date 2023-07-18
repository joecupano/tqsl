/***************************************************************************
                          convert.h  -  description
                             -------------------
    begin                : Sun Nov 17 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __tqslconvert_h
#define __tqslconvert_h

#include "tqsllib.h"

/** \file
  * tQSL library converter functions.
  */

/** \defgroup Convert Converter API
  *
  * The Converter API provides the capability of converting Cabrillo
  * and ADIF files to GABBI output.
  */
/** @{ */

typedef void * tQSL_Converter;  //!< Opaque converter type used by applications
				//!< to access conversion functions
				//!<

#ifdef __cplusplus
extern "C" {
#endif

/** Create a simple converter object
 *
 * Allocates resources for converting logs and processing duplicate records.
 */

DLLEXPORT int CALLCONVENTION
tqsl_beginConverter(tQSL_Converter *convp);

/** Initiates the conversion process for an ADIF file.
  *
  * \c certs and \c ncerts define a set of certificates that are available to the
  * converter for signing records. Typically, this list will be obtained by
  * calling tqsl_selectCertificates().
  *
  * tqsl_endConverter() should be called to free the resources when the conversion
  * is finished.
  */
DLLEXPORT int CALLCONVENTION tqsl_beginADIFConverter(tQSL_Converter *conv, const char *filename,
	tQSL_Cert *certs, int ncerts, tQSL_Location loc);

/** Initiates the conversion process for a Cabrillo file.
  *
  * \c certs and \c ncerts define a set of certificates that are available to the
  * converter for signing records. Typically, this list will be obtained by
  * calling tqsl_selectCertificates().
  *
  * tqsl_endConverter() should be called to free the resources when the conversion
  * is finished.
  */
DLLEXPORT int CALLCONVENTION tqsl_beginCabrilloConverter(tQSL_Converter *conv, const char *filename,
	tQSL_Cert *certs, int ncerts, tQSL_Location loc);

/** End the conversion process by freeing the used resources. */
DLLEXPORT int CALLCONVENTION tqsl_endConverter(tQSL_Converter *conv);

/** Configure the converter to allow (allow != 0) or disallow (allow == 0)
  * nonamateur call signs in the CALL field. (Note: the test for
  * validity is fairly trivial and will allow some nonamateur calls to
  * get through, but it does catch most common errors.)
  *
  * \c allow defaults to 0 when tqsl_beginADIFConverter or
  * tqsl_beginCabrilloConverter is called.
  */
DLLEXPORT int CALLCONVENTION tqsl_setConverterAllowBadCall(tQSL_Converter conv, int allow);

#define TQSL_LOC_IGNORE 0 ///< Ignore MY_ ADIF fields
#define TQSL_LOC_REPORT 1 ///< Report on MY_ ADIF fields not matching cert/location
#define TQSL_LOC_UPDATE 2 ///< Update Cert/Loc to track MY_ ADIF fields

/** Configure the converter's handing of QTH fields in an adif input file
  *
  * \c allow defaults to 0 when tqsl_beginADIFConverter or
  * tqsl_beginCabrilloConverter is called.
  */
DLLEXPORT int CALLCONVENTION tqsl_setConverterQTHDetails(tQSL_Converter conv, int logverify);

/** Configure the converter to allow (allow != 0) or disallow (allow == 0)
  * duplicate QSOs in a signed log.
  * Duplicate detection is done using QSO details, location details, and
  * certificate serial number.
  *
  * \c allow defaults to 1 for backwards compatibility when tqsl_beginADIFConverter or
  * tqsl_beginCabrilloConverter is called.
  */
DLLEXPORT int CALLCONVENTION tqsl_setConverterAllowDuplicates(tQSL_Converter convp, int ignore);
/** Configure the converter to ignore (ignore != 0) or include (ignore == 0)
  * seconds in times when detecting duplicate QSOs in a signed log.
  * Duplicate detection is done using QSO details, location details, and
  * certificate serial number.
  *
  * \c ignore defaults to 0.
  */
DLLEXPORT int CALLCONVENTION tqsl_setConverterIgnoreSeconds(tQSL_Converter convp, int ignore);

/** Specify the name of the application using the conversion library.
  * This is output in a header record in the exported log file.
  * Call this before calling tqsl_getConverterGABBI.
  *
  * \c app is a c string containing the application name.
  */
DLLEXPORT int CALLCONVENTION tqsl_setConverterAppName(tQSL_Converter convp, const char *app);

/** Roll back insertions into the duplicates database.
  *
  * This is called when cancelling creating a log, and causes any records
  * added to the duplicates database to be removed so re-processing that
  * log does not cause the records to be mis-marked as duplicates.
  */
DLLEXPORT int CALLCONVENTION tqsl_converterRollBack(tQSL_Converter convp);

/** Commits insertions into the duplicates database.
  *
  * This is called when a log is created normally and without issue, and so
  * the presumption is that we are "done" with these QSOs.
  */
DLLEXPORT int CALLCONVENTION tqsl_converterCommit(tQSL_Converter convp);

/** Bulk read the duplicate DB records
  *
  * This is called to retrieve the QSO records from the dupe database.
  * It returns the key/value pair upon each call. 
  * Return -1 for end of file, 0 for success, 1 for errors.
  */
DLLEXPORT int CALLCONVENTION
tqsl_getDuplicateRecords(tQSL_Converter convp, char *key, char *data, int keylen);

/** Bulk read the duplicate DB records
  *
  * This is called to retrieve the QSO records from the dupe database.
  * It returns the key/value pair upon each call. 
  * Return -1 for end of file, 0 for success, 1 for errors.
  * V2 expects a 256 byte buffer for the "data" string.
  */
DLLEXPORT int CALLCONVENTION
tqsl_getDuplicateRecordsV2(tQSL_Converter convp, char *key, char *data, int keylen);

/** Bulk write duplicate DB records
  *
  * This is called to store a QSO record into the dupe database.
  * 
  * Return -1 for duplicate insertion, 0 for success, 1 for errors.
  */
DLLEXPORT int CALLCONVENTION
tqsl_putDuplicateRecord(tQSL_Converter convp, const char *key, const char *data, int keylen);

/** Set QSO date filtering in the converter.
  *
  * If \c start points to a valid date, QSOs prior to that date will be ignored
  * by the converter. Similarly, if \c end points to a valid date, QSOs after
  * that date will be ignored. Either or both may be NULL (or point to an
  * invalid date) to disable date filtering for the respective range.
  */

DLLEXPORT int CALLCONVENTION tqsl_setADIFConverterDateFilter(tQSL_Converter conv, tQSL_Date *start,
	tQSL_Date *end);

/** This is the main converter function. It returns a single GABBI
  * record.
  *
  * Returns the NULL pointer on error or EOF. (Test tQSL_Error to determine which.)
  *
  * tQSL_Error is set to TQSL_DATE_OUT_OF_RANGE if QSO date range checking
  * is active and the QSO date is outside the specified range.
  * This is a non-fatal error.
  *
  * tQSL_Error is set to TQSL_DUPLICATE_QSO if the QSO has already been
  * processed on the current computer.
  *
  * N.B. On systems that distinguish text-mode files from binary-mode files,
  * notably Windows, the GABBI records should be written in binary mode.
  *
  * N.B. If the selected certificate has not been initialized for signing via
  * tqsl_beginSigning(), this function will return a TQSL_SIGNINIT_ERROR.
  * The cert that caused the error can be obtained via tqsl_getConverterCert(),
  * initialized for signing, and then this function can be called again. No
  * data records will be lost in this process.
  */
DLLEXPORT const char* CALLCONVENTION tqsl_getConverterGABBI(tQSL_Converter conv);

/** Get the certificate used to sign the most recent QSO record. */
DLLEXPORT int CALLCONVENTION tqsl_getConverterCert(tQSL_Converter conv, tQSL_Cert *certp);

/** Get the input-file line number last read by the converter, starting
  * at line 1. */
DLLEXPORT int CALLCONVENTION tqsl_getConverterLine(tQSL_Converter conv, int *lineno);

/** Get the text of the last record read by the converter.
  *
  * Returns NULL on error.
  */
DLLEXPORT const char* CALLCONVENTION tqsl_getConverterRecordText(tQSL_Converter conv);

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* __tqslconvert_h */

