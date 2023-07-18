/***************************************************************************
                          cabrillo.h  -  description
                             -------------------
    begin                : Thu Dec 5 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __CABRILLO_H
#define __CABRILLO_H

#include "tqsllib.h"

#undef CLIENT_STATIC
#ifndef LOTW_SERVER
#define CLIENT_STATIC static	///< Static linkage
#else
#define CLIENT_STATIC
#endif

/*! \file */

/** \defgroup Cabrillo Cabrillo API
  *
  * These functions and data structures provide a means of parsing a Cabrillo
  * file into its component fields.
  *
  * For convenience, the returned fields are identified using field names
  * from the \link ADIF ADIF \endlink specification.
  */
/** @{ */

#define TQSL_CABRILLO_MAX_FIELDS 12		///< Max field count
#define TQSL_CABRILLO_FIELD_NAME_LENGTH_MAX 64	///< Max field name length
#define TQSL_CABRILLO_FIELD_VALUE_LENGTH_MAX 40	///< Max field value length

/// Cabrillo status values
typedef enum {
	TQSL_CABRILLO_NO_ERROR,
	TQSL_CABRILLO_EOF,
	TQSL_CABRILLO_NO_START_RECORD,
	TQSL_CABRILLO_NO_CONTEST_RECORD,
	TQSL_CABRILLO_UNKNOWN_CONTEST,
	TQSL_CABRILLO_BAD_FIELD_DATA,
	TQSL_CABRILLO_EOR,
} TQSL_CABRILLO_ERROR_TYPE;		///< Error type

/*! \enum TQSL_CABRILLO_FREQ_TYPE
 * Frequency type: HF, VHF, or UNKNOWN
 */
typedef enum {
	TQSL_CABRILLO_HF,
	TQSL_CABRILLO_VHF,
	TQSL_CABRILLO_UNKNOWN,
} TQSL_CABRILLO_FREQ_TYPE;

// Minimum field number for callsign and default field number
// For VHF, default should be 7.
#define TQSL_MIN_CABRILLO_MAP_FIELD 5	///< First possible call-worked field
#define TQSL_DEF_CABRILLO_MAP_FIELD 8	///< Default call-worked field

/** Cabrillo field data:
  *
  * \li \c name - ADIF field name
  * \li \c value - Field content
  */
typedef struct {	///< Cabrillo field
	char name[TQSL_CABRILLO_FIELD_NAME_LENGTH_MAX +1];	///< Field name
	char value[TQSL_CABRILLO_FIELD_VALUE_LENGTH_MAX +1];	///< Field value
} tqsl_cabrilloField;

typedef void * tQSL_Cabrillo;	///< Opaque cabrillo log type

#ifdef __cplusplus
extern "C" {
#endif

/** Get the Cabrillo error message that corresponds to a particular error value */
DLLEXPORT const char* CALLCONVENTION tqsl_cabrilloGetError(TQSL_CABRILLO_ERROR_TYPE err);

/** Initialize a Cabrillo file for reading */
DLLEXPORT int CALLCONVENTION tqsl_beginCabrillo(tQSL_Cabrillo *cabp, const char *filename);

/** Get the Contest name as specified in the Cabrillo CONTEST line */
DLLEXPORT int CALLCONVENTION tqsl_getCabrilloContest(tQSL_Cabrillo cab, char *buf, int bufsiz);

/** Get the Frequency type (HF or VHF) as determined by the contest */
DLLEXPORT int CALLCONVENTION tqsl_getCabrilloFreqType(tQSL_Cabrillo cab, TQSL_CABRILLO_FREQ_TYPE *type);

/** Get the current line number (starting from 1) of the input file */
DLLEXPORT int CALLCONVENTION tqsl_getCabrilloLine(tQSL_Cabrillo cab, int *lineno);

/** Get the text of the current Cabrillo record */
DLLEXPORT const char* CALLCONVENTION tqsl_getCabrilloRecordText(tQSL_Cabrillo cab);

/** Get the next field of the Cabrillo record
  *
  * \c err is set to \c TQSL_CABRILLO_NO_ERROR or \c TQSL_CABRILLO_EOR (end-of-record)
  * if \c field was populated with data. If \c err == \c TQSL_CABRILLO_EOR, this
  * is the last field of the record.
  *
  * \c err == \c TQSL_CABRILLO_EOF when there is no more data available.
  */
DLLEXPORT int CALLCONVENTION tqsl_getCabrilloField(tQSL_Cabrillo cab, tqsl_cabrilloField *field, TQSL_CABRILLO_ERROR_TYPE *err);

/** Finish reading a Cabrillo file and release its resources */
DLLEXPORT int CALLCONVENTION tqsl_endCabrillo(tQSL_Cabrillo *cabp);

#ifdef __cplusplus
}
#endif

/** @} */

#endif // __CABRILLO_H
