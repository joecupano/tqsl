/***************************************************************************
                          tqslerrno.h  -  description
                             -------------------
    begin                : Tue May 28 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __TQSLERRNO_H
#define __TQSLERRNO_H

/** \file
  *  #tQSL_Error values
*/

#define TQSL_NO_ERROR 0		///< No error
#define TQSL_SYSTEM_ERROR 1	///< System Error
#define TQSL_OPENSSL_ERROR 2	///< Error in OpenSSL calls
#define TQSL_ADIF_ERROR 3	///< ADIF Errors
#define TQSL_CUSTOM_ERROR 4	///< Custom errors - output to tQSL_CustomError
#define TQSL_CABRILLO_ERROR 5	///< Cabrillo handler error
#define TQSL_OPENSSL_VERSION_ERROR 6	///< OpenSSL version obsolete
#define TQSL_ERROR_ENUM_BASE 16		///< Base for enumerated errors
#define TQSL_ALLOC_ERROR 16	///< Memory allocation error
#define TQSL_RANDOM_ERROR 17	///< Error initializing random number generator
#define TQSL_ARGUMENT_ERROR 18	///< Invalid arguments
#define TQSL_OPERATOR_ABORT 19	///< Aborted by operator
#define TQSL_NOKEY_ERROR 20	///< No key available
#define TQSL_BUFFER_ERROR 21	///< Insufficient buffer space
#define TQSL_INVALID_DATE 22	///< Date string invalid
#define TQSL_SIGNINIT_ERROR 23	///< Error initializing signing routine
#define TQSL_PASSWORD_ERROR 24	///< Invalid password
#define TQSL_EXPECTED_NAME 25	///< Name expected but not supplied
#define TQSL_NAME_EXISTS 26	///< Entity name exists already
#define TQSL_NAME_NOT_FOUND 27	///< Entity name does not exist
#define TQSL_INVALID_TIME 28	///< Time format is invalid
#define TQSL_CERT_DATE_MISMATCH 29	///< Certificate date mismatch
#define TQSL_PROVIDER_NOT_FOUND 30	///< Certificate provider unknown
#define TQSL_CERT_KEY_ONLY 31		///< No signed public key is installed
#define TQSL_CONFIG_ERROR 32	///< There is an error in the configuration file
#define TQSL_CERT_NOT_FOUND 33	///< The certificate could not be found
#define TQSL_PKCS12_ERROR 34	///< There is an error parsing the .p12 file
#define TQSL_CERT_TYPE_ERROR 35	///< The certificate type is invalid
#define TQSL_DATE_OUT_OF_RANGE 36	///< The date is out of the valid range
#define TQSL_DUPLICATE_QSO 37	///< This QSO is already uploaded
#define TQSL_DB_ERROR 38	///< The dupe database could not be accessed
#define TQSL_LOCATION_NOT_FOUND 39	///< The station location is invalid
#define TQSL_CALL_NOT_FOUND 40	///< The callsign could not be located
#define TQSL_CONFIG_SYNTAX_ERROR 41	///< The config file has a syntax error
#define TQSL_FILE_SYSTEM_ERROR 42	///< There was a file system I/O error
#define TQSL_FILE_SYNTAX_ERROR 43	///< The file format is invalid
#define TQSL_CERT_ERROR 44	///< Callsign certificate could not be installed
#define TQSL_CERT_MISMATCH 45	///< Callsign Certificate does not match QSO details
#define TQSL_LOCATION_MISMATCH 46	///< Station Location does not match QSO details

#endif /* __TQSLERRNO_H */
