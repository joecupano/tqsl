/***************************************************************************
                          tqsllib-doc.h  -  description
                             -------------------
    begin                : Tue Jun 4 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

/** \mainpage
  *
  * The TrustedQSL library API is divided into several groups:
  *
  * \li \ref CertStuff - Request, load and retrieve digital certificates
  * \li \ref Data - Manage station-location data and produce signed data records
  * \li \ref Convert - Convert and sign ADIF and Cabrillo log files
  * \li \ref Util - Functions to operate on objects, set system parameters, and report errors
  * \li \ref Sign - Low-level digital signing
  * \li \ref ADIF - Low-level parsing and creation of ADIF files
  * \li \ref Cabrillo - Low-level parsing of Cabrillo files.
  *
  * Most of the library functions return an integer value that is
  * zero if there is no error and 1 if there is an error. The specific
  * error can be determined by examining #tQSL_Error and, possibly,
  * #tQSL_ADIF_Error, #tQSL_Cabrillo_Error, #tQSL_ErrorFile and
  * #tQSL_CustomError. The tqsl_getErrorString() and tqsl_getErrorString_v()
  * functions can be used to get error text strings.
  *
  */
