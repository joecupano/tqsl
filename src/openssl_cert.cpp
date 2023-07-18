/***************************************************************************
                          openssl_cert.c  -  description
                             -------------------
    begin                : Tue May 14 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

/* Routines to massage X.509 certs for tQSL. See openssl_cert.h for overview */

/* 2004-04-10 Fixed tqsl_add_bag_attribute error for OpenSSL > 0.96
   (Thanks to Kenji, JJ1BDX for the fix)
*/

/* Portions liberally copied from OpenSSL's apps source code */

/* ====================================================================
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#define OPENSSL_CERT_SOURCE

#define TQSLLIB_DEF


#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <zlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#ifdef _WIN32
    #include <direct.h>
	#define MKDIR(x, y) _wmkdir(x)
#else
	#define MKDIR(x, y) mkdir(x, y)
	#include <unistd.h>
    #include <dirent.h>
#endif

#define OPENSSL_SUPPRESS_DEPRECATED		// Suppress warnings for deprecated functions

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#undef X509_NAME //http://www.mail-archive.com/openssl-users@openssl.org/msg59116.html
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pkcs12.h>
#include <openssl/opensslv.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/pkcs12.h>

/* Ugly workaround for Openssl 1.0 bug per:
 * http://rt.openssl.org/Ticket/Display.html?user=guest&pass=guest&id=2123
 */
#if (OPENSSL_VERSION_NUMBER == 0x10000003L)
#define i2d_ASN1_SET i2d_ASN1_SET_buggy
#define d2i_ASN1_SET d2i_ASN1_SET_buggy
#define ASN1_seq_unpack ASN1_seq_unpack_buggy
#define ASN1_seq_pack ASN1_seq_pack_buggy
#include <openssl/asn1.h>
#undef i2d_ASN1_SET
#undef d2i_ASN1_SET
#undef ASN1_seq_unpack
#undef ASN1_seq_pack

#ifdef __cplusplus
extern "C" {
#endif
int i2d_ASN1_SET(void *a, unsigned char **pp,
		 i2d_of_void *i2d, int ex_tag, int ex_class,
		 int is_set);
void *d2i_ASN1_SET(void *a, const unsigned char **pp,
		   long length, d2i_of_void *d2i,
		   void (*free_func)(void* p), int ex_tag,
		   int ex_class);
void *ASN1_seq_unpack(const unsigned char *buf, int len,
		      d2i_of_void *d2i, void (*free_func)(void* dummy));
unsigned char *ASN1_seq_pack(void *safes, i2d_of_void *i2d,
			     unsigned char **buf, int *len);
#ifdef __cplusplus
}
#endif

#endif	// OpenSSL v1.0
//  Work with OpenSSL 1.1.0 and later
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#ifndef M_PKCS12_bag_type
# define M_PKCS12_bag_type PKCS12_bag_type
#endif
#ifndef M_PKCS12_cert_bag_type
# define M_PKCS12_cert_bag_type PKCS12_cert_bag_type
#endif
#ifndef M_PKCS12_crl_bag_type
# define M_PKCS12_crl_bag_type PKCS12_cert_bag_type
#endif
#ifndef M_PKCS12_certbag2x509
# define M_PKCS12_certbag2x509 PKCS12_SAFEBAG_get1_cert
#endif
#ifndef M_PKCS12_decrypt_skey
# define M_PKCS12_decrypt_skey PKCS12_decrypt_skey
#endif
#ifndef M_PKCS12_unpack_authsafes
# define M_PKCS12_unpack_authsafes PKCS12_unpack_authsafes
#endif
#ifndef M_PKCS12_pack_authsafes
# define M_PKCS12_pack_authsafes PKCS12_pack_authsafes
#endif
#ifndef PKCS12_get_attr
# define PKCS12_get_attr PKCS12_SAFEBAG_get0_attr
#endif
#ifndef PKCS12_bag_type
# define PKCS12_bag_type PKCS12_SAFEBAG_get_nid
#endif
#ifndef PKCS12_cert_bag_type
# define PKCS12_cert_bag_type PKCS12_SAFEBAG_get_bag_nid
#endif
#if !defined(PKCS12_x5092certbag) && !defined(LIBRESSL_VERSION_NUMBER)
# define PKCS12_x5092certbag PKCS12_SAFEBAG_create_cert
#endif
#ifndef PKCS12_x509crl2certbag
# define PKCS12_x509crl2certbag PKCS12_SAFEBAG_create_crl
#endif
#ifndef  X509_STORE_CTX_trusted_stack
# define X509_STORE_CTX_trusted_stack X509_STORE_CTX_set0_trusted_stack
#endif
#ifndef X509_get_notAfter
# define X509_get_notAfter X509_get0_notAfter
#endif
#ifndef X509_get_notBefore
# define X509_get_notBefore X509_get0_notBefore
#endif
#if !defined (PKCS12_MAKE_SHKEYBAG) && !defined(LIBRESSL_VERSION_NUMBER)
# define PKCS12_MAKE_SHKEYBAG PKCS12_SAFEBAG_create_pkcs8_encrypt
#endif
#ifndef X509_V_FLAG_CB_ISSUER_CHECK
# define X509_V_FLAG_CB_ISSUER_CHECK 0x0
#endif
#else
# define ASN1_STRING_get0_data ASN1_STRING_data
#endif
#include <map>
#include <vector>
#include <set>
#include <string>
#include <fstream>
#include <iostream>

#include "tqsllib.h"
#include "tqslerrno.h"
#include "xml.h"

#include "winstrdefs.h"

#ifdef _MSC_VER //is a visual studio compiler
#include "windirent.h"
#endif

#define tqsl_malloc malloc
#define tqsl_realloc realloc
#define tqsl_calloc calloc
#define tqsl_free free

#define TQSL_OBJ_TO_API(x) (reinterpret_cast<void *>((x)))
#define TQSL_API_TO_OBJ(x, type) ((type)(x))
#define TQSL_API_TO_CERT(x) TQSL_API_TO_OBJ((x), tqsl_cert *)

#include "openssl_cert.h"

using std::vector;
using std::map;
using std::set;
using std::string;
using std::ofstream;
using std::ios;
using std::endl;
using std::exception;
using tqsllib::XMLElement;
using tqsllib::XMLElementList;

#ifdef _WIN32
#define TQSL_OPEN_READ  L"rb"
#define TQSL_OPEN_WRITE  L"wb"
#define TQSL_OPEN_APPEND L"ab"
#else
#define TQSL_OPEN_READ  "r"
#define TQSL_OPEN_WRITE  "w"
#define TQSL_OPEN_APPEND "a"
#endif

#if (OPENSSL_VERSION_NUMBER & 0xfffff000) >= 0x10000000L
#define uni2asc OPENSSL_uni2asc
#define asc2uni OPENSSL_asc2uni
#endif

static char *tqsl_trim(char *buf);
static int tqsl_check_parm(const char *p, const char *parmName);
static TQSL_CERT_REQ *tqsl_copy_cert_req(TQSL_CERT_REQ *userreq);
static TQSL_CERT_REQ *tqsl_free_cert_req(TQSL_CERT_REQ *req, int seterr);
static char *tqsl_make_key_path(const char *callsign, char *path, int size);
static int tqsl_make_key_list(vector< map<string, string> > & keys);
static int tqsl_find_matching_key(X509 *cert, EVP_PKEY **keyp, TQSL_CERT_REQ **crq, const char *password, int (*cb)(char *, int, void *), void *);
static char *tqsl_make_cert_path(const char *filename, char *path, int size);
static char *tqsl_make_backup_path(const char *filename, char *path, int size);
static int tqsl_get_cert_ext(X509 *cert, const char *ext, unsigned char *userbuf, int *buflen, int *crit);
CLIENT_STATIC int tqsl_get_asn1_date(const ASN1_TIME *tm, tQSL_Date *date);
static char *tqsl_sign_base64_data(tQSL_Cert cert, char *b64data);
static int fixed_password_callback(char *buf, int bufsiz, int verify, void *userdata);
static int prompted_password_callback(char *buf, int bufsiz, int verify, void *userfunc);
static int tqsl_check_crq_field(tQSL_Cert cert, char *buf, int bufsiz);
static bool safe_strncpy(char *dest, const char *src, int size);
static int tqsl_ssl_error_is_nofile();
static int tqsl_unlock_key(const char *pem, EVP_PKEY **keyp, const char *password, int (*cb)(char *, int, void *), void *);
static int tqsl_replace_key(const char *callsign, const char *path, map<string, string>& newfields, int (*cb)(int, const char *, void *), void *userdata);
static int tqsl_self_signed_is_ok(int ok, X509_STORE_CTX *ctx);
static int tqsl_expired_is_ok(int ok, X509_STORE_CTX *ctx);
static int tqsl_clear_deleted(const char *callsign, const char *path, EVP_PKEY *cert_key);
static int tqsl_key_exists(const char *callsign, EVP_PKEY *cert_key);
static int tqsl_open_key_file(const char *path);
static int tqsl_read_key(map<string, string>& fields);
static void tqsl_close_key_file(void);

extern const char* tqsl_openssl_error(void);

/* Private data structures */

typedef struct {
	long id;
	X509 *cert;
	EVP_PKEY *key;
	TQSL_CERT_REQ *crq;
	char *pubkey;
	char *privkey;
	unsigned char keyonly;
} tqsl_cert;

typedef struct {
	long id;
	X509 *cert;
} tqsl_crq;

static tqsl_cert * tqsl_cert_new();
static void tqsl_cert_free(tqsl_cert *p);
static int tqsl_cert_check(tqsl_cert *p, bool needcert = true);

struct tqsl_loadcert_handler {
	int type;
	int (*func)(const char *pem, X509 *x, int(*cb)(int type, const char *, void *), void *);
};

static int tqsl_handle_root_cert(const char *, X509 *, int (*cb)(int, const char *, void *), void *);
static int tqsl_handle_ca_cert(const char *, X509 *, int (*cb)(int, const char *, void *), void *);
static int tqsl_handle_user_cert(const char *, X509 *, int (*cb)(int, const char *, void *), void *);

static struct tqsl_loadcert_handler tqsl_loadcert_handlers[] = {
	 { TQSL_CERT_CB_ROOT, &tqsl_handle_root_cert },
	 { TQSL_CERT_CB_CA, &tqsl_handle_ca_cert },
	 { TQSL_CERT_CB_USER, &tqsl_handle_user_cert }
};

static const char *notypes[] = { "" };
/*
static tqsl_adifFieldDefinitions tqsl_cert_file_fields[] = {
	{ "TQSL_CERT", "", TQSL_ADIF_RANGE_TYPE_NONE, 0, 0, 0, NULL, NULL },
	{ "TQSL_CERT_USER", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, &tqsl_load_user_cert },
	{ "TQSL_CERT_CA", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, &tqsl_load_ca_cert },
	{ "TQSL_CERT_ROOT", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, &tqsl_load_root_cert },
};
*/

static unsigned char tqsl_static_buf[2001];

static char ImportCall[256];

#if !defined(__APPLE__) && !defined(_WIN32) && !defined(__clang__)
        #pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

static unsigned char *
tqsl_static_alloc(size_t size) {
	if (size > sizeof tqsl_static_buf)
		return NULL;
	strncpy(reinterpret_cast<char *>(tqsl_static_buf), "<EMPTY>", sizeof tqsl_static_buf);
	return tqsl_static_buf;
}

namespace tqsllib {

int
tqsl_import_cert(const char *data, certtype type, int(*cb)(int, const char *, void *), void *userdata) {
	BIO *bio;
	X509 *cert;
	int stat;
	struct tqsl_loadcert_handler *handler = &(tqsl_loadcert_handlers[type]);

	/* This is a certificate, supposedly. Let's make sure */

	tqslTrace("tqsl_import_cert", NULL);
	bio = BIO_new_mem_buf(reinterpret_cast<void *>(const_cast<char *>(data)), strlen(data));
	if (bio == NULL) {
		tqslTrace("tqsl_import_cert", "BIO mem buf error %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if (cert == NULL) {
		tqslTrace("tqsl_import_cert", "BIO read error, err=%s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	/* It's a certificate. Let's try to add it. Any errors will be
	 * reported via the callback (if any) but will not be fatal unless
	 * the callback says so.
	 */
	ImportCall[0] = '\0';
	tQSL_ImportSerial = 0;
	stat = (*(handler->func))(data, cert, cb, userdata);
	X509_free(cert);
	if (stat) {
		if (tQSL_Error == TQSL_CERT_ERROR) {
			return 1;
		}
		if (cb != NULL) {
			stat = (*cb)(handler->type | TQSL_CERT_CB_RESULT | TQSL_CERT_CB_ERROR, tqsl_getErrorString_v(tQSL_Error), userdata);
			if (stat) {
				tqslTrace("tqsl_import_cert", "import error %d", tQSL_Error);
				return 1;
			} else {
				tqslTrace("tqsl_import_cert", "import error. Handler suppressed.");
			}
		} else {
			/* No callback -- any errors are fatal */
			tqslTrace("tqsl_import_cert", "import error %d", tQSL_Error);
			return 1;
		}
		return stat;
	}
	strncpy(tQSL_ImportCall, ImportCall, sizeof tQSL_ImportCall);
	return 0;
}

int
tqsl_get_pem_serial(const char *pem, long *serial) {
	BIO *bio;
	X509 *cert;

	tqslTrace("tqsl_get_pem_serial", NULL);
	if (tqsl_init())
		return 1;
	if (pem == NULL || serial == NULL) {
		tqslTrace("tqsl_get_pem_serial", "arg error pem=0x%lx, serial=0x%lx", pem, serial);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	bio = BIO_new_mem_buf(reinterpret_cast<void *>(const_cast<char *>(pem)), strlen(pem));
	if (bio == NULL) {
		tqslTrace("tqsl_get_pem_serial", "mem buf error %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	BIO_free(bio);
	if (cert == NULL) {
		tqslTrace("tqsl_get_pem_serial", "cert read error %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	*serial = ASN1_INTEGER_get(X509_get_serialNumber(cert));
	return 0;
}

} // namespace tqsllib

/********** PUBLIC API FUNCTIONS ***********/

DLLEXPORT int CALLCONVENTION
tqsl_createCertRequest(const char *filename, TQSL_CERT_REQ *userreq,
	int (*pwcb)(char *pwbuf, int pwsize, void *), void *userdata) {
	TQSL_CERT_REQ *req = NULL;
	EVP_PKEY *key = NULL;
	X509_REQ *xr = NULL;
	X509_NAME *subj = NULL;
	int nid, len;
	int rval = 1;
	FILE *out = NULL;
	BIO *bio = NULL;
	const EVP_MD *digest = NULL;
	char buf[200];
	char path[TQSL_MAX_PATH_LEN];
	char *cp;
	const EVP_CIPHER *cipher = NULL;
	char *password;
	const char *type;

	tqslTrace("tqsl_createCertRequest", NULL);
	if (tqsl_init())
		return 1;
	if (filename == NULL || userreq == NULL) {
		tqslTrace("tqsl_createCertRequest", "arg error filename=0x%lx, userreq=0x%lx", filename, userreq);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (userreq->signer != NULL && (!tqsl_cert_check(TQSL_API_TO_CERT(userreq->signer))
		|| TQSL_API_TO_CERT(userreq->signer)->key == NULL)) {
		tqslTrace("tqsl_createCertRequest", "arg error signer/key");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if ((req = tqsl_copy_cert_req(userreq)) == NULL) {
		tqslTrace("tqsl_createCertRequest", "Error copying %d", tQSL_Error);
		goto end;
	}

	/* Check parameters for validity */

	tqsl_trim(req->providerName);
	tqsl_trim(req->providerUnit);
	tqsl_trim(req->name);
	if (tqsl_check_parm(req->name, "Name")) {
		tqslTrace("tqsl_createCertRequest", "check_parm Name");
		goto end;
	}
	tqsl_trim(req->callSign);
	if (tqsl_check_parm(req->callSign, "Call Sign")) {
		tqslTrace("tqsl_createCertRequest", "check_parm Call Sign");
		goto end;
	}
	tqsl_trim(req->address1);
	if (tqsl_check_parm(req->address1, "Address")) {
		tqslTrace("tqsl_createCertRequest", "check_parm Address1");
		goto end;
	}
	tqsl_trim(req->address2);
	tqsl_trim(req->city);
	if (tqsl_check_parm(req->city, "City")) {
		tqslTrace("tqsl_createCertRequest", "check_parm City");
		goto end;
	}
	tqsl_trim(req->state);
	tqsl_trim(req->country);
	if (tqsl_check_parm(req->country, "Country")) {
		tqslTrace("tqsl_createCertRequest", "check_parm Country");
		goto end;
	}
	tqsl_trim(req->postalCode);
	tqsl_trim(req->emailAddress);
	if (tqsl_check_parm(req->emailAddress, "Email address")) {
		tqslTrace("tqsl_createCertRequest", "check_parm email");
		goto end;
	}
	if ((cp = strchr(req->emailAddress, '@')) == NULL || strchr(cp, '.') == NULL) {
		strncpy(tQSL_CustomError, "Invalid email address", sizeof tQSL_CustomError);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		tqslTrace("tqsl_createCertRequest", "check_parm email: %s %s", req->emailAddress, tQSL_CustomError);
		goto end;
	}
	if (!tqsl_isDateValid(&(req->qsoNotBefore))) {
		strncpy(tQSL_CustomError, "Invalid date (qsoNotBefore)", sizeof tQSL_CustomError);
		tqslTrace("tqsl_createCertRequest", "check_parm not before: %s %s", req->qsoNotBefore, tQSL_CustomError);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		goto end;
	}
	if (!tqsl_isDateNull(&(req->qsoNotAfter))) {
		if (!tqsl_isDateValid(&(req->qsoNotAfter))) {
			strncpy(tQSL_CustomError, "Invalid date (qsoNotAfter)", sizeof tQSL_CustomError);
			tqslTrace("tqsl_createCertRequest", "check_parm not after: %s %s", req->qsoNotAfter, tQSL_CustomError);
			tQSL_Error = TQSL_CUSTOM_ERROR;
			goto end;
		}
		if (tqsl_compareDates(&(req->qsoNotAfter), &(req->qsoNotBefore)) < 0) {
			strncpy(tQSL_CustomError, "qsoNotAfter date is earlier than qsoNotBefore", sizeof tQSL_CustomError);
			tqslTrace("tqsl_createCertRequest", "check_parm not after: %s %s", req->qsoNotAfter, tQSL_CustomError);
			tQSL_Error = TQSL_CUSTOM_ERROR;
			goto end;
		}
	}

	/* Try opening the output stream */

#ifdef _WIN32
	wchar_t* wfilename = utf8_to_wchar(filename);
	if ((out = _wfopen(wfilename, TQSL_OPEN_WRITE)) == NULL) {
		free_wchar(wfilename);
#else
	if ((out = fopen(filename, TQSL_OPEN_WRITE)) == NULL) {
#endif
		strncpy(tQSL_ErrorFile, filename, sizeof tQSL_ErrorFile);
		tqslTrace("tqsl_createCertRequest", "Open file - system error %s", strerror(errno));
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		goto end;
	}
#ifdef _WIN32
	free_wchar(wfilename);
#endif
	if (fputs("\ntQSL certificate request\n\n", out) == EOF) {
		strncpy(tQSL_ErrorFile, filename, sizeof tQSL_ErrorFile);
		tqslTrace("tqsl_createCertRequest", "Write request file - system error %s", strerror(errno));
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		goto end;
	}
	tqsl_write_adif_field(out, "eoh", 0, NULL, 0);
	type = (req->signer != NULL) ? (req->renew ? "TQSL_CRQ_RENEWAL" : "TQSL_CRQ_ADDITIONAL") : "TQSL_CRQ_NEW";
	int libmaj, libmin, configmaj, configmin;
	tqsl_getVersion(&libmaj, &libmin);
	tqsl_getConfigVersion(&configmaj, &configmin);
	snprintf(buf, sizeof buf, "TQSL: %d.%d.%d, Lib: V%d.%d, Config: %d.%d", TQSL_VERSION_MAJOR,
			TQSL_VERSION_MINOR, TQSL_VERSION_UPDATE, libmaj, libmin, configmaj, configmin);
	tqsl_write_adif_field(out, "TQSL_IDENT", 0, (unsigned char *)buf, -1);
	tqsl_write_adif_field(out, type, 0, NULL, 0);
	tqsl_write_adif_field(out, "TQSL_CRQ_PROVIDER", 0, (unsigned char *)req->providerName, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_PROVIDER_UNIT", 0, (unsigned char *)req->providerUnit, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_EMAIL", 0, (unsigned char *)req->emailAddress, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_NAME", 0, (unsigned char *)req->name, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_ADDRESS1", 0, (unsigned char *)req->address1, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_ADDRESS2", 0, (unsigned char *)req->address2, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_CITY", 0, (unsigned char *)req->city, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_STATE", 0, (unsigned char *)req->state, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_POSTAL", 0, (unsigned char *)req->postalCode, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_COUNTRY", 0, (unsigned char *)req->country, -1);
	snprintf(buf, sizeof buf, "%d", req->dxccEntity);
	tqsl_write_adif_field(out, "TQSL_CRQ_DXCC_ENTITY", 0, (unsigned char *)buf, -1);
	tqsl_convertDateToText(&(req->qsoNotBefore), buf, sizeof buf);
	tqsl_write_adif_field(out, "TQSL_CRQ_QSO_NOT_BEFORE", 0, (unsigned char *)buf, -1);
	if (!tqsl_isDateNull(&(req->qsoNotAfter))) {
		tqsl_convertDateToText(&(req->qsoNotAfter), buf, sizeof buf);
		tqsl_write_adif_field(out, "TQSL_CRQ_QSO_NOT_AFTER", 0, (unsigned char *)buf, -1);
	}

	/* Generate a new key pair */

	if ((key = tqsl_new_rsa_key(1024)) == NULL) {
		tqslTrace("tqsl_createCertRequest", "key create error %d", tQSL_Error);
		goto end;
	}

	/* Make the X.509 certificate request */

	if ((xr = X509_REQ_new()) == NULL) {
		tqslTrace("tqsl_createCertRequest", "req create error %s", tqsl_openssl_error());
		goto err;
	}
	if (!X509_REQ_set_version(xr, 0L)) {
		tqslTrace("tqsl_createCertRequest", "version set error %s", tqsl_openssl_error());
		goto err;
	}
	subj = X509_REQ_get_subject_name(xr);
	nid = OBJ_txt2nid("AROcallsign");
	if (nid != NID_undef)
		X509_NAME_add_entry_by_NID(subj, nid, MBSTRING_ASC, (unsigned char *)req->callSign, -1, -1, 0);
	nid = OBJ_txt2nid("commonName");
	if (nid != NID_undef)
		X509_NAME_add_entry_by_NID(subj, nid, MBSTRING_ASC, (unsigned char *)req->name, -1, -1, 0);
	nid = OBJ_txt2nid("emailAddress");
	if (nid != NID_undef)
		X509_NAME_add_entry_by_NID(subj, nid, MBSTRING_ASC, (unsigned char *)req->emailAddress, -1, -1, 0);
	X509_REQ_set_pubkey(xr, key);
	if ((digest = EVP_sha256()) == NULL) {
		tqslTrace("tqsl_createCertRequest", "evp_sha256 error %s", tqsl_openssl_error());
		goto err;
	}
	if (!X509_REQ_sign(xr, key, digest)) {
		tqslTrace("tqsl_createCertRequest", "req_sign error %s", tqsl_openssl_error());
		goto err;
	}
	if ((bio = BIO_new(BIO_s_mem())) == NULL) {
		tqslTrace("tqsl_createCertRequest", "bio_new error %s", tqsl_openssl_error());
		goto err;
	}
	if (!PEM_write_bio_X509_REQ(bio, xr)) {
		tqslTrace("tqsl_createCertRequest", "write_bio error %s", tqsl_openssl_error());
		goto err;
	}
	len = static_cast<int>(BIO_get_mem_data(bio, &cp));
	tqsl_write_adif_field(out, "TQSL_CRQ_REQUEST", 0, (unsigned char *)cp, len);

	if (req->signer != NULL) {
		char *b64;
		char ibuf[256];

		if ((b64 = tqsl_sign_base64_data(req->signer, cp)) == NULL) {
			fclose(out);
			tqslTrace("tqsl_createCertRequest", "tqsl_sign_base64 error %s", tqsl_openssl_error());
			goto end;
		}
		tqsl_write_adif_field(out, "TQSL_CRQ_SIGNATURE", 0, (unsigned char *)b64, -1);
		tqsl_getCertificateIssuer(req->signer, ibuf, sizeof ibuf);
		tqsl_write_adif_field(out, "TQSL_CRQ_SIGNATURE_CERT_ISSUER", 0, (unsigned char *)ibuf, -1);
		snprintf(ibuf, sizeof ibuf, "%ld", ASN1_INTEGER_get(X509_get_serialNumber(TQSL_API_TO_CERT(req->signer)->cert)));
		tqsl_write_adif_field(out, "TQSL_CRQ_SIGNATURE_CERT_SERIAL", 0, (unsigned char *)ibuf, -1);
	}

	BIO_free(bio);
	bio = NULL;
	tqsl_write_adif_field(out, "eor", 0, NULL, 0);
	if (fclose(out) == EOF) {
		strncpy(tQSL_ErrorFile, filename, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_createCertRequest", "write error %d", errno);
		goto end;
	}
	out = NULL;

	/* Write the key to the key store */

	if (!tqsl_make_key_path(req->callSign, path, sizeof path)) {
		tqslTrace("tqsl_createCertRequest", "make_key_path error %d", errno);
		goto end;
	}
#ifdef _WIN32
	wchar_t* wpath = utf8_to_wchar(path);
	if ((out = _wfopen(wpath, TQSL_OPEN_APPEND)) == NULL) {
		free_wchar(wpath);
#else
	if ((out = fopen(path, TQSL_OPEN_APPEND)) == NULL) {
#endif
		strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_createCertRequest", "opening file error %s", strerror(errno));
		goto end;
	}
#ifdef _WIN32
	free_wchar(wpath);
#endif
	tqsl_write_adif_field(out, "TQSL_CRQ_PROVIDER", 0, (unsigned char *)req->providerName, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_PROVIDER_UNIT", 0, (unsigned char *)req->providerUnit, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_EMAIL", 0, (unsigned char *)req->emailAddress, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_ADDRESS1", 0, (unsigned char *)req->address1, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_ADDRESS2", 0, (unsigned char *)req->address2, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_CITY", 0, (unsigned char *)req->city, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_STATE", 0, (unsigned char *)req->state, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_POSTAL", 0, (unsigned char *)req->postalCode, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_COUNTRY", 0, (unsigned char *)req->country, -1);
	tqsl_write_adif_field(out, "CALLSIGN", 0, (unsigned char *)req->callSign, -1);
	snprintf(buf, sizeof buf, "%d", req->dxccEntity);
	tqsl_write_adif_field(out, "TQSL_CRQ_DXCC_ENTITY", 0, (unsigned char *)buf, -1);
	tqsl_convertDateToText(&(req->qsoNotBefore), buf, sizeof buf);
	tqsl_write_adif_field(out, "TQSL_CRQ_QSO_NOT_BEFORE", 0, (unsigned char *)buf, -1);
	if (!tqsl_isDateNull(&(req->qsoNotAfter))) {
		tqsl_convertDateToText(&(req->qsoNotAfter), buf, sizeof buf);
		tqsl_write_adif_field(out, "TQSL_CRQ_QSO_NOT_AFTER", 0, (unsigned char *)buf, -1);
	}
	if ((bio = BIO_new(BIO_s_mem())) == NULL) {
		tqslTrace("tqsl_createCertRequest", "bio_new error %s", tqsl_openssl_error());
		goto err;
	}
	password = const_cast<char *>(req->password);
	if (password == NULL && pwcb != NULL) {
		if ((*pwcb)(buf, TQSL_MAX_PW_LENGTH, userdata)) {
			tqslTrace("tqsl_createCertRequest", "password abort");
			tQSL_Error = TQSL_OPERATOR_ABORT;
			goto end;
		}
		password = buf;
	}
	if (password != NULL && *password != '\0') {
		if ((cipher = EVP_des_ede3_cbc()) == NULL) {
			tqslTrace("tqsl_createCertRequest", "password error");
			goto err;
		}
		len = strlen(password);
	} else {
		password = NULL;
		len = 0;
	}
	if (!PEM_write_bio_PrivateKey(bio, key, cipher, (unsigned char *)password, len, NULL, NULL)) {
		tqslTrace("tqsl_createCertRequest", "write priv key error %s", tqsl_openssl_error());
		goto err;
	}
	len = static_cast<int>(BIO_get_mem_data(bio, &cp));
	tqsl_write_adif_field(out, "PRIVATE_KEY", 0, (unsigned char *)cp, len);
	BIO_free(bio);
	if ((bio = BIO_new(BIO_s_mem())) == NULL) {
		tqslTrace("tqsl_createCertRequest", "bio_new error %s", tqsl_openssl_error());
		goto err;
	}
	if (!PEM_write_bio_PUBKEY(bio, key)) {
		tqslTrace("tqsl_createCertRequest", "write pubkey %s", tqsl_openssl_error());
		goto err;
	}
	len = static_cast<int>(BIO_get_mem_data(bio, &cp));
	tqsl_write_adif_field(out, "PUBLIC_KEY", 0, (unsigned char *)cp, len);
	BIO_free(bio);
	bio = NULL;
	tqsl_write_adif_field(out, "eor", 0, NULL, 0);
	if (fclose(out) == EOF) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_createCertRequest", "write file error %s", strerror(tQSL_Errno));
		goto end;
	}
	out = NULL;

	rval = 0;
	goto end;
 err:
	tQSL_Error = TQSL_OPENSSL_ERROR;
 end:
	if (bio != NULL)
		BIO_free(bio);
	if (out != NULL)
		fclose(out);
	if (xr != NULL)
		X509_REQ_free(xr);
	if (key != NULL)
		EVP_PKEY_free(key);
	if (req != NULL)
		tqsl_free_cert_req(req, 0);
	return rval;
}

DLLEXPORT int CALLCONVENTION
tqsl_getSelectedCertificate(tQSL_Cert *cert, const tQSL_Cert **certlist,
	int idx) {
	tqslTrace("tqsl_getSelectedCertificate", NULL);
	if (tqsl_init())
		return 1;
	if (certlist == NULL || cert == NULL || idx < 0) {
		tqslTrace("tqsl_getSelectedCertificate", "arg error certlist=0x%lx, cert=0x%lx, idx=%d", certlist, cert, idx);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*cert = (*certlist)[idx];
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_isCertificateExpired(tQSL_Cert cert, int *status) {
	tqslTrace("tqsl_isCertificateExpired", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || status == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_isCertificateExpired", "arg error cert=0x%lx status=0x%lx", cert, status);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		if (status) *status = false;
		return 1;
	}

	int keyonly;
	if (tqsl_getCertificateKeyOnly(cert, &keyonly) == 0 && keyonly) {
		*status = false;
		return 0;
	}

	long serial = 0;
	tqsl_getCertificateSerial(cert, &serial);
	int sts = tqsl_getCertificateStatus(serial);
	if (sts == TQSL_CERT_STATUS_EXP || sts == TQSL_CERT_STATUS_INV) {
		*status = true;
		return 0;
	}
	*status = false;
	/* Check for expired */
	time_t t = time(0);
	struct tm *tm = gmtime(&t);
	tQSL_Date d;
	d.year = tm->tm_year + 1900;
	d.month = tm->tm_mon + 1;
	d.day = tm->tm_mday;
	const ASN1_TIME *ctm;
	if ((ctm = X509_get_notAfter(TQSL_API_TO_CERT(cert)->cert)) == NULL) {
		*status = true;
		return 0;
	} else {
		tQSL_Date cert_na;
		tqsl_get_asn1_date(ctm, &cert_na);
		if (tqsl_compareDates(&cert_na, &d) < 0) {
			*status = true;
			return 0;
		}
	}
	return 0;
}

static TQSL_X509_STACK *xcerts = NULL;
DLLEXPORT int CALLCONVENTION
tqsl_isCertificateSuperceded(tQSL_Cert cert, int *status) {
	char path[TQSL_MAX_PATH_LEN];
	int i;
	X509 *x = NULL;
	char *cp;
	vector< map<string, string> > keylist;
	vector< map<string, string> >::iterator it;
	set<string> superceded_certs;
	int len;
	bool superceded = false;
	char buf[256];

	tqslTrace("tqsl_isCertificateSuperceded", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || status == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_isCertificateSuperceded", "arg error cert=0x%lx, status=0x%lx", cert, status);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}

	*status = false;
	int keyonly;
	if (tqsl_getCertificateKeyOnly(cert, &keyonly) == 0 && keyonly) {
		return 0;
	}

	long serial = 0;
	tqsl_getCertificateSerial(cert, &serial);
	if (tqsl_getCertificateStatus(serial) == TQSL_CERT_STATUS_SUP) {
		*status = true;
		tqslTrace("tqsl_isCertificateSuperceded", "returning true");
		return 0;
	}
	/* Get the certs from the cert store */
	tqsl_make_cert_path("user", path, sizeof path);
	if (xcerts == NULL)
		xcerts = tqsl_ssl_load_certs_from_file(path);
	if (xcerts == NULL) {
		if (tQSL_Error == TQSL_OPENSSL_ERROR) {
			tqslTrace("tqsl_isCertificateSuperceded", "openssl error loading certs %d", tQSL_Error);
			return 1;
		}
	}
	/* Make a list of superceded certs */
	for (i = 0; i < sk_X509_num(xcerts); i++) {
		x = sk_X509_value(xcerts, i);
		len = sizeof buf-1;
		if (!tqsl_get_cert_ext(x, "supercededCertificate", (unsigned char *)buf, &len, NULL)) {
			buf[len] = 0;
			string sup = buf;
			superceded_certs.insert(sup);
			/* Fix - the extension as inserted by ARRL
			 * reads ".../Email=lotw@arrl.org", not
			 * the expected ".../emailAddress=".
			 * save both forms in case this gets
			 * changed at the LoTW site
			 */
			size_t pos = sup.find("/Email");
			if (pos != string::npos) {
				sup.replace(pos, 6, "/emailAddress");
				superceded_certs.insert(sup);
			}
		}
	}

	// "supercededCertificate" extension is <issuer>;<serial>
	cp = X509_NAME_oneline(X509_get_issuer_name(TQSL_API_TO_CERT(cert)->cert), buf, sizeof(buf));
	if (cp == NULL) {
		superceded = false;
		tqslTrace("tqsl_isCertificateSuperceded", "returning false");
	} else {
		string sup = buf;
		sup += ";";
		long serial = 0;
		tqsl_getCertificateSerial(cert, &serial);
		snprintf(buf, sizeof buf, "%ld", serial);
		sup += buf;
		if (superceded_certs.find(sup) != superceded_certs.end()) {
			tqslTrace("tqsl_isCertificateSuperceded", "returning true");
			superceded = true;
		}
	}
	*status = superceded;
	return 0;
}
DLLEXPORT int CALLCONVENTION
tqsl_selectCertificates(tQSL_Cert **certlist, int *ncerts,
	const char *callsign, int dxcc, const tQSL_Date *date, const TQSL_PROVIDER *issuer, int flags) {
	int withkeys = flags & TQSL_SELECT_CERT_WITHKEYS;
	TQSL_X509_STACK *selcerts = NULL;
	char path[TQSL_MAX_PATH_LEN];
	int i;
	X509 *x;
	int rval = 1;
	tqsl_cert *cp;
	TQSL_CERT_REQ *crq;
	BIO *bio = NULL;
	EVP_PKEY *pubkey = NULL;
	EVP_PKEY *curkey = NULL;
	vector< map<string, string> > keylist;
	vector< map<string, string> >::iterator it;
	bool keyerror = false;
	int savedError;
	int savedErrno;

	tqslTrace("tqsl_selectCertificates", "callsign=%s, dxcc=%d, flags=%d", callsign ? callsign : "NULL", dxcc, flags);
	if (tqsl_init())
		return 1;
	if (ncerts == NULL) {
		tqslTrace("tqsl_selectCertificates", "arg error ncerts=0x%lx", ncerts);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*ncerts = 0;
	if (certlist)
		*certlist = NULL;

	/* Convert the dates to tQSL_Date objects */
	if (date && !tqsl_isDateNull(date) && !tqsl_isDateValid(date)) {
		tqslTrace("tqsl_selectCertificates", "arg error - bad date");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}

	/* Get the certs from the cert store */
	tqsl_make_cert_path("user", path, sizeof path);
	if (xcerts == NULL)
		xcerts = tqsl_ssl_load_certs_from_file(path);
	if (xcerts == NULL) {
		if (tQSL_Error == TQSL_OPENSSL_ERROR) {
			tqslTrace("tqsl_selectCertificates", "openssl error");
			return 1;
		} else if (tQSL_Error != TQSL_SYSTEM_ERROR || tQSL_Errno != ENOENT) { // No file
			tqslTrace("tqsl_selectCertificates", "other error %d", tQSL_Error);
			return 1;
		}
	} else {
		selcerts = tqsl_filter_cert_list(xcerts, callsign, dxcc, date, issuer, flags);
	}
	// Get a list of keys and find any unmatched (no cert) ones
	if (withkeys) {
		if (tqsl_make_key_list(keylist)) {
			keyerror = true;		// Remember that an error occurred
			savedError = tQSL_Error;	// but allow the rest of the certs to load
			savedErrno = tQSL_Errno;
			tqslTrace("tqsl_selectCertificates", "make_key_list error %d %d", tQSL_Error, tQSL_Errno);
		}
		if (xcerts != NULL) {
			for (i = 0; i < sk_X509_num(xcerts); i++) {
				x = sk_X509_value(xcerts, i);
				if ((pubkey = X509_get_pubkey(x)) == NULL) {
					tqslTrace("tqsl_selectCertificates", "can't get pubkey");
					goto err;
				}
				for (it = keylist.begin(); it != keylist.end(); it++) {
					int match = 0;
					/* Compare the keys */
					string& keystr = (*it)["PUBLIC_KEY"];
					if ((bio = BIO_new_mem_buf(static_cast<void *>(const_cast<char *>(keystr.c_str())), keystr.length())) == NULL) {
						tqslTrace("tqsl_selectCertifcates", "bio_new error %s", tqsl_openssl_error());
						goto err;
					}
					if ((curkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL)) == NULL) {
						tqslTrace("tqsl_selectCertificates", "pem_read_bio err %s", tqsl_openssl_error());
						goto err;
					}
					BIO_free(bio);
					bio = NULL;
					if (EVP_PKEY_cmp(curkey, pubkey) == 1)
						match = 1;
					EVP_PKEY_free(curkey);
					curkey = NULL;
					if (match) {
						// Remove matched key from list
						keylist.erase(it);
						break;
					}
				}
				EVP_PKEY_free(pubkey);
				pubkey = NULL;
			}
		}
		// Filter keylist
		for (it = keylist.begin(); it != keylist.end(); ) {
			if ((*it)["TQSL_CRQ_PROVIDER"] == "")
				it = keylist.erase(it);
			else if (callsign && (*it)["CALLSIGN"] != callsign)
				it = keylist.erase(it);
			else if (dxcc && strtol((*it)["TQSL_CRQ_DXCC_ENTITY"].c_str(), NULL, 10) != dxcc)
				it = keylist.erase(it);
			else if (issuer && (*it)["TQSL_CRQ_PROVIDER"] != issuer->organizationName)
				it = keylist.erase(it);
			else if (issuer && (*it)["TQSL_CRQ_PROVIDER_UNIT"] != issuer->organizationalUnitName)
				it = keylist.erase(it);
			else
				it++;
		}
	}

//cerr << keylist.size() << " unmatched keys" << endl;

	*ncerts = (selcerts ? sk_X509_num(selcerts) : 0) + keylist.size();
	tqslTrace("tqsl_selectCertificates", "ncerts=%d", *ncerts);
	if (certlist == NULL)		// Only want certificate count
		goto end;
	*certlist = reinterpret_cast<tQSL_Cert *>(tqsl_calloc(*ncerts, sizeof(tQSL_Cert)));
	if (selcerts != NULL) {
		for (i = 0; i < sk_X509_num(selcerts); i++) {
			x = sk_X509_value(selcerts, i);
			if ((cp = tqsl_cert_new()) == NULL) {
				tqslTrace("tqsl_selectCertificates", "error making new cert - %s", tqsl_openssl_error());
				goto end;
			}
			cp->cert = X509_dup(x);
			(*certlist)[i] = TQSL_OBJ_TO_API(cp);
		}
	} else {
		i = 0;
	}
	for (it = keylist.begin(); it != keylist.end(); it++) {
		if ((cp = tqsl_cert_new()) == NULL) {
			tqslTrace("tqsl_selectCertificates", "error making new cert - %s", tqsl_openssl_error());
			goto end;
		}
		crq = reinterpret_cast<TQSL_CERT_REQ *>(tqsl_calloc(1, sizeof(TQSL_CERT_REQ)));
		if (crq != NULL) {
			tQSL_Error = TQSL_BUFFER_ERROR;
			if (!safe_strncpy(crq->providerName, (*it)["TQSL_CRQ_PROVIDER"].c_str(), sizeof crq->providerName))
				goto end;
			if (!safe_strncpy(crq->providerUnit, (*it)["TQSL_CRQ_PROVIDER_UNIT"].c_str(), sizeof crq->providerUnit))
				goto end;
			if (!safe_strncpy(crq->callSign, (*it)["CALLSIGN"].c_str(), sizeof crq->callSign))
				goto end;
			if (!safe_strncpy(crq->name, (*it)["TQSL_CRQ_NAME"].c_str(), sizeof crq->name))
				goto end;
			if (!safe_strncpy(crq->emailAddress, (*it)["TQSL_CRQ_EMAIL"].c_str(), sizeof crq->emailAddress))
				goto end;
			if (!safe_strncpy(crq->address1, (*it)["TQSL_CRQ_ADDRESS1"].c_str(), sizeof crq->address1))
				goto end;
			if (!safe_strncpy(crq->address2, (*it)["TQSL_CRQ_ADDRESS2"].c_str(), sizeof crq->address2))
				goto end;
			if (!safe_strncpy(crq->city, (*it)["TQSL_CRQ_CITY"].c_str(), sizeof crq->city))
				goto end;
			if (!safe_strncpy(crq->state, (*it)["TQSL_CRQ_STATE"].c_str(), sizeof crq->state))
				goto end;
			if (!safe_strncpy(crq->postalCode, (*it)["TQSL_CRQ_POSTAL"].c_str(), sizeof crq->postalCode))
				goto end;
			if (!safe_strncpy(crq->country, (*it)["TQSL_CRQ_COUNTRY"].c_str(), sizeof crq->country))
				goto end;
			crq->dxccEntity = strtol((*it)["TQSL_CRQ_DXCC_ENTITY"].c_str(), NULL, 10);
			tqsl_initDate(&(crq->qsoNotBefore), (*it)["TQSL_CRQ_QSO_NOT_BEFORE"].c_str());
			tqsl_initDate(&(crq->qsoNotAfter), (*it)["TQSL_CRQ_QSO_NOT_AFTER"].c_str());
			tQSL_Error = 0;
		}
		cp->crq = crq;
		int len = strlen((*it)["PUBLIC_KEY"].c_str());
		if (len) {
			cp->pubkey = new char[len+1];
			strncpy(cp->pubkey, (*it)["PUBLIC_KEY"].c_str(), len+1);
		}
		len = strlen((*it)["PRIVATE_KEY"].c_str());
		if (len) {
			cp->privkey = new char[len+1];
			strncpy(cp->privkey, (*it)["PRIVATE_KEY"].c_str(), len+1);
		}
		cp->keyonly = 1;
		(*certlist)[i++] = TQSL_OBJ_TO_API(cp);
	}

	if (keyerror) {				// If an error happened with private key scan
		tQSL_Error = savedError;	// Restore the error status from that
		tQSL_Errno = savedErrno;
		rval = 1;
	} else {
		rval = 0;
	}
	goto end;
 err:
	tQSL_Error = TQSL_OPENSSL_ERROR;
 end:
	if (selcerts != NULL)
		sk_X509_free(selcerts);
	if (bio != NULL)
		BIO_free(bio);
	if (pubkey != NULL)
		EVP_PKEY_free(pubkey);
	if (curkey != NULL)
		EVP_PKEY_free(curkey);
	return rval;
}

DLLEXPORT int CALLCONVENTION
tqsl_selectCACertificates(tQSL_Cert **certlist, int *ncerts, const char *type) {
	TQSL_X509_STACK *cacerts = NULL;
	int rval = 1;
	char path[TQSL_MAX_PATH_LEN];
	int i;
	X509 *x;
	tqsl_cert *cp;
	vector< map<string, string> > keylist;
	vector< map<string, string> >::iterator it;
	tqslTrace("tqsl_selectCACertificates", NULL);

	if (tqsl_init())
		return 1;
	if (certlist == NULL || ncerts == NULL) {
		tqslTrace("tqsl_selectCACertificates", "arg error certlist=0x%lx, ncerts=0x%lx", certlist, ncerts);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}

	/* Get the certs from the cert store */
	tqsl_make_cert_path(type, path, sizeof path);
	cacerts = tqsl_ssl_load_certs_from_file(path);
	if (cacerts == NULL) {
		if (tQSL_Error == TQSL_OPENSSL_ERROR) {
			tqslTrace("tqsl_selectCACertificates", "cacerts openssl error");
			return 1;
		}
	}
	*ncerts = (cacerts ? sk_X509_num(cacerts) : 0) + keylist.size();
	*certlist = reinterpret_cast<tQSL_Cert *>(tqsl_calloc(*ncerts, sizeof(tQSL_Cert)));
	if (cacerts != NULL) {
		for (i = 0; i < sk_X509_num(cacerts); i++) {
			x = sk_X509_value(cacerts, i);
			if ((cp = tqsl_cert_new()) == NULL) {
				tqslTrace("tqsl_selectCACertificates", "cert_new error %s", tqsl_openssl_error());
				goto end;
			}
			cp->cert = X509_dup(x);
			(*certlist)[i] = TQSL_OBJ_TO_API(cp);
		}
	}
	rval = 0;
 end:
	if (cacerts != NULL)
		sk_X509_free(cacerts);
	return rval;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateKeyOnly(tQSL_Cert cert, int *keyonly) {
	tqslTrace("tqsl_getCertificateKeyOnly", "cert=0x%lx, keyonly=0x%lx", cert, keyonly);
	if (tqsl_init())
		return 1;
	if (cert == NULL || keyonly == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_getCertificateKeyOnly", "arg error");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*keyonly = TQSL_API_TO_CERT(cert)->keyonly;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateEncoded(tQSL_Cert cert, char *buf, int bufsiz) {
	BIO *bio = NULL;
	int len;
	char *cp;
	int rval = 1;
	tqslTrace("tqsl_getCertificateEncoded", NULL);

	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_getCertificateEncoded", "arg error cert=0x%lx, buf=0x%lx", cert, buf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if ((bio = BIO_new(BIO_s_mem())) == NULL) {
		tqslTrace("tqsl_getCertificateEncoded", "bio_new err %s", tqsl_openssl_error());
		goto err;
	}
	if (!PEM_write_bio_X509(bio, TQSL_API_TO_CERT(cert)->cert)) {
		tqslTrace("tqsl_getCertificateEncoded", "pem_write_bio err %s", tqsl_openssl_error());
		goto err;
	}
	len = static_cast<int>(BIO_get_mem_data(bio, &cp));
	if (len < bufsiz) {
		memcpy(buf, cp, len);
		buf[len] = 0;
	} else {
		tqslTrace("tqsl_getCertificateEncoded", "buffer error %d needed %d there", len, bufsiz);
		tQSL_Error = TQSL_BUFFER_ERROR;
		goto end;
	}
	rval = 0;
	goto end;
 err:
	tQSL_Error = TQSL_OPENSSL_ERROR;
 end:
	if (bio != NULL)
		BIO_free(bio);
	return rval;
}

DLLEXPORT int CALLCONVENTION
tqsl_getKeyEncoded(tQSL_Cert cert, char *buf, int bufsiz) {
	BIO *b64 = NULL;
	BIO *bio = NULL;
	BIO *out = NULL;
	char callsign[40];
	long  len;
	char *cp;
	vector< map<string, string> > keylist;
	vector< map<string, string> >::iterator it;
	EVP_PKEY *pubkey = NULL;
	EVP_PKEY *curkey = NULL;
	tqslTrace("tqsl_getKeyEncoded", NULL);

	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_getKeyEncoded", "arg error cert=0x%lx, buf=0x%lx", cert, buf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	tQSL_Error = TQSL_OPENSSL_ERROR;
	// If it's 'keyonly', then there's no public key - use the one in the cert.
	if (TQSL_API_TO_CERT(cert)->keyonly) {
		if (TQSL_API_TO_CERT(cert)->privkey == 0) {
			tqslTrace("tqsl_getKeyEncoded", "arg error no private key");
			tQSL_Error = TQSL_ARGUMENT_ERROR;
			return 1;
		}
		strncpy(callsign, TQSL_API_TO_CERT(cert)->crq->callSign, sizeof callsign);
		b64 = BIO_new(BIO_f_base64());
		out = BIO_new(BIO_s_mem());
		out = BIO_push(b64, out);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		if (tqsl_bio_write_adif_field(out, "CALLSIGN", 0, (const unsigned char *)callsign, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		if (tqsl_bio_write_adif_field(out, "PRIVATE_KEY", 0, (const unsigned char *)TQSL_API_TO_CERT(cert)->privkey, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		if (tqsl_bio_write_adif_field(out, "PUBLIC_KEY", 0, (const unsigned char *)TQSL_API_TO_CERT(cert)->pubkey, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		char numbuf[10];
		snprintf(numbuf, sizeof numbuf, "%d", TQSL_API_TO_CERT(cert)->crq->dxccEntity);
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_DXCC_ENTITY", 0, (const unsigned char *)numbuf, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_PROVIDER", 0, (const unsigned char *)TQSL_API_TO_CERT(cert)->crq->providerName, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_PROVIDER_UNIT", 0, (const unsigned char *)TQSL_API_TO_CERT(cert)->crq->providerUnit, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_EMAIL", 0, (const unsigned char *)TQSL_API_TO_CERT(cert)->crq->emailAddress, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_ADDRESS1", 0, (const unsigned char *)TQSL_API_TO_CERT(cert)->crq->address1, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_ADDRESS2", 0, (const unsigned char *)TQSL_API_TO_CERT(cert)->crq->address2, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_CITY", 0, (const unsigned char *)TQSL_API_TO_CERT(cert)->crq->city, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_STATE", 0, (const unsigned char *)TQSL_API_TO_CERT(cert)->crq->state, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_POSTAL", 0, (const unsigned char *)TQSL_API_TO_CERT(cert)->crq->postalCode, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_COUNTRY", 0, (const unsigned char *)TQSL_API_TO_CERT(cert)->crq->country, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			return 1;
		}
		char datebuf[20];
		tqsl_convertDateToText(&(TQSL_API_TO_CERT(cert)->crq->qsoNotAfter), datebuf, sizeof datebuf);
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_QSO_NOT_AFTER", 0, (const unsigned char *)datebuf, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 1;
		}
		tqsl_convertDateToText(&(TQSL_API_TO_CERT(cert)->crq->qsoNotBefore), datebuf, sizeof datebuf);
		if (tqsl_bio_write_adif_field(out, "TQSL_CRQ_QSO_NOT_BEFORE", 0, (const unsigned char *)datebuf, -1)) {
			tqslTrace("tqsl_getKeyEncoded", "write_adif_field error %d", tQSL_Error);
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 1;
		}
		tqsl_bio_write_adif_field(out, "eor", 0, NULL, 0);
		if (BIO_flush(out) != 1) {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, "Error encoding certificate", sizeof tQSL_CustomError);
			BIO_free_all(out);
			tqslTrace("tqsl_getKeyEncoded", "BIO_flush error %s", tqsl_openssl_error());
			return 1;
		}

		len = BIO_get_mem_data(out, &cp);
		if (len > bufsiz) {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Private key buffer size %d is too small - %ld needed", bufsiz, len);
			BIO_free_all(out);
			tqslTrace("tqsl_getKeyEncoded", "buffer size err: %s", tQSL_CustomError);
			return 1;
		}
		memcpy(buf, cp, len);
		buf[len] = '\0';
		BIO_free_all(out);
		return 0;
	}

	if (tqsl_getCertificateCallSign(cert, callsign, sizeof callsign)) {
		tqslTrace("tqsl_getKeyEncoded", "Error getting callsign %d", tQSL_Error);
		return 1;
	}
	if (tqsl_make_key_list(keylist)) {
		tqslTrace("tqsl_getKeyEncoded", "Error making keylist %d", tQSL_Error);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		return 1;
	}

	if ((pubkey = X509_get_pubkey(TQSL_API_TO_CERT(cert)->cert)) == 0) {
		tqslTrace("tqsl_getKeyEncoded", "Error getting pubkey %d", tQSL_Error);
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	// Find the matching private key
	for (it = keylist.begin(); it != keylist.end(); it++) {
		string& keystr = (*it)["PUBLIC_KEY"];
		if ((bio = BIO_new_mem_buf(static_cast<void *>(const_cast<char *>(keystr.c_str())), keystr.length())) == NULL) {
			tqslTrace("tqsl_getKeyEncoded", "Error getting buffer %s", tqsl_openssl_error());
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 1;
		}
		if ((curkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL)) == NULL) {
			BIO_free(bio);
			tqslTrace("tqsl_getKeyEncoded", "Error reading PUBKEY %s", tqsl_openssl_error());
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 1;
		}
		BIO_free(bio);
		bio = NULL;

		if (EVP_PKEY_cmp(curkey, pubkey) == 1) {
			// This is the matching private key. Let's feed it back.
			EVP_PKEY_free(curkey);
			curkey = NULL;
			EVP_PKEY_free(pubkey);
			pubkey = NULL;
			b64 = BIO_new(BIO_f_base64());
			out = BIO_new(BIO_s_mem());
			out = BIO_push(b64, out);
			map<string, string>::iterator mit;
			for (mit = it->begin(); mit != it->end(); mit++) {
				if (tqsl_bio_write_adif_field(out, mit->first.c_str(), 0, (const unsigned char *)mit->second.c_str(), -1)) {
					tQSL_Error = TQSL_SYSTEM_ERROR;
					tqslTrace("tqsl_getKeyEncoded", "Error writing field %s", tqsl_openssl_error());
					return 1;
				}
			}
			tqsl_bio_write_adif_field(out, "eor", 0, NULL, 0);
				if (BIO_flush(out) != 1) {
				tQSL_Error = TQSL_CUSTOM_ERROR;
				tqslTrace("tqsl_getKeyEncoded", "Error flushing write %s", tqsl_openssl_error());
				strncpy(tQSL_CustomError, "Error encoding certificate", sizeof tQSL_CustomError);
				BIO_free_all(out);
				return 1;
			}

			len = BIO_get_mem_data(out, &cp);
			if (len > bufsiz) {
				tQSL_Error = TQSL_CUSTOM_ERROR;
				snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Private key buffer size %d is too small - %ld needed", bufsiz, len);
				tqslTrace("tqsl_getKeyEncoded", "Buffer err %s", tQSL_CustomError);
				BIO_free_all(out);
				return 1;
			}
			memcpy(buf, cp, len);
			buf[len] = '\0';
			BIO_free_all(out);
			return 0;
		} else {
			EVP_PKEY_free(curkey);
			curkey = NULL;
		}
	}
	if (pubkey != NULL)
		EVP_PKEY_free(pubkey);
	tqslTrace("tqsl_getKeyEncoded", "private key not found");
	tQSL_Error = TQSL_CUSTOM_ERROR;
	snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Private key not found for callsign %s", callsign);
	return 1;	// Private key not found
}

DLLEXPORT int CALLCONVENTION
tqsl_importKeyPairEncoded(const char *callsign, const char *type, const char *keybuf, const char *certbuf) {
	BIO *in = NULL;
	BIO *b64 = NULL;
	BIO *pub = NULL;
	X509 *cert;
	char path[TQSL_MAX_PATH_LEN];
	char temppath[TQSL_MAX_PATH_LEN];
	char biobuf[4096];
	int cb = 0;
	map<string, string> fields;
	void* userdata = NULL;

	tqslTrace("tqsl_importKeyPairEncoded", NULL);

	if (tqsl_init())
		return 1;
	if (certbuf == NULL || type == NULL) {
		tqslTrace("tqsl_importKeyPairEncoded", "arg error certbuf=0x%lx, type=0x%lx", certbuf, type);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (strcmp(type, "user") == 0) {
		if (keybuf == NULL) {
			tqslTrace("tqsl_importKeyPairEncoded", "arg error user cert keybuf null");
			tQSL_Error = TQSL_ARGUMENT_ERROR;
			return 1;
		}
		cb = TQSL_CERT_CB_USER;
	} else if (strcmp(type, "root") == 0) {
		cb = TQSL_CERT_CB_ROOT;
	} else if (strcmp(type, "authorities") == 0) {
		cb = TQSL_CERT_CB_CA;
	} else {
		tqslTrace("tqsl_importKeyPairEncoded", "arg error type unknown");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (keybuf) {
		if (!tqsl_make_key_path(callsign, path, sizeof path)) {
			goto noprv;
		}

		in = BIO_new_mem_buf(static_cast<void *>(const_cast<char *>(keybuf)), strlen(keybuf));
		if (in == NULL) {
			goto noprv;
		}

		b64 = BIO_new(BIO_f_base64());
		in = BIO_push(b64, in);

		size_t bloblen;
		bloblen = BIO_read(in, biobuf, strlen(keybuf));
		biobuf[bloblen] = '\0';

		strncpy(temppath, tQSL_BaseDir, sizeof temppath);
		FILE *temp = NULL;
#ifdef _WIN32
		strncat(temppath, "\\pk.tmp", sizeof temppath - strlen(temppath) -1);
		wchar_t* wpath = utf8_to_wchar(temppath);
		if ((temp = _wfopen(wpath, TQSL_OPEN_WRITE)) == NULL) {
			free_wchar(wpath);
#else
		strncat(temppath, "/pk.tmp", sizeof temppath - strlen(temppath) -1);
		if ((temp = fopen(temppath, TQSL_OPEN_WRITE)) == NULL) {
#endif
			strncpy(tQSL_ErrorFile, temppath, sizeof tQSL_ErrorFile);
			tqslTrace("tqsl_importKeyPairEncoded", "Open file - system error %s", strerror(errno));
			tQSL_Error = TQSL_SYSTEM_ERROR;
			tQSL_Errno = errno;
			return 1;
		}
#ifdef _WIN32
		free_wchar(wpath);
#endif
		if (fputs(biobuf, temp) == EOF) {
			strncpy(tQSL_ErrorFile, temppath, sizeof tQSL_ErrorFile);
			tqslTrace("tqsl_importKeyPairEncoded", "Write request file - system error %s", strerror(errno));
			tQSL_Error = TQSL_SYSTEM_ERROR;
			tQSL_Errno = errno;
			return 1;
		}
		if (fclose(temp) == EOF) {
			strncpy(tQSL_ErrorFile, temppath, sizeof tQSL_ErrorFile);
			tQSL_Error = TQSL_SYSTEM_ERROR;
			tQSL_Errno = errno;
			tqslTrace("tqsl_importKeyPairEncoded", "write error %d", errno);
			return 1;
		}

// Now is there a private key already with this serial?
		char *pubkey = strstr(biobuf, "-----BEGIN PUBLIC KEY-----");
		char *endpub = strstr(biobuf, "-----END PUBLIC KEY-----");
		int publen = endpub - pubkey + strlen("-----END PUBLIC KEY-----");
		if (pubkey) {
			EVP_PKEY *new_key = NULL;
			if ((pub = BIO_new_mem_buf(reinterpret_cast<void *>(pubkey), publen)) == NULL) {
				goto noprv;
			}
			if ((new_key = PEM_read_bio_PUBKEY(pub, NULL, NULL, NULL)) == NULL) {
				goto noprv;
			}
			BIO_free(pub);
			pub = 0;
			if (!tqsl_key_exists(callsign, new_key)) {
				// Populate fields from the temp file
				if (!tqsl_open_key_file(temppath)) {
					if (!tqsl_read_key(fields)) {
						tqsl_replace_key(callsign, path, fields, NULL, userdata);
					}
					tqsl_close_key_file();
				}
			}
			BIO_free_all(in);
		}
	} // Import of private key

 noprv:
	if (strlen(certbuf) == 0) 		// Keyonly 'certificates'
		return 0;

	// Now process the certificate
	in = BIO_new_mem_buf(static_cast<void *>(const_cast<char *>(certbuf)), strlen(certbuf));
	if (in == NULL) {
		tqslTrace("tqsl_importKeyPairEncoded", "cert new_mem_buf err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	cert = PEM_read_bio_X509(in, NULL, NULL, NULL);
	BIO_free(in);
	if (cert == NULL) {
		tqslTrace("tqsl_importKeyPairEncoded", "read_bio_x509 err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	int ret = tqsl_store_cert(certbuf, cert, type, cb, true, NULL, NULL);
	// it's OK if installing the cert gets a dupe
	if (ret != 0 && tQSL_Error == TQSL_CERT_ERROR) {
		ret = 0;
	}
	return ret;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateCallSign(tQSL_Cert cert, char *buf, int bufsiz) {
	char nbuf[40];
	TQSL_X509_NAME_ITEM item;

	tqslTrace("tqsl_getCertificateCallSign", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_getCertificateCallSign", "arg err cert=0x%lx buf=0x%lx", cert, buf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->keyonly && TQSL_API_TO_CERT(cert)->crq) {
		// Handle the key-only case
		if (bufsiz <= static_cast<int>(strlen(TQSL_API_TO_CERT(cert)->crq->callSign))) {
			tqslTrace("tqsl_getCertificateCallSign", "bufsiz=%d, needed=%d", bufsiz, static_cast<int>(strlen(TQSL_API_TO_CERT(cert)->crq->callSign)));
			tQSL_Error = TQSL_BUFFER_ERROR;
			return 1;
		}
		strncpy(buf, TQSL_API_TO_CERT(cert)->crq->callSign, bufsiz);
		tqslTrace("tqsl_getCertificateCallSign", "KeyOnly, call=%s", buf);
		return 0;
	}
	item.name_buf = nbuf;
	item.name_buf_size = sizeof nbuf;
	item.value_buf = buf;
	item.value_buf_size = bufsiz;
	int ret = tqsl_cert_get_subject_name_entry(TQSL_API_TO_CERT(cert)->cert, "AROcallsign", &item);
	tqslTrace("tqsl_getCertificateCallSign", "Result=%d, call=%s", ret, buf);
	return !ret;
}


DLLEXPORT int CALLCONVENTION
tqsl_getCertificateAROName(tQSL_Cert cert, char *buf, int bufsiz) {
	char nbuf[40];
	TQSL_X509_NAME_ITEM item;

	tqslTrace("tqsl_getCertificateAROName", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_getCertificateAROName", "cert=0x%lx, buf=0x%lx", cert, buf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	item.name_buf = nbuf;
	item.name_buf_size = sizeof nbuf;
	item.value_buf = buf;
	item.value_buf_size = bufsiz;
	return !tqsl_cert_get_subject_name_entry(TQSL_API_TO_CERT(cert)->cert, "commonName", &item);
}


DLLEXPORT int CALLCONVENTION
tqsl_getCertificateEmailAddress(tQSL_Cert cert, char *buf, int bufsiz) {
	char nbuf[40];
	TQSL_X509_NAME_ITEM item;

	tqslTrace("tqsl_getCertificateEmailAddress", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_getCertificateEmailAddress", "arg err cert=0x%lx, buf=0x%lx", cert, buf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	item.name_buf = nbuf;
	item.name_buf_size = sizeof nbuf;
	item.value_buf = buf;
	item.value_buf_size = bufsiz;
	return !tqsl_cert_get_subject_name_entry(TQSL_API_TO_CERT(cert)->cert, "emailAddress", &item);
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateSerial(tQSL_Cert cert, long *serial) {
	tqslTrace("tqsl_getCertificateSerial", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || serial == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_getCertificateSerial", "arg err cert=0x%lx, serial=0x%lx", cert, serial);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*serial = ASN1_INTEGER_get(X509_get_serialNumber(TQSL_API_TO_CERT(cert)->cert));
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateSerialExt(tQSL_Cert cert, char *serial, int serialsiz) {
	tqslTrace("tqsl_getCertificateSerialExt", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || serial == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert)) || serialsiz < 1) {
		tqslTrace("tqsl_getCertificateSerialExt", "arg err cert=0x%lx, serial=0x%lx", cert, serial);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	BIGNUM *bn = BN_new();
	ASN1_INTEGER_to_BN(X509_get_serialNumber(TQSL_API_TO_CERT(cert)->cert), bn);
	char *s = BN_bn2hex(bn);
	strncpy(serial, s, serialsiz);
	serial[serialsiz-1] = 0;
	OPENSSL_free(s);
	BN_free(bn);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateSerialLength(tQSL_Cert cert) {
	int rval;
	tqslTrace("tqsl_getCertificateSerialLength", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL) {
		tqslTrace("tqsl_getCertificateSerialLength", "arg error,cert=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	BIGNUM *bn = BN_new();
	ASN1_INTEGER_to_BN(X509_get_serialNumber(TQSL_API_TO_CERT(cert)->cert), bn);
	char *s = BN_bn2hex(bn);
	rval = strlen(s);
	OPENSSL_free(s);
	BN_free(bn);
	return rval;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateIssuer(tQSL_Cert cert, char *buf, int bufsiz) {
	char *cp;

	tqslTrace("tqsl_getCertificateIssuer", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_getCertificateIssuer", "arg err cert=0x%lx, buf=0x%lx", cert, buf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	cp = X509_NAME_oneline(X509_get_issuer_name(TQSL_API_TO_CERT(cert)->cert), buf, bufsiz);
	if (cp == NULL) {
		tqslTrace("tqsl_getCertificateIssuer", "X509_NAME_oneline error %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
	}
	return (cp == NULL);
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateIssuerOrganization(tQSL_Cert cert, char *buf, int bufsiz) {
	char nbuf[40];
	TQSL_X509_NAME_ITEM item;
	X509_NAME *iss;

	tqslTrace("tqsl_getCertificateIssuerOrganization", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_getCertificateIssuerOrganization", "arg error cert=0x%lx buf=0x%lx", cert, buf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->keyonly && TQSL_API_TO_CERT(cert)->crq) {
		// Handle the key-only case
		if (bufsiz <= static_cast<int>(strlen(TQSL_API_TO_CERT(cert)->crq->providerName))) {
			tqslTrace("tqsl_getCertificateIssuerOrganization", "bufsiz error have=%d need=%d",
					bufsiz,
					static_cast<int>(strlen(TQSL_API_TO_CERT(cert)->crq->providerName)));
			tQSL_Error = TQSL_BUFFER_ERROR;
			return 1;
		}
		strncpy(buf, TQSL_API_TO_CERT(cert)->crq->providerName, bufsiz);
		return 0;
	}
	item.name_buf = nbuf;
	item.name_buf_size = sizeof nbuf;
	item.value_buf = buf;
	item.value_buf_size = bufsiz;
	if ((iss = X509_get_issuer_name(TQSL_API_TO_CERT(cert)->cert)) == NULL) {
		tqslTrace("tqsl_getCertificateIssuerOrganization", "get_issuer_name err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	return !tqsl_get_name_entry(iss, "organizationName", &item);
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateIssuerOrganizationalUnit(tQSL_Cert cert, char *buf, int bufsiz) {
	char nbuf[40];
	TQSL_X509_NAME_ITEM item;
	X509_NAME *iss;

	tqslTrace("tqsl_getCertificateIssuerOrganizationalUnit", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_getCertificateIssuerOrganizationalUnit", "arg err cert=0x%lx, buf=0x%lx", cert, buf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->keyonly && TQSL_API_TO_CERT(cert)->crq) {
		// Handle the key-only case
		if (bufsiz <= static_cast<int>(strlen(TQSL_API_TO_CERT(cert)->crq->providerUnit))) {
			tqslTrace("tqsl_getCertificateIssuerOrganizationalUnit", "bufsize error have=%d need=%d",
				bufsiz,
				static_cast<int>(strlen(TQSL_API_TO_CERT(cert)->crq->providerUnit)));
			tQSL_Error = TQSL_BUFFER_ERROR;
			return 1;
		}
		strncpy(buf, TQSL_API_TO_CERT(cert)->crq->providerUnit, bufsiz);
		return 0;
	}
	item.name_buf = nbuf;
	item.name_buf_size = sizeof nbuf;
	item.value_buf = buf;
	item.value_buf_size = bufsiz;
	if ((iss = X509_get_issuer_name(TQSL_API_TO_CERT(cert)->cert)) == NULL) {
		tqslTrace("tqsl_getCertificateIssuerOrganizationalUnit", "get_issuer_name err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	return !tqsl_get_name_entry(iss, "organizationalUnitName", &item);
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateQSONotBeforeDate(tQSL_Cert cert, tQSL_Date *date) {
	char datebuf[40];
	int len = (sizeof datebuf) -1;
	tqslTrace("tqsl_getCertificateQSONotBeforeDate", NULL);

	if (tqsl_init())
		return 1;
	if (cert == NULL || date == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_getCertificateQSONotBeforeDate", "arg err cert=0x%lx date=0x%lx", cert, date);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->keyonly && TQSL_API_TO_CERT(cert)->crq) {
		// Handle the key-only case
		*date = TQSL_API_TO_CERT(cert)->crq->qsoNotBefore;
		return 0;
	}
	if (tqsl_get_cert_ext(TQSL_API_TO_CERT(cert)->cert, "QSONotBeforeDate",
		(unsigned char *)datebuf, &len, NULL))
		return 1;
	datebuf[len] = 0;
	return tqsl_initDate(date, const_cast<char *>(datebuf));
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateQSONotAfterDate(tQSL_Cert cert, tQSL_Date *date) {
	char datebuf[40];
	int len = (sizeof datebuf) -1;
	tqslTrace("tqsl_getCertificateQSONotAfterDate", NULL);

	if (tqsl_init())
		return 1;
	if (cert == NULL || date == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_getCertificateQSONotAfterDate", "arg err cert=0x%lx date=0x%lx", cert, date);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->keyonly && TQSL_API_TO_CERT(cert)->crq) {
		// Handle the key-only case
		*date = TQSL_API_TO_CERT(cert)->crq->qsoNotAfter;
		return 0;
	}
	if (tqsl_get_cert_ext(TQSL_API_TO_CERT(cert)->cert, "QSONotAfterDate",
		(unsigned char *)datebuf, &len, NULL))
		return 1;
	datebuf[len] = 0;
	return tqsl_initDate(date, const_cast<char *>(datebuf));
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateNotBeforeDate(tQSL_Cert cert, tQSL_Date *date) {
	const ASN1_TIME *tm;

	tqslTrace("tqsl_getCertificateNotBeforeDate", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || date == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_getCertificateNotBeforeDate", "arg err cert=0x%lx date=0x%lx", cert, date);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->keyonly) {
		tqslTrace("tqsl_getCertificateNotBeforeDate", "Err:cert is keyonly");
		tQSL_Error = TQSL_CERT_KEY_ONLY;
		return 1;
	}
	if ((tm = X509_get_notBefore(TQSL_API_TO_CERT(cert)->cert)) == NULL) {
		tqslTrace("tqsl_getCertificateNotBeforeDate", "get_notBefore err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	return tqsl_get_asn1_date(tm, date);
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateNotAfterDate(tQSL_Cert cert, tQSL_Date *date) {
	const ASN1_TIME *tm;

	if (tqsl_init())
		return 1;
	if (cert == NULL || date == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_getCertificateNotAfterDate", "arg err cert=0x%lx date=0x%lx", cert, date);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->keyonly) {
		tqslTrace("tqsl_getCertificateNotAfterDate", "Err:cert is keyonly");
		tQSL_Error = TQSL_CERT_KEY_ONLY;
		return 1;
	}
	if ((tm = X509_get_notAfter(TQSL_API_TO_CERT(cert)->cert)) == NULL) {
		tqslTrace("tqsl_getCertificateNotAfterDate", "get_notAfter err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	return tqsl_get_asn1_date(tm, date);
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateDXCCEntity(tQSL_Cert cert, int *dxcc) {
	char buf[40];
	int len = sizeof buf;
	tqslTrace("tqsl_getCertificateDXCCEntity", NULL);

	if (tqsl_init())
		return 1;
	if (cert == NULL || dxcc == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_getCertificateDXCCEntity", "arg err cert=0x%lx dxcc=0x%lx", cert, dxcc);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->keyonly && TQSL_API_TO_CERT(cert)->crq) {
		// Handle the key-only case
		*dxcc = TQSL_API_TO_CERT(cert)->crq->dxccEntity;
		return 0;
	}
	if (tqsl_get_cert_ext(TQSL_API_TO_CERT(cert)->cert, "dxccEntity",
		(unsigned char *)buf, &len, NULL)) {
		tqslTrace("tqsl_getCertificateDXCCEntity", "Cert does not have dxcc extension");
		return 1;
	}
	*dxcc = strtol(buf, NULL, 10);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificatePrivateKeyType(tQSL_Cert cert) {
	tqslTrace("tqsl_getCertificatePrivateKeyType", NULL);

	if (tqsl_init())
		return 1;
	if (!tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_getCertificatePrivateKeyType", "arg err, bad cert");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (tqsl_beginSigning(cert, const_cast<char *> (""), 0, 0)) {		// Try to unlock the key using no password
		if (tQSL_Error == TQSL_PASSWORD_ERROR) {
			tqsl_getErrorString();	// Clear the error
			tqslTrace("tqsl_getCertificatePrivateKeyType", "password error - encrypted");
			return TQSL_PK_TYPE_ENC;
		}
		tqslTrace("tqsl_getCertificatePrivateKeyType", "other error");
		return TQSL_PK_TYPE_ERR;
	}
	tqslTrace("tqsl_getCertificatePrivateKeyType", "unencrypted");
	return TQSL_PK_TYPE_UNENC;
}

DLLEXPORT void CALLCONVENTION
tqsl_freeCertificate(tQSL_Cert cert) {
	if (cert == NULL) return;
	tqsl_cert_free(TQSL_API_TO_CERT(cert));
}

DLLEXPORT void CALLCONVENTION
tqsl_freeCertificateList(tQSL_Cert* list, int ncerts) {
	for (int i = 0; i < ncerts; i++)
		if (list[i]) tqsl_cert_free(TQSL_API_TO_CERT(list[i]));
	if (list) free(list);
}

DLLEXPORT int CALLCONVENTION
tqsl_beginSigning(tQSL_Cert cert, char *password, int(*pwcb)(char *, int, void *), void *userdata) {
	tqslTrace("tqsl_beginSigning", NULL);

	if (tqsl_init())
		return 1;
	if (cert == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_beginSigning", "arg err cert=0x%lx", cert);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->key != NULL)
		return 0;
	if (TQSL_API_TO_CERT(cert)->keyonly) {
		if (TQSL_API_TO_CERT(cert)->privkey == 0) {
			tqslTrace("tqsl_beginSigning", "can't sign, keyonly");
			tQSL_Error = TQSL_ARGUMENT_ERROR;
			return 1;
		}
		return tqsl_unlock_key(TQSL_API_TO_CERT(cert)->privkey, &(TQSL_API_TO_CERT(cert)->key),
			password, pwcb, userdata);
	}
	return tqsl_find_matching_key(TQSL_API_TO_CERT(cert)->cert, &(TQSL_API_TO_CERT(cert)->key),
		&(TQSL_API_TO_CERT(cert)->crq), password, pwcb, userdata);
}

DLLEXPORT int CALLCONVENTION
tqsl_getMaxSignatureSize(tQSL_Cert cert, int *sigsize) {
	tqslTrace("tqsl_getMaxSignatureSize", NULL);

	if (tqsl_init())
		return 1;
	if (cert == NULL || sigsize == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_getMaxSignatureSize", "arg err cert=0x%lx, sigsize=0x%lx", cert, sigsize);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->key == NULL) {
		tqslTrace("tqsl_getMaxSignatureSize", "arg err key=null");
		tQSL_Error = TQSL_SIGNINIT_ERROR;
		return 1;
	}
	*sigsize = EVP_PKEY_size(TQSL_API_TO_CERT(cert)->key);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_checkSigningStatus(tQSL_Cert cert) {
	tqslTrace("tqsl_checkSigningStatus", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_checkSigningStatus", "arg err cert=0x%lx", cert);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->key == NULL) {
		tqslTrace("tqsl_checkSigningStatus", "arg err no key");
		tQSL_Error = TQSL_SIGNINIT_ERROR;
		return 1;
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_signDataBlock(tQSL_Cert cert, const unsigned char *data, int datalen, unsigned char *sig, int *siglen) {
	tqslTrace("tqsl_signDataBlock", NULL);
	if (tqsl_init())
		return 1;
	if (cert == NULL || data == NULL || sig == NULL || siglen == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_signDataBlock", "arg error cert=0x%lx data=0x%lx sig=0x%lx siglen=0x%lx", cert, data, sig, siglen);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	EVP_MD_CTX *ctx = EVP_MD_CTX_create();
	if (ctx == NULL)
		return 1;
	unsigned int slen = *siglen;

	if (TQSL_API_TO_CERT(cert)->key == NULL) {
		tqslTrace("tqsl_signDataBlock", "can't sign, no key");
		tQSL_Error = TQSL_SIGNINIT_ERROR;
		if (ctx)
			EVP_MD_CTX_destroy(ctx);
		return 1;
	}
	EVP_SignInit(ctx, EVP_sha1());
	EVP_SignUpdate(ctx, data, datalen);
	if (!EVP_SignFinal(ctx, sig, &slen, TQSL_API_TO_CERT(cert)->key)) {
		tqslTrace("tqsl_signDataBlock", "signing failed %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		if (ctx)
			EVP_MD_CTX_destroy(ctx);
		return 1;
	}
	*siglen = slen;
	if (ctx)
		EVP_MD_CTX_destroy(ctx);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_verifyDataBlock(tQSL_Cert cert, const unsigned char *data, int datalen, unsigned char *sig, int siglen) {
	EVP_MD_CTX *ctx = EVP_MD_CTX_create();

	unsigned int slen = siglen;
	tqslTrace("tqsl_verifyDataBlock", NULL);

	if (ctx == NULL)
		return 1;
	if (tqsl_init())
		return 1;
	if (cert == NULL || data == NULL || sig == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_verifyDataBlock", "arg error cert=0x%lx data=0x%lx sig=0x%lx", cert, data, sig);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		if (ctx)
			EVP_MD_CTX_destroy(ctx);
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->key == NULL) {
		tqslTrace("tqsl_verifyDataBlock", "no key");
		tQSL_Error = TQSL_SIGNINIT_ERROR;
		if (ctx)
			EVP_MD_CTX_destroy(ctx);
		return 1;
	}
	EVP_VerifyInit(ctx, EVP_sha1());
	EVP_VerifyUpdate(ctx, data, datalen);
	if (EVP_VerifyFinal(ctx, sig, slen, TQSL_API_TO_CERT(cert)->key) <= 0) {
		tqslTrace("tqsl_verifyDataBlock", "verify fail %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		if (ctx)
			EVP_MD_CTX_destroy(ctx);
		return 1;
	}
	if (ctx)
		EVP_MD_CTX_destroy(ctx);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_endSigning(tQSL_Cert cert) {
	tqslTrace("tqsl_endSigning", NULL);

	if (tqsl_init())
		return 1;
	if (cert == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_endSigning", "arg err cert=0x%lx", cert);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->key != NULL) {
		EVP_PKEY_free(TQSL_API_TO_CERT(cert)->key);
		TQSL_API_TO_CERT(cert)->key = NULL;
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateRequestAddress1(tQSL_Cert cert, char *buf, int bufsiz) {
	tqslTrace("tqsl_getCertificateRequestAddress1", NULL);

	if (tqsl_check_crq_field(cert, buf, bufsiz)) {
		tqslTrace("tqsl_getCertificateRequestAddress1", "check fail");
		return 1;
	}
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->address1) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->address1),
		bufsiz);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateRequestAddress2(tQSL_Cert cert, char *buf, int bufsiz) {
	tqslTrace("tqsl_getCertificateRequestAddress2", NULL);

	if (tqsl_check_crq_field(cert, buf, bufsiz)) {
		tqslTrace("tqsl_getCertificateRequestAddress2", "check fail");
		return 1;
	}
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->address2) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->address2),
		bufsiz);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateRequestCity(tQSL_Cert cert, char *buf, int bufsiz) {
	tqslTrace("tqsl_getCertificateRequestCity", NULL);

	if (tqsl_check_crq_field(cert, buf, bufsiz)) {
		tqslTrace("tqsl_getCertificateRequestCity", "check fail");
		return 1;
	}
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->city) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->city),
		bufsiz);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateRequestState(tQSL_Cert cert, char *buf, int bufsiz) {
	tqslTrace("tqsl_getCertificateRequestState", NULL);
	if (tqsl_check_crq_field(cert, buf, bufsiz)) {
		tqslTrace("tqsl_getCertificateRequestState", "check fail");
		return 1;
	}
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->state) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->state),
		bufsiz);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateRequestPostalCode(tQSL_Cert cert, char *buf, int bufsiz) {
	tqslTrace("tqsl_getCertificateRequestPostalCode", NULL);
	if (tqsl_check_crq_field(cert, buf, bufsiz)) {
		tqslTrace("tqsl_getCertificateRequestPostalCode", "check fail");
		return 1;
	}
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->postalCode) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->postalCode),
		bufsiz);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateRequestCountry(tQSL_Cert cert, char *buf, int bufsiz) {
	tqslTrace("tqsl_getCertificateRequestCountry", NULL);
	if (tqsl_check_crq_field(cert, buf, bufsiz)) {
		tqslTrace("tqsl_getCertificateRequestCountry", "check fail");
		return 1;
	}
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->country) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->country),
		bufsiz);
	return 0;
}

static int
tqsl_add_bag_attribute(PKCS12_SAFEBAG *bag, const char *oidname, const string& value) {
	int nid;
	nid = OBJ_txt2nid(const_cast<char *>(oidname));
	if (nid == NID_undef) {
		tqslTrace("tqsl_add_bag_attribute", "OBJ_txt2nid err %s", tqsl_openssl_error());
		return 1;
	}
	unsigned char *uni;
	int unilen;
	if (asc2uni(value.c_str(), value.length(), &uni, &unilen)) {
		ASN1_TYPE *val;
		X509_ATTRIBUTE *attrib;
		if (!uni[unilen - 1] && !uni[unilen - 2])
			unilen -= 2;
		if ((val = ASN1_TYPE_new()) != 0) {
			ASN1_TYPE_set(val, V_ASN1_BMPSTRING, uni);
			if ((attrib = X509_ATTRIBUTE_new()) != 0) {
				X509_ATTRIBUTE_set1_object(attrib, OBJ_nid2obj(nid));
				if ((X509_ATTRIBUTE_set1_data(attrib, V_ASN1_BMPSTRING, uni, unilen)) != 0) {
#if (OPENSSL_VERSION_NUMBER & 0xfffff000) == 0x00906000
					attrib->set = 1;
#else
  #if OPENSSL_VERSION_NUMBER < 0x10100000L
    #if (OPENSSL_VERSION_NUMBER & 0xfffff000) >= 0x00907000
					attrib->single = 0;
    #else
        #error "Unexpected OpenSSL version; check X509_ATTRIBUTE struct compatibility"
    #endif
  #endif
#endif
#if OPENSSL_VERSION_NUMBER >= 0x10100000L && !defined(LIBRESSL_VERSION_NUMBER)
					STACK_OF(X509_ATTRIBUTE) *sk;
					sk = (STACK_OF(X509_ATTRIBUTE)*)PKCS12_SAFEBAG_get0_attrs(bag);
					if (sk) {
						sk_X509_ATTRIBUTE_push(sk, attrib);
#else
					if (bag->attrib) {
						sk_X509_ATTRIBUTE_push(bag->attrib, attrib);
#endif
//cerr << "Added " << oidname << endl;
					} else {
						tqslTrace("tqsl_add_bag_attribute", "no attrib");
						return 1;
					}
				} else {
					tqslTrace("tqsl_add_bag_attribute", "no value set");
					return 1;
				}
			} else {
				tqslTrace("tqsl_add_bag_attribute", "attrib create err %s", tqsl_openssl_error());
				return 1;
			}
		} else {
			tqslTrace("tqsl_add_bag_attribute", "bmp->data empty");
			return 1;
		}
	} else {  // asc2uni ok
		tqslTrace("tqsl_add_bag_attribute", "asc2uni err %s", tqsl_openssl_error());
		return 1;
	}
	return 0;
}

static int
tqsl_exportPKCS12(tQSL_Cert cert, bool returnB64, const char *filename, char *base64, int b64len, const char *p12password) {
	STACK_OF(X509) *root_sk = 0, *ca_sk = 0, *chain = 0;
	const char *cp;
	char rootpath[TQSL_MAX_PATH_LEN], capath[TQSL_MAX_PATH_LEN];
	char buf[256];
	unsigned char keyid[EVP_MAX_MD_SIZE];
	unsigned int keyidlen = 0;
	STACK_OF(PKCS12_SAFEBAG) *bags = 0;
	PKCS12_SAFEBAG *bag = 0;
	STACK_OF(PKCS7) *safes = 0;
	PKCS7 *authsafe = 0;
	int cert_pbe = NID_pbe_WithSHA1And40BitRC2_CBC;
	int key_pbe = NID_pbe_WithSHA1And3_Key_TripleDES_CBC;
	PKCS8_PRIV_KEY_INFO *p8 = 0;
	PKCS12 *p12 = 0;
	BIO *out = 0, *b64 = 0;
	string callSign, issuerOrganization, issuerOrganizationalUnit;
	tQSL_Date date;
	string QSONotBeforeDate, QSONotAfterDate, dxccEntity, Email,
		Address1, Address2, City, State, Postal, Country;
	int dxcc = 0;
	int rval = 1;

	tqslTrace("tqsl_exportPKCS12", NULL);
	if (cert == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_exportPKCS12", "arg error cert=0x%lx", cert);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if ((returnB64 && base64 == NULL) || (!returnB64 && filename == NULL)) {
		tqslTrace("tqsl_exportPKCS12", "arg error returnB64=%d base64=0x%lx filename=0x%lx", returnB64, base64, filename);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}

	/* Get parameters for key bag attributes */
	if (tqsl_getCertificateCallSign(cert, buf, sizeof buf)) {
		tqslTrace("tqsl_exportPKCS12", "get callsign err %d", tQSL_Error);
		return 1;
	}
	callSign = buf;
	if (tqsl_getCertificateIssuerOrganization(cert, buf, sizeof buf)) {
		tqslTrace("tqsl_exportPKCS12", "get org err %d", tQSL_Error);
		return 1;
	}
	issuerOrganization = buf;
	if (tqsl_getCertificateIssuerOrganizationalUnit(cert, buf, sizeof buf)) {
		tqslTrace("tqsl_exportPKCS12", "get ou err %d", tQSL_Error);
		return 1;
	}
	issuerOrganizationalUnit = buf;
	if (!TQSL_API_TO_CERT(cert)->keyonly) {
		if (tqsl_getCertificateEmailAddress(cert, buf, sizeof buf)) {
			tqslTrace("tqsl_exportPKCS12", "get email err %d", tQSL_Error);
			return 1;
		}
		Email = buf;
		if (tqsl_getCertificateRequestAddress1(cert, buf, sizeof buf)) {
			tqslTrace("tqsl_exportPKCS12", "get addr1 err %d", tQSL_Error);
			return 1;
		}
		Address1 = buf;
		if (tqsl_getCertificateRequestAddress2(cert, buf, sizeof buf)) {
			tqslTrace("tqsl_exportPKCS12", "get addr2 err %d", tQSL_Error);
			return 1;
		}
		Address2 = buf;
		if (tqsl_getCertificateRequestCity(cert, buf, sizeof buf)) {
			tqslTrace("tqsl_exportPKCS12", "get city err %d", tQSL_Error);
			return 1;
		}
		City = buf;
		if (tqsl_getCertificateRequestState(cert, buf, sizeof buf)) {
			tqslTrace("tqsl_exportPKCS12", "get state err %d", tQSL_Error);
			return 1;
		}
		State = buf;
		if (tqsl_getCertificateRequestPostalCode(cert, buf, sizeof buf)) {
			tqslTrace("tqsl_exportPKCS12", "get postal err %d", tQSL_Error);
			return 1;
		}
		Postal = buf;
		if (tqsl_getCertificateRequestCountry(cert, buf, sizeof buf)) {
			tqslTrace("tqsl_exportPKCS12", "get country err %d", tQSL_Error);
			return 1;
		}
		Country = buf;
	}
	if (tqsl_getCertificateQSONotBeforeDate(cert, &date)) {
		tqslTrace("tqsl_exportPKCS12", "get qso not before err %d", tQSL_Error);
		return 1;
	}
	if (!tqsl_convertDateToText(&date, buf, sizeof buf)) {
		tqslTrace("tqsl_exportPKCS12", "qso not before err %d", tQSL_Error);
		return 1;
	}
	QSONotBeforeDate = buf;
	if (tqsl_getCertificateQSONotAfterDate(cert, &date)) {
		tqslTrace("tqsl_exportPKCS12", "get qso not after err %d", tQSL_Error);
		return 1;
	}
	if (!tqsl_isDateNull(&date)) {
		if (!tqsl_convertDateToText(&date, buf, sizeof buf)) {
			tqslTrace("tqsl_exportPKCS12", "qso not before err %d", tQSL_Error);
			return 1;
		}
		QSONotAfterDate = buf;
	}
	if (tqsl_getCertificateDXCCEntity(cert, &dxcc)) {
		tqslTrace("tqsl_exportPKCS12", "get entity err %d", tQSL_Error);
		return 1;
	}
	snprintf(buf, sizeof buf, "%d", dxcc);
	dxccEntity = buf;

	if (TQSL_API_TO_CERT(cert)->key == NULL) {
		tqslTrace("tqsl_exportPKCS12", "key is null");
		tQSL_Error = TQSL_SIGNINIT_ERROR;
		return 1;
	}

	if (!TQSL_API_TO_CERT(cert)->keyonly) {
		tqslTrace("tqsl_exportPKCS12", "keyonly cert");
		/* Generate local key ID to tie key to cert */
		X509_digest(TQSL_API_TO_CERT(cert)->cert, EVP_sha1(), keyid, &keyidlen);

		/* Check the chain of authority back to a trusted root */
		tqsl_make_cert_path("root", rootpath, sizeof rootpath);
		if ((root_sk = tqsl_ssl_load_certs_from_file(rootpath)) == NULL) {
			if (!tqsl_ssl_error_is_nofile()) {
				tqslTrace("tqsl_exportPKCS12", "can't find certs");
				goto p12_end;
			}
		}
		tqsl_make_cert_path("authorities", capath, sizeof capath);
		if ((ca_sk = tqsl_ssl_load_certs_from_file(capath)) == NULL) {
			if (!tqsl_ssl_error_is_nofile()) {
				tqslTrace("tqsl_exportPKCS12", "can't find certs");
				goto p12_end;
			}
		}

		/* tqsl_ssl_verify_cert will collect the certificates in the chain, back to the
		 * root certificate, verify them and return a stack containing copies of just
		 * those certificates (including the user certificate).
		 */

		cp = tqsl_ssl_verify_cert(TQSL_API_TO_CERT(cert)->cert, ca_sk, root_sk, 0, &tqsl_expired_is_ok, &chain);
		if (cp) {
			if (chain)
				sk_X509_free(chain);
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, cp, sizeof tQSL_CustomError);
			tqslTrace("tqsl_exportPKCS12", "verify fail: %s", cp);
			return 1;
		}
	} // !keyonly

	tQSL_Error = TQSL_OPENSSL_ERROR;	// Assume error

	/* Open the output file */
	if (!returnB64) {
		out = BIO_new_file(filename, "wb");
	} else {
		b64 = BIO_new(BIO_f_base64());
		out = BIO_new(BIO_s_mem());
		out = BIO_push(b64, out);
	}
	if (!out) {
		tqslTrace("tqsl_exportPKCS12", "BIO_new err %s", tqsl_openssl_error());
		goto p12_end;
	}

	safes = sk_PKCS7_new_null();

	if (!TQSL_API_TO_CERT(cert)->keyonly) {
		/* Create a safebag stack and fill it with the needed certs */
		bags = sk_PKCS12_SAFEBAG_new_null();
		for (int i = 0; i < sk_X509_num(chain); i++) {
			X509 *x = sk_X509_value(chain, i);
#if (OPENSSL_VERSION_NUMBER & 0xfffff000) == 0x00906000
			bag = PKCS12_pack_safebag(reinterpret_cast<char *>(x), (int (*)())i2d_X509, NID_x509Certificate, NID_certBag);
#else
			bag = PKCS12_x5092certbag(x);
#endif
			if (!bag) {
				tqslTrace("tqsl_exportPKCS12", "Error creating bag: %s", tqsl_openssl_error());
				goto p12_end;
			}
			if (x == TQSL_API_TO_CERT(cert)->cert) {
				PKCS12_add_friendlyname(bag, "TrustedQSL user certificate", -1);
				PKCS12_add_localkeyid(bag, keyid, keyidlen);
			}
			sk_PKCS12_SAFEBAG_push(bags, bag);
		}

		/* Convert stack of safebags into an authsafe */
		authsafe = PKCS12_pack_p7encdata(cert_pbe, p12password, -1, 0, 0, PKCS12_DEFAULT_ITER, bags);
		if (!authsafe) {
			tqslTrace("tqsl_exportPKCS12", "Error creating authsafe: %s", tqsl_openssl_error());
			goto p12_end;
		}
		sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
		bags = 0;

		/* Add to stack of authsafes */
		sk_PKCS7_push(safes, authsafe);
	} // !keyonly

	/* Make a shrouded key bag */
	p8 = EVP_PKEY2PKCS8(TQSL_API_TO_CERT(cert)->key);
	if (!p8) {
		tqslTrace("tqsl_exportPKCS12", "Error creating p8 container: %s", tqsl_openssl_error());
		goto p12_end;
	}
	bag = PKCS12_MAKE_SHKEYBAG(key_pbe, p12password, -1, 0, 0, PKCS12_DEFAULT_ITER, p8);
	if (!bag) {
		tqslTrace("tqsl_exportPKCS12", "Error creating p8 keybag: %s", tqsl_openssl_error());
		goto p12_end;
	}
	PKCS8_PRIV_KEY_INFO_free(p8);
	p8 = NULL;
	PKCS12_add_friendlyname(bag, "TrustedQSL user certificate", -1);
	if (!TQSL_API_TO_CERT(cert)->keyonly)
		PKCS12_add_localkeyid(bag, keyid, keyidlen);

	/* Add the attributes to the private key bag */
	tqsl_add_bag_attribute(bag, "AROcallsign", callSign);
	tqsl_add_bag_attribute(bag, "QSONotBeforeDate", QSONotBeforeDate);
	if (QSONotAfterDate != "")
		tqsl_add_bag_attribute(bag, "QSONotAfterDate", QSONotAfterDate);
	tqsl_add_bag_attribute(bag, "tqslCRQIssuerOrganization", issuerOrganization);
	tqsl_add_bag_attribute(bag, "tqslCRQIssuerOrganizationalUnit", issuerOrganizationalUnit);
	tqsl_add_bag_attribute(bag, "dxccEntity", dxccEntity);
	tqsl_add_bag_attribute(bag, "tqslCRQEmail", Email);
	tqsl_add_bag_attribute(bag, "tqslCRQAddress1", Address1);
	tqsl_add_bag_attribute(bag, "tqslCRQAddress2", Address2);
	tqsl_add_bag_attribute(bag, "tqslCRQCity", City);
	tqsl_add_bag_attribute(bag, "tqslCRQState", State);
	tqsl_add_bag_attribute(bag, "tqslCRQPostal", Postal);
	tqsl_add_bag_attribute(bag, "tqslCRQCountry", Country);

	bags = sk_PKCS12_SAFEBAG_new_null();
	if (!bags) {
		tqslTrace("tqsl_exportPKCS12", "Error creating safebag: %s", tqsl_openssl_error());
		goto p12_end;
	}
	sk_PKCS12_SAFEBAG_push(bags, bag);

	/* Turn shrouded key bag into unencrypted safe bag and add to safes stack */
	authsafe = PKCS12_pack_p7data(bags);
	sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
	bags = NULL;
	sk_PKCS7_push(safes, authsafe);

	/* Form into PKCS12 data */
	p12 = PKCS12_init(NID_pkcs7_data);
	M_PKCS12_pack_authsafes(p12, safes);

	sk_PKCS7_pop_free(safes, PKCS7_free);
	safes = NULL;
	PKCS12_set_mac(p12, p12password, -1, 0, 0, PKCS12_DEFAULT_ITER, 0);

	/* Write the PKCS12 data */

	i2d_PKCS12_bio(out, p12);
	if (BIO_flush(out) != 1) {
		rval = 1;
		tqslTrace("tqsl_exportPKCS12", "Error writing pkcs12: %s", tqsl_openssl_error());
		goto p12_end;
	}

	if (returnB64) {
		char *encoded;
		int len;
		len = BIO_get_mem_data(out, &encoded);
		encoded[len - 1] = '\0';
		strncpy(base64, encoded, b64len);
	}

	rval = 0;
	tQSL_Error = TQSL_NO_ERROR;
 p12_end:
	if (out) {
		BIO_free(out);
		if (rval && !returnB64) {
#ifdef _WIN32
			wchar_t* wfilename = utf8_to_wchar(filename);
			_wunlink(wfilename);
			free_wchar(wfilename);
#else
			unlink(filename);
#endif
		}
	}
	if (chain)
		sk_X509_free(chain);
	if (root_sk)
		sk_X509_free(root_sk);
	if (ca_sk)
		sk_X509_free(ca_sk);
	if (bags)
		sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
	if (safes)
		sk_PKCS7_pop_free(safes, PKCS7_free);
	if (p8)
		PKCS8_PRIV_KEY_INFO_free(p8);
	return rval;
}

DLLEXPORT int CALLCONVENTION
tqsl_exportPKCS12File(tQSL_Cert cert, const char *filename, const char *p12password) {
	tqslTrace("tqsl_exportPKCS12File", NULL);
	return tqsl_exportPKCS12(cert, false, filename, NULL, 0, p12password);
}

DLLEXPORT int CALLCONVENTION
tqsl_exportPKCS12Base64(tQSL_Cert cert, char *base64, int b64len, const char *p12password) {
	tqslTrace("tqsl_exportPKCS12Base64", NULL);
	return tqsl_exportPKCS12(cert, true, NULL, base64, b64len, p12password);
}

static string
tqsl_asn1_octet_string_to_hex(ASN1_OCTET_STRING *os) {
	string str;

	for (int k = 0; k < os->length; k++) {
		char hex[3] = "  ";
		hex[0] = ((os->data[k] >> 4) & 0xf) + '0';
		if (hex[0] > '9') hex[0] += 'A' - '9' - 1;
		hex[1] = (os->data[k] & 0xf) + '0';
		if (hex[1] > '9') hex[1] += 'A' - '9' - 1;
		if (str.size()) str += " ";
		str += hex;
	}
	return str;
}

struct tqsl_imported_cert {
	string pem;
	string keyid;
	string callsign;
};

static int
tqsl_get_bag_attribute(PKCS12_SAFEBAG *bag, const char *oidname, string& str) {
	const ASN1_TYPE *attr;

	str = "";
	if ((attr = PKCS12_get_attr(bag, OBJ_txt2nid(const_cast<char *>(oidname)))) != 0) {
		if (attr->type != V_ASN1_BMPSTRING) {
			tQSL_Error = TQSL_CERT_TYPE_ERROR;
			tqslTrace("tqsl_get_bag_attribute", "cert type error oid %s", oidname);
			return 1;
		}
		char *c = uni2asc(attr->value.bmpstring->data, attr->value.bmpstring->length);
		str = c;
		OPENSSL_free(c);
	}
	return 0;
}

static int
tqsl_importPKCS12(bool importB64, const char *filename, const char *base64, const char *p12password, const char *password,
	int (*pwcb)(char *, int, void *), int(*cb)(int, const char *, void *), void *userdata) {
	PKCS12 *p12 = 0;
	PKCS12_SAFEBAG *bag;
	PKCS8_PRIV_KEY_INFO *p8 = 0;
	EVP_PKEY *pkey = 0;
	BIO *in = 0, *bio = 0 , *b64 = 0;
	STACK_OF(PKCS7) *safes = 0;
	STACK_OF(PKCS12_SAFEBAG) *bags = 0;
	PKCS7 *p7;
	X509 *x;
	BASIC_CONSTRAINTS *bs = 0;
	ASN1_OBJECT *callobj = 0, *obj = 0;
	const ASN1_TYPE *attr = 0;
	const EVP_CIPHER *cipher;
	unsigned char *cp;
	int i, j, bagnid, len;
	vector<tqsl_imported_cert> rootcerts;
	vector<tqsl_imported_cert> cacerts;
	vector<tqsl_imported_cert> usercerts;
	vector<tqsl_imported_cert> *certlist;
	vector<tqsl_imported_cert>::iterator it;
	bool is_cacert;
	string public_key, private_key, private_keyid, key_callsign, str;
	map<string, string> key_attr;
	map<string, string> newrecord;
	map<string, string>::iterator mit;
	char path[TQSL_MAX_PATH_LEN], pw[256];
	int rval = 1;

	tqslTrace("tqsl_importPKCS12", NULL);

	if (tqsl_init())
		return 1;
	if ((!importB64 && filename == NULL) || (importB64 && base64 == NULL)) {
		tqslTrace("tqsl_importPKCS12", "arg error importB64=%d filename=0x%lx base64=0x%lx", importB64, filename, base64);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}

	tQSL_ImportCall[0] = '\0';
	tQSL_ImportSerial = 0;
	ImportCall[0] = '\0';
	tQSL_Error = TQSL_OPENSSL_ERROR;

	/* Read in the PKCS#12 file */
	if (importB64) {
		b64 = BIO_new(BIO_f_base64());
		in = BIO_new_mem_buf(const_cast<char *>(base64), strlen(base64));
		in = BIO_push(b64, in);
	} else {
		in = BIO_new_file(filename, "rb");
	}
	if (in == 0) {
		tqslTrace("tqsl_importPKCS12", "Couldn't create bio: %s", tqsl_openssl_error());
		goto imp_end;
	}
	if ((p12 = d2i_PKCS12_bio(in, 0)) == 0) {
		tqslTrace("tqsl_importPKCS12", "Couldn't read pkcs12: %s", tqsl_openssl_error());
		goto imp_end;
	}
	BIO_free(in);
	in = 0;

	/* Verify MAC */
	if (!PKCS12_verify_mac(p12, p12password, -1)) {
		tqslTrace("tqsl_importPKCS12", "Mac doesn't verify");
		tQSL_Error = TQSL_PASSWORD_ERROR;
		goto imp_end;
	}
	/* Loop through the authsafes */
	if ((safes = M_PKCS12_unpack_authsafes(p12)) == 0) {
		tqslTrace("tqsl_importPKCS12", "Can't unpack authsafe: %s", tqsl_openssl_error());
		goto imp_end;
	}
	callobj = OBJ_txt2obj("AROcallsign", 0);
	for (i = 0; i < sk_PKCS7_num(safes); i++) {
		tqsl_imported_cert imported_cert;
		p7 = sk_PKCS7_value(safes, i);
		bagnid = OBJ_obj2nid(p7->type);
		if (bagnid == NID_pkcs7_data) {
			bags = PKCS12_unpack_p7data(p7);
		} else if (bagnid == NID_pkcs7_encrypted) {
			bags = PKCS12_unpack_p7encdata(p7, p12password, strlen(p12password));
		} else {
			continue;	// Not something we understand
		}
		if (!bags) {
			tQSL_Error = TQSL_PKCS12_ERROR;
			tqslTrace("tqsl_importPKCS12", "bags empty: %s", tqsl_openssl_error());
			goto imp_end;
		}
		/* Loop through safebags */
		for (j = 0; j < sk_PKCS12_SAFEBAG_num(bags); j++) {
			tqsl_imported_cert imported_cert;
			bag = sk_PKCS12_SAFEBAG_value(bags, j);
			switch (M_PKCS12_bag_type(bag)) {
				case NID_certBag:
					if (M_PKCS12_cert_bag_type(bag) != NID_x509Certificate)
						break;	// Can't handle anything else
					if ((x = M_PKCS12_certbag2x509(bag)) == 0) {
						tqslTrace("tqsl_importPKCS12", "bag2x509: %s", tqsl_openssl_error());
						goto imp_end;
					}
					if ((bio = BIO_new(BIO_s_mem())) == NULL) {
						tqslTrace("tqsl_importPKCS12", "bio_new: %s", tqsl_openssl_error());
						goto imp_end;
					}
					if (!PEM_write_bio_X509(bio, x)) {
						tqslTrace("tqsl_importPKCS12", "write_bio: %s", tqsl_openssl_error());
						goto imp_end;
					}
					len = static_cast<int>(BIO_get_mem_data(bio, &cp));
					imported_cert.pem = string((const char *)cp, len);
					if ((attr = PKCS12_get_attr(bag, NID_localKeyID)) != 0) {
						if (attr->type != V_ASN1_OCTET_STRING) {
							tqslTrace("tqsl_importPKCS12", "Cert type mismatch");
							tQSL_Error = TQSL_CERT_TYPE_ERROR;
							goto imp_end;
						}
						imported_cert.keyid = tqsl_asn1_octet_string_to_hex(attr->value.octet_string);
					}
					BIO_free(bio);
					bio = 0;
					is_cacert = false;
					if ((bs = reinterpret_cast<BASIC_CONSTRAINTS *>(X509_get_ext_d2i(x, NID_basic_constraints, 0, 0))) != 0) {
						if (bs->ca)
							is_cacert = true;
						BASIC_CONSTRAINTS_free(bs);
						bs = 0;
					}
					certlist = &usercerts;
					if (is_cacert) {
						if (X509_check_issued(x, x) == X509_V_OK)	// Self signed must be trusted
							certlist = &rootcerts;
						else
							certlist = &cacerts;
					} else {
						/* Make sure the cert is TQSL compatible */
						TQSL_X509_NAME_ITEM item;
						char nbuf[40];
						char callbuf[256];
						item.name_buf = nbuf;
						item.name_buf_size = sizeof nbuf;
						item.value_buf = callbuf;
						item.value_buf_size = sizeof callbuf;
						if (!tqsl_cert_get_subject_name_entry(x, "AROcallsign", &item)) {
							tqslTrace("tqsl_importPKCS12", "Cert type mismatch");
							tQSL_Error = TQSL_CERT_TYPE_ERROR;
							goto imp_end;
						}
						imported_cert.callsign = callbuf;
					}
					(*certlist).push_back(imported_cert);
					break;
				case NID_pkcs8ShroudedKeyBag:
					if ((attr = PKCS12_get_attr(bag, NID_localKeyID)) != 0) {
						if (attr->type != V_ASN1_OCTET_STRING) {
							tQSL_Error = TQSL_CERT_TYPE_ERROR;
							tqslTrace("tqsl_importPKCS12", "Cert type mismatch");
							goto imp_end;
						}
						private_keyid = tqsl_asn1_octet_string_to_hex(attr->value.octet_string);
					}
					if (tqsl_get_bag_attribute(bag, "AROcallsign", key_callsign)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no callsign");
						goto imp_end;
					}
					if (tqsl_get_bag_attribute(bag, "dxccEntity", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no dxcc");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_DXCC_ENTITY"] = str;
					if (tqsl_get_bag_attribute(bag, "QSONotBeforeDate", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no qsonotbefore");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_QSO_NOT_BEFORE"] = str;
					if (tqsl_get_bag_attribute(bag, "QSONotAfterDate", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no qsonotafter");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_QSO_NOT_AFTER"] = str;
					if (tqsl_get_bag_attribute(bag, "tqslCRQIssuerOrganization", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no org");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_PROVIDER"] = str;
					if (tqsl_get_bag_attribute(bag, "tqslCRQIssuerOrganizationalUnit", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no ou");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_PROVIDER_UNIT"] = str;
					if (tqsl_get_bag_attribute(bag, "tqslCRQEmail", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no email");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_EMAIL"] = str;
					if (tqsl_get_bag_attribute(bag, "tqslCRQAddress1", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no addr1");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_ADDRESS1"] = str;
					if (tqsl_get_bag_attribute(bag, "tqslCRQAddress2", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no addr2");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_ADDRESS2"] = str;
					if (tqsl_get_bag_attribute(bag, "tqslCRQCity", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no city");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_CITY"] = str;
					if (tqsl_get_bag_attribute(bag, "tqslCRQState", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no state");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_STATE"] = str;
					if (tqsl_get_bag_attribute(bag, "tqslCRQPostal", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no postal");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_POSTAL"] = str;
					if (tqsl_get_bag_attribute(bag, "tqslCRQCountry", str)) {
						tqslTrace("tqsl_importPKCS12", "Cert type mismatch - no country");
						goto imp_end;
					}
					if (str != "")
						key_attr["TQSL_CRQ_COUNTRY"] = str;
					if ((p8 = M_PKCS12_decrypt_skey(bag, p12password, strlen(p12password))) == 0) {
						tqslTrace("tqsl_importPKCS12", "password error");
						goto imp_end;
					}
					if ((pkey = EVP_PKCS82PKEY(p8)) == 0) {
						tqslTrace("tqsl_importPKCS12", "pkey error");
						goto imp_end;
					}
					if ((bio = BIO_new(BIO_s_mem())) == NULL) {
						tqslTrace("tqsl_importPKCS12", "bio_new error %s", tqsl_openssl_error());
						goto imp_end;
					}
					if (password == 0) {
						if (pwcb) {
							if ((*pwcb)(pw, sizeof pw -1, userdata)) {
								tqslTrace("tqsl_importPKCS12", "operator aborted at password prompt");
								tQSL_Error = TQSL_OPERATOR_ABORT;
								goto imp_end;
							}
							password = pw;
						} else {
							password = NULL;
						}
					}
					if (password && *password != '\0') {
						cipher = EVP_des_ede3_cbc();
						len = strlen(password);
					} else {
						cipher = 0;
						len = 0;
					}
					if (!PEM_write_bio_PrivateKey(bio, pkey, cipher, (unsigned char *)password, len, 0, 0)) {
						tqslTrace("tqsl_importPKCS12", "writing bio err: %s", tqsl_openssl_error());
						goto imp_end;
					}
					len = static_cast<int>(BIO_get_mem_data(bio, &cp));
					private_key = string((const char *)cp, len);
					BIO_free(bio);
					if ((bio = BIO_new(BIO_s_mem())) == NULL) {
						tqslTrace("tqsl_importPKCS12", "new bio err: %s", tqsl_openssl_error());
						goto imp_end;
					}
					if (!PEM_write_bio_PUBKEY(bio, pkey)) {
						tqslTrace("tqsl_importPKCS12", "write pubkey bio err: %s", tqsl_openssl_error());
						goto imp_end;
					}
					len = static_cast<int>(BIO_get_mem_data(bio, &cp));
					public_key = string((const char *)cp, len);
					BIO_free(bio);
					bio = 0;
					EVP_PKEY_free(pkey);
					pkey = 0;
					PKCS8_PRIV_KEY_INFO_free(p8);
					p8 = 0;
					break;
				case NID_keyBag:
					tqslTrace("tqsl_importPKCS12", "cert type err: NID_keyBag");
					tQSL_Error = TQSL_CERT_TYPE_ERROR;
					goto imp_end;
			} // bag type switch
		} // safebags loop
		sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
		bags = 0;
	}
	sk_PKCS7_pop_free(safes, PKCS7_free);
	safes = 0;

	/* Now we have the certificates and key pair, so add them to the local store */

	tqslTrace("tqsl_importPKCS12", "have keys");
	if (key_callsign == "") {
		/* Need to get call sign from cert. The tQSL_exportKeys function puts the
		 * call sign in a safebag attribute that should have been read already,
		 * but if some other software created the PKCS#12 file that may not have
		 * happened. There should, however, be a localKeyID attribute that matches
		 * the key to the certificate. If not, it's an error.
		 */
		if (private_keyid == "") {	// No key ID, can't find cert
			tqslTrace("tqsl_importPKCS12", "no callsign attribute");
			tQSL_Error = TQSL_CERT_TYPE_ERROR;
			goto imp_end;
		}
		for (it = usercerts.begin(); it != usercerts.end(); it++) {
			if ((*it).keyid == private_keyid) {
				key_callsign = (*it).callsign;
				break;
			}
		}
		if (key_callsign == "") {	// Can't find cert or callsign
			tqslTrace("tqsl_importPKCS12", "can't find cert or callsign");
			tQSL_Error = TQSL_CERT_TYPE_ERROR;
			goto imp_end;
		}
	}

	if (!tqsl_make_key_path(key_callsign.c_str(), path, sizeof path)) {
		tqslTrace("tqsl_importPKCS12", "keypath error %d", tQSL_Error);
		goto imp_end;
	}
	newrecord["PUBLIC_KEY"] = public_key;
	newrecord["PRIVATE_KEY"] = private_key;
	newrecord["CALLSIGN"] = key_callsign;
	for (mit = key_attr.begin(); mit != key_attr.end(); mit++)
		newrecord[mit->first] = mit->second;

	if (tqsl_replace_key(key_callsign.c_str(), path, newrecord, cb, userdata)) {
		tqslTrace("tqsl_importPKCS12", "replace key error %d", tQSL_Error);
		goto imp_end;
	}
	for (it = rootcerts.begin(); it != rootcerts.end(); it++) {
		if (tqsl_import_cert(it->pem.c_str(), tqsllib::ROOTCERT, cb, userdata) && tQSL_Error != TQSL_CERT_ERROR) {
			tqslTrace("tqsl_importPKCS12", "import root cert error %d", tQSL_Error);
			goto imp_end;
		}
	}
	for (it = cacerts.begin(); it != cacerts.end(); it++) {
		if (tqsl_import_cert(it->pem.c_str(), tqsllib::CACERT, cb, userdata) && tQSL_Error != TQSL_CERT_ERROR) {
			tqslTrace("tqsl_importPKCS12", "import ca cert error %d", tQSL_Error);
			goto imp_end;
		}
	}
	rval = 0;	// Assume no errors
	for (it = usercerts.begin(); it != usercerts.end(); it++) {
		if (tqsl_import_cert(it->pem.c_str(), tqsllib::USERCERT, cb, userdata)) {
			if (tQSL_Error == TQSL_CERT_ERROR) {
				rval = 1;		// Remember failure to import
				continue;
			}
			char savepath[TQSL_MAX_PATH_LEN], badpath[TQSL_MAX_PATH_LEN];
			strncpy(badpath, path, sizeof(badpath));
			strncat(badpath, ".bad", sizeof(badpath)-strlen(badpath)-1);
			badpath[sizeof(badpath)-1] = '\0';
#ifdef _WIN32
			wchar_t* wpath = utf8_to_wchar(path);
			wchar_t* wbadpath = utf8_to_wchar(badpath);
			wchar_t* wsavepath = NULL;
			if (!_wrename(wpath, wbadpath)) {
#else
			if (!rename(path, badpath)) {
#endif
				strncpy(savepath, path, sizeof(savepath));
				strncat(savepath, ".save", sizeof(savepath)-strlen(savepath)-1);
				savepath[sizeof(savepath)-1] = '\0';
#ifdef _WIN32
				wsavepath = utf8_to_wchar(savepath);
				if (_wrename(wsavepath, wpath))  // didn't work
					_wrename(wbadpath, wpath);
#else
				if (rename(savepath, path))  // didn't work
					rename(badpath, path);
#endif
				else
#ifdef _WIN32
			        {
					_wunlink(wbadpath);
					free_wchar(wpath);
					free_wchar(wbadpath);
					if (wsavepath) free_wchar(wsavepath);
				}
#else
					unlink(badpath);
#endif
			}
			goto imp_end;
		}
	}

	if (rval == 0) {
		tQSL_Error = TQSL_NO_ERROR;
		strncpy(tQSL_ImportCall, ImportCall, sizeof tQSL_ImportCall);
	} else {
		if (tQSL_Error == 0) {
			tQSL_Error = TQSL_CERT_ERROR;
		}
	}
 imp_end:
	if (p12)
		PKCS12_free(p12);
	if (in)
		BIO_free(in);
	if (bags)
		sk_PKCS12_SAFEBAG_pop_free(bags, PKCS12_SAFEBAG_free);
	if (safes)
		sk_PKCS7_pop_free(safes, PKCS7_free);
	if (obj)
		ASN1_OBJECT_free(obj);
	if (callobj)
		ASN1_OBJECT_free(callobj);
	if (bs)
		BASIC_CONSTRAINTS_free(bs);
	if (p8)
		PKCS8_PRIV_KEY_INFO_free(p8);
	if (pkey)
		EVP_PKEY_free(pkey);
	return rval;
}

DLLEXPORT int CALLCONVENTION
tqsl_importPKCS12File(const char *filename, const char *p12password, const char *password,
	int (*pwcb)(char *, int, void *), int(*cb)(int, const char *, void *), void *userdata) {
	tqslTrace("tqsl_importPKCS12File", NULL);
	return tqsl_importPKCS12(false, filename, NULL, p12password, password, pwcb, cb, userdata);
}

DLLEXPORT int CALLCONVENTION
tqsl_importPKCS12Base64(const char *base64, const char *p12password, const char *password,
	int (*pwcb)(char *, int, void *), int(*cb)(int, const char *, void *), void *userdata) {
	tqslTrace("tqsl_importPKCS12Base64", NULL);
	return tqsl_importPKCS12(true, NULL, base64, p12password, password, pwcb, cb, userdata);
}

static int
tqsl_backup_cert(tQSL_Cert cert) {
	char callsign[64];
	long serial = 0;
	int dxcc = 0;
	int keyonly;
	tqsl_getCertificateKeyOnly(cert, &keyonly);
	tqsl_getCertificateCallSign(cert, callsign, sizeof callsign);
	if (!keyonly)
		tqsl_getCertificateSerial(cert, &serial);
	tqsl_getCertificateDXCCEntity(cert, &dxcc);

	char backupPath[TQSL_MAX_PATH_LEN];
	tqsl_make_backup_path(callsign, backupPath, sizeof backupPath);

	FILE* out = NULL;
#ifdef _WIN31
	wchar_t* wpath = utf8_to_wchar(backupPath);
	_wunlink(wpath);
	fd =  _wfopen(lfn, L"wb");
	free_wchar(wpath);
#else
	unlink(backupPath);
	out = fopen(backupPath, "wb");
#endif
	if (!out) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		strncpy(tQSL_ErrorFile, backupPath, sizeof tQSL_ErrorFile);
		tQSL_ErrorFile[sizeof tQSL_ErrorFile-1] = 0;
		tqslTrace("tqsl_backup_cert", "Error %d errno %d file %s", tQSL_Error, tQSL_Errno, backupPath);
		return 1;
	}
	char buf[8192];
	fprintf(out, "<UserCert CallSign=\"%s\" dxcc=\"%d\" serial=\"%ld\">\n", callsign, dxcc, serial);
	if (!keyonly) {
		fprintf(out, "<SignedCert>\n");
		tqsl_getCertificateEncoded(cert, buf, sizeof buf);
		fprintf(out, "%s", buf);
		fprintf(out, "</SignedCert>\n");
	}
	fprintf(out, "<PrivateKey>\n");
	tqsl_getKeyEncoded(cert, buf, sizeof buf);
	fprintf(out, "%s", buf);
	fprintf(out, "</PrivateKey>\n</UserCert>\n");
	fclose(out);
	return 0;
}

static int
tqsl_make_backup_list(const char* filter, vector<string>& keys) {
	keys.clear();

	string path = tQSL_BaseDir;
#ifdef _WIN32
	path += "\\certtrash";
	wchar_t* wpath = utf8_to_wchar(path.c_str());
	MKDIR(wpath, 0700);
#else
	path += "/certtrash";
	MKDIR(path.c_str(), 0700);
#endif

#ifdef _WIN32
	WDIR *dir = wopendir(wpath);
	free_wchar(wpath);
#else
	DIR *dir = opendir(path.c_str());
#endif
	if (dir == NULL) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_make_backup_list", "Opendir %s error %s", path.c_str(), strerror(errno));
		return 1;
	}
#ifdef _WIN32
	struct wdirent *ent;
#else
	struct dirent *ent;
#endif
	int rval = 0;
	int savedError = 0;
	int savedErrno = 0;
	char *savedFile = NULL;

#ifdef _WIN32
	while ((ent = wreaddir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;
		char dname[TQSL_MAX_PATH_LEN];
		wcstombs(dname, ent->d_name, TQSL_MAX_PATH_LEN);
		string filename = path + "\\" + dname;
		struct _stat32 s;
		wchar_t* wfilename = utf8_to_wchar(filename.c_str());
		if (_wstat32(wfilename, &s) == 0) {
			if (S_ISDIR(s.st_mode)) {
				free_wchar(wfilename);
				continue;		// If it's a directory, skip it.
			}
		}
#else
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;
		string filename = path + "/" + ent->d_name;
		struct stat s;
		if (stat(filename.c_str(), &s) == 0) {
			if (S_ISDIR(s.st_mode))
				continue;		// If it's a directory, skip it.
		}
#endif
		XMLElement xel;
		int status = xel.parseFile(filename.c_str());
		if (status)
			continue;			// Can't be parsed

		XMLElement cert;
		xel.getFirstElement(cert);
		pair<string, bool> atrval = cert.getAttribute("CallSign");
		if (atrval.second) {
			// If the callsign matches, or if the filter is empty, add it.
			if (filter == NULL || atrval.first == filter) {
				keys.push_back(atrval.first);
			}
		}
	}
#ifdef _WIN32
	_wclosedir(dir);
#else
	closedir(dir);
#endif
	if (rval) {
		tQSL_Error = savedError;
		tQSL_Errno = savedErrno;
		if (savedFile) {
			strncpy(tQSL_ErrorFile, savedFile, sizeof tQSL_ErrorFile);
			free(savedFile);
		}
		tqslTrace("tqsl_make_backup_list", "error %s %s", tQSL_ErrorFile, strerror(tQSL_Errno));
	}
	return rval;
}

DLLEXPORT int CALLCONVENTION
tqsl_deleteCertificate(tQSL_Cert cert) {
	tqslTrace("tqsl_deleteCertificate", NULL);

	if (tqsl_init())
		return 1;
	if (cert == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert), false)) {
		tqslTrace("tqsl_deleteCertificate", "arg err cert=0x%lx", cert);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}

	tqsl_backup_cert(cert);
	char callsign[256], path[TQSL_MAX_PATH_LEN], newpath[TQSL_MAX_PATH_LEN];
	if (tqsl_getCertificateCallSign(cert, callsign, sizeof callsign)) {
		tqslTrace("tqsl_deleteCertificate", "no callsign %d", tQSL_Error);
		return 1;
	}

	int rval = 1;
	EVP_PKEY *key = 0;
	BIO *bio = 0;
	tQSL_Error = TQSL_OPENSSL_ERROR;

	// Delete private key
	map<string, string> rec;
	if (TQSL_API_TO_CERT(cert)->pubkey) {
		rec["PUBLIC_KEY"] = TQSL_API_TO_CERT(cert)->pubkey;
	} else {
		// Get public key from cert
		if ((key = X509_get_pubkey(TQSL_API_TO_CERT(cert)->cert)) == 0) {
			tqslTrace("tqsl_deleteCertificate", "no public key %s", tqsl_openssl_error());
			goto dc_end;
		}
		if ((bio = BIO_new(BIO_s_mem())) == NULL) {
			tqslTrace("tqsl_deleteCertificate", "bio err %s", tqsl_openssl_error());
			goto dc_end;
		}
		if (!PEM_write_bio_PUBKEY(bio, key)) {
			tqslTrace("tqsl_deleteCertificate", "bio write err %s", tqsl_openssl_error());
			goto dc_end;
		}
		char *cp;
		int len = static_cast<int>(BIO_get_mem_data(bio, &cp));
		rec["PUBLIC_KEY"] = string(cp, len);
		BIO_free(bio);
		bio = 0;
		EVP_PKEY_free(key);
		key = 0;
	}
	rec["CALLSIGN"] = callsign;
	if (!tqsl_make_key_path(callsign, path, sizeof path)) {
		tqslTrace("tqsl_deleteCertificate", "key path err %s", tQSL_Error);
		goto dc_end;
	}
	// Since there is no private key in "rec," tqsl_replace_key will just remove the
	// existing private key.
	tqsl_replace_key(callsign, path, rec, 0, 0);

	if (TQSL_API_TO_CERT(cert)->keyonly) {
		tqslTrace("tqsl_deleteCertificate", "key only");
		goto dc_ok;
	}

	// Now the certificate

	tqsl_make_cert_path("user", path, sizeof path);
	tqsl_make_cert_path("user.new", newpath, sizeof newpath);
	if (xcerts == NULL) {
		if ((xcerts = tqsl_ssl_load_certs_from_file(path)) == 0) {
			tqslTrace("tqsl_deleteCertificate", "error reading certs %d", tQSL_Error);
			goto dc_end;
		}
	}
	if ((bio = BIO_new_file(newpath, "wb")) == 0) {
		tqslTrace("tqsl_deleteCertificate", "bio_new_file %s", tqsl_openssl_error());
		goto dc_end;
	}
	X509 *x;
	while ((x = sk_X509_shift(xcerts)) != 0) {
		if (X509_issuer_and_serial_cmp(x, TQSL_API_TO_CERT(cert)->cert)) {
			if (!PEM_write_bio_X509(bio, x)) {	// No match -- keep this one
				tqslTrace("tqsl_deleteCertificate", "pem_write_bio %s", tqsl_openssl_error());
				goto dc_end;
			}
		}
	}
	BIO_free(bio);
	bio = 0;

	// Looks like the new file is okay, commit it
#ifdef _WIN32
	wchar_t* wpath = utf8_to_wchar(path);
	if (_wunlink(wpath) && errno != ENOENT) {
		free_wchar(wpath);
#else
	if (unlink(path) && errno != ENOENT) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_deleteCertificate", "unlink err %d", errno);
		goto dc_end;
	}
#ifdef _WIN32
	wchar_t* wnewpath = utf8_to_wchar(newpath);
	if (_wrename(wnewpath, wpath)) {
		free_wchar(wpath);
		free_wchar(wnewpath);
#else
	if (rename(newpath, path)) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_deleteCertificate", "rename err %d", errno);
		goto dc_end;
	}
#ifdef _WIN32
	free_wchar(wpath);
	free_wchar(wnewpath);
#endif

 dc_ok:
	rval = 0;
	tQSL_Error = TQSL_NO_ERROR;

 dc_end:
	if (xcerts) {
		sk_X509_free(xcerts);
		xcerts = NULL;
	}
	if (key)
		EVP_PKEY_free(key);
	if (bio)
		BIO_free(bio);
	return rval;
}

/** Get the list of restorable callsign certificates. */
DLLEXPORT int CALLCONVENTION
tqsl_getDeletedCallsignCertificates(char ***calls, int *ncall, const char *filter) {
	vector <string> callsigns;

	if (tqsl_make_backup_list(filter, callsigns)) {
		return 1;
	}
	*ncall = callsigns.size();
	if (*ncall == 0) {
		if (calls) {
			*calls = NULL;
		}
		return 0;
	}
	if (calls == NULL) {
		return 0;
	}
	*calls = reinterpret_cast<char **>(calloc(*ncall, sizeof(**calls)));
	vector<string>::iterator it;
	char **p = *calls;
	for (it = callsigns.begin(); it != callsigns.end(); it++) {
		*p++ = strdup((*it).c_str());
	}
	return 0;
}

DLLEXPORT void CALLCONVENTION
tqsl_freeDeletedCertificateList(char **list, int nloc) {
	if (!list) return;
	for (int i = 0; i < nloc; i++)
		if (list[i]) free(list[i]);
	if (list) free(list);
}

/** Restore a deleted callsign certificate by callsign. */
DLLEXPORT int CALLCONVENTION
tqsl_restoreCallsignCertificate(const char *callsign) {
	tqslTrace("tqsl_restoreCallsignCertificate", "callsign = %s", callsign);
	char backupPath[TQSL_MAX_PATH_LEN];
	tqsl_make_backup_path(callsign, backupPath, sizeof backupPath);

	XMLElement xel;
	int status = xel.parseFile(backupPath);
	if (status) {
		if (errno == ENOENT) {		// No file is OK
			tqslTrace("tqsl_restoreCallsignCertificate", "FNF");
			return 0;
		}
		strncpy(tQSL_ErrorFile, backupPath, sizeof tQSL_ErrorFile);
		if (status == XML_PARSE_SYSTEM_ERROR) {
			tQSL_Error = TQSL_FILE_SYSTEM_ERROR;
			tQSL_Errno = errno;
			tqslTrace("tqsl_restoreCallsignCertificate", "open error %s: %s", backupPath, strerror(tQSL_Errno));
		} else {
			tqslTrace("tqsl_restoreCallsignCertificate", "syntax error %s", backupPath);
			tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
		}
		return 1;
	}
	XMLElement cert;
	string call;
	int dxcc = 0;
	long serial = 0;
	string privateKey;
	string publicKey;
	XMLElementList& ellist = xel.getElementList();
	XMLElementList::iterator ep;
	for (ep = ellist.find("UserCert"); ep != ellist.end(); ep++) {
		if (ep->first != "UserCert")
			break;
		pair<string, bool> rval = ep->second->getAttribute("CallSign");
		if (rval.second) call = rval.first;
		rval = ep->second->getAttribute("serial");
		if (rval.second) serial = strtol(rval.first.c_str(), NULL, 10);
		rval = ep->second->getAttribute("dxcc");
		if (rval.second) dxcc = strtol(rval.first.c_str(), NULL, 10);

		XMLElement el;
		if (ep->second->getFirstElement("SignedCert", el)) {
			publicKey = el.getText();
		}
		if (ep->second->getFirstElement("PrivateKey", el)) {
			privateKey = el.getText();
		}
	}

	// See if this certificate exists
	tQSL_Cert *certlist;
	int ncerts;
	tqsl_selectCertificates(&certlist, &ncerts, call.c_str(), dxcc, 0, 0, TQSL_SELECT_CERT_EXPIRED|TQSL_SELECT_CERT_SUPERCEDED|TQSL_SELECT_CERT_WITHKEYS);
	for (int i = 0; i < ncerts; i++) {
		long s = 0;
		int keyonly = false;
		tqsl_getCertificateKeyOnly(certlist[i], &keyonly);
		if (keyonly) {
			if (serial != 0) {		// A full cert for this was imported
				continue;
			}
		}
		if (tqsl_getCertificateSerial(certlist[i], &s)) {
			continue;
		}
		if (s == serial) {			// Already imported
			tqsl_freeCertificateList(certlist, ncerts);
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, "This callsign certificate is already active and cannot be restored.",
				sizeof tQSL_CustomError);
			tqslTrace("tqsl_restoreCallsignCertificate", "certificate already exists");
			return 1;
		}
	}
	tqsl_freeCertificateList(certlist, ncerts);
	int stat = tqsl_importKeyPairEncoded(call.c_str(), "user", privateKey.c_str(), publicKey.c_str());
	if (!stat) {
		// Remove the backup file
#ifdef _WIN32
		wchar_t* wbpath = utf8_to_wchar(backupPath);
		_wunlink(wbpath);
		free_wchar(wbpath);
#else
		unlink(backupPath);
#endif
	}
	return stat;
}

/********** END OF PUBLIC API FUNCTIONS **********/

/* Utility functions to manage private data structures */

static int
tqsl_check_crq_field(tQSL_Cert cert, char *buf, int bufsiz) {
	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || bufsiz < 0 || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tqslTrace("tqsl_check_crq_field", "arg err cert=0x%lx buf=0x%lx bufsiz=%d", cert, buf, bufsiz);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->crq == NULL) {
		if (tqsl_find_matching_key(TQSL_API_TO_CERT(cert)->cert, NULL,
			&(TQSL_API_TO_CERT(cert)->crq), "", NULL, NULL) && tQSL_Error != TQSL_PASSWORD_ERROR) {
			tqslTrace("tqsl_check_crq_field", "can't find matching key err %d", tQSL_Error);
			return 1;
		}
	}
	return 0;
}

static int
tqsl_cert_check(tqsl_cert *p, bool needcert) {
	if (p != NULL && p->id == 0xCE && (!needcert || p->cert != NULL))
		return 1;
	tQSL_Error = TQSL_ARGUMENT_ERROR;
	return 0;
}

static tqsl_cert *
tqsl_cert_new() {
	tqsl_cert *p;

	p = reinterpret_cast<tqsl_cert *>(tqsl_calloc(1, sizeof(tqsl_cert)));
	if (p != NULL)
		p->id = 0xCE;
	return p;
}

static void
tqsl_cert_free(tqsl_cert *p) {
	if (p == NULL || p->id != 0xCE)
		return;
	p->id = 0;
	if (p->cert != NULL)
		X509_free(p->cert);
	if (p->key != NULL)
		EVP_PKEY_free(p->key);
	if (p->crq != NULL)
		tqsl_free_cert_req(p->crq, 0);
	if (p->pubkey != NULL)
		delete[] p->pubkey;
	if (p->privkey != NULL)
		delete[] p->privkey;
	tqsl_free(p);
}

static TQSL_CERT_REQ *
tqsl_free_cert_req(TQSL_CERT_REQ *req, int seterr) {
	if (req == NULL)
		return NULL;
	tqsl_free(req);
	if (seterr)
		tQSL_Error = seterr;
	return NULL;
}

static TQSL_CERT_REQ *
tqsl_copy_cert_req(TQSL_CERT_REQ *userreq) {
	TQSL_CERT_REQ *req;

	if ((req = reinterpret_cast<TQSL_CERT_REQ *>(tqsl_calloc(1, sizeof(TQSL_CERT_REQ)))) == NULL) {
		tqslTrace("tqsl_copy_cert_req", "ENOMEM");
		errno = ENOMEM;
		return NULL;
	}
	*req = *userreq;
	return req;
}

static int
tqsl_check_parm(const char *p, const char *parmName) {
	if (strlen(p) == 0) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
			"Missing parameter: %s", parmName);
		tqslTrace("tqsl_check_parm", "error %s", tQSL_CustomError);
		return 1;
	}
	return 0;
}

static char *
tqsl_trim(char *buf) {
	char lastc;
	char *cp, *op;

	/* Trim white space off end of string */
	cp = buf + strlen(buf);
	while (cp != buf) {
		cp--;
		if (!isspace(*cp))
			break;
		*cp = 0;
	}
	/* Skip past leading whitespace */
	for (cp = buf; isspace(*cp); cp++)
	    {}
	/* Fold runs of white space into single space */
	lastc = 0;
	op = buf;
	for (; *cp != '\0'; cp++) {
		if (isspace(*cp))
			*cp = ' ';
		if (*cp != ' ' || lastc != ' ')
			*op++ = *cp;
		lastc = *cp;
	}
	*op = '\0';
	return cp;
}
/* Filter a list (stack) of X509 certs based on call sign, QSO date and/or
 * issuer.
 *
 * Returns a new stack of matching certs without altering the original stack.
 *
 * Note that you don't have to supply any of the criteria. If you supply
 * none, you;ll just get back an exact copy of the stack.
 */
CLIENT_STATIC STACK_OF(X509) *
tqsl_filter_cert_list(STACK_OF(X509) *sk, const char *callsign, int dxcc,
	const tQSL_Date *date, const TQSL_PROVIDER *issuer, int flags) {
	set<string> superceded_certs;

	char buf[256], name_buf[256];
	TQSL_X509_NAME_ITEM item;
	X509 *x;
	STACK_OF(X509) *newsk;
	int i, ok, len;
	tQSL_Date qso_date;

	tqslTrace("tqsl_filter_cert_list", NULL);
	if (tqsl_init())
		return NULL;
	if ((newsk = sk_X509_new_null()) == NULL) {
		tqslTrace("tqsl_filter_cert_list", "sk_X509_new_null err %s", tqsl_openssl_error());
		return NULL;
	}
	tqsl_cert* cp = tqsl_cert_new();
	/* Loop through the list of certs */
	for (i = 0; i < sk_X509_num(sk); i++) {
		ok = 1;	/* Certificate is selected unless some check says otherwise */
		x = sk_X509_value(sk, i);
		cp->cert = x;
		/* Check for expired unless asked not to */
		if (ok && !(flags & TQSL_SELECT_CERT_EXPIRED)) {
			int exp = false;
			if (!tqsl_isCertificateExpired(TQSL_OBJ_TO_API(cp), &exp)) {
				if (exp) {
					ok = 0;
				}
			}
		}
		/* Check for superceded unless asked not to */
		if (ok && !(flags & TQSL_SELECT_CERT_SUPERCEDED)) {
			int sup = false;
			if (!tqsl_isCertificateSuperceded(TQSL_OBJ_TO_API(cp), &sup)) {
				if (sup) {
					ok = 0;
				}
			}
		}
		/* Compare issuer if asked to */
		if (ok && issuer != NULL) {
			X509_NAME *iss;
			if ((iss = X509_get_issuer_name(x)) == NULL)
				ok = 0;
			if (ok) {
				item.name_buf = name_buf;
				item.name_buf_size = sizeof name_buf;
				item.value_buf = buf;
				item.value_buf_size = sizeof buf;
				tqsl_get_name_entry(iss, "organizationName", &item);
				ok = !strcmp(issuer->organizationName, item.value_buf);
			}
			if (ok) {
				item.name_buf = name_buf;
				item.name_buf_size = sizeof name_buf;
				item.value_buf = buf;
				item.value_buf_size = sizeof buf;
				tqsl_get_name_entry(iss, "organizationalUnitName", &item);
				ok = !strcmp(issuer->organizationalUnitName, item.value_buf);
			}
		}
		/* Check call sign if asked */
		if (ok && callsign != NULL) {
			item.name_buf = name_buf;
			item.name_buf_size = sizeof name_buf;
			item.value_buf = buf;
			item.value_buf_size = sizeof buf;
			if (!tqsl_cert_get_subject_name_entry(x, "AROcallsign", &item))
				ok = 0;
			else
				ok = !strcmp(callsign, item.value_buf);
		}
		/* Check DXCC entity if asked */
		if (ok && dxcc > 0) {
			len = sizeof buf-1;
			if (tqsl_get_cert_ext(x, "dxccEntity", (unsigned char *)buf, &len, NULL)) {
				ok = 0;
			} else {
				buf[len] = 0;
				if (dxcc != strtol(buf, NULL, 10))
					ok = 0;
			}
		}
		/* Check QSO date if asked */
		if (ok && date != NULL && !tqsl_isDateNull(date)) {
			len = sizeof buf-1;
			if (tqsl_get_cert_ext(x, "QSONotBeforeDate", (unsigned char *)buf, &len, NULL))
				ok = 0;
			else if (tqsl_initDate(&qso_date, buf))
				ok = 0;
			else if (tqsl_compareDates(date, &qso_date) < 0)
				ok = 0;
		}
		if (ok && date != NULL && !tqsl_isDateNull(date)) {
			len = sizeof buf-1;
			if (tqsl_get_cert_ext(x, "QSONotAfterDate", (unsigned char *)buf, &len, NULL))
				ok = 0;
			else if (tqsl_initDate(&qso_date, buf))
				ok = 0;
			else if (tqsl_compareDates(date, &qso_date) > 0)
				ok = 0;
		}
		/* If no check failed, copy this cert onto the new stack */
		if (ok)
			sk_X509_push(newsk, X509_dup(x));
	}
	tqsl_free(cp);
	return newsk;
}


/* Set up a read-only BIO from the given file and pass to
 * tqsl_ssl_load_certs_from_BIO.
 */
CLIENT_STATIC STACK_OF(X509) *
tqsl_ssl_load_certs_from_file(const char *filename) {
	BIO *in;
	STACK_OF(X509) *sk;
	FILE *cfile;

	tqslTrace("tqsl_ssl_load_certs_from_file", "filename=%s", filename);

#ifdef _WIN32
	wchar_t* wfilename = utf8_to_wchar(filename);
	if ((cfile = _wfopen(wfilename, L"r")) == NULL) {
		free_wchar(wfilename);
#else
	if ((cfile = fopen(filename, "r")) == NULL) {
#endif
		strncpy(tQSL_ErrorFile, filename, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_ssl_load_certs_from_file", "File open error %s", strerror(errno));
		return NULL;
	}
#ifdef _WIN32
	free(wfilename);
#endif
	if ((in = BIO_new_fp(cfile, 0)) == NULL) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		tqslTrace("tqsl_ssl_load_certs_from_file", "bio_new_fp err %s", tqsl_openssl_error());
		return NULL;
	}
	sk = tqsl_ssl_load_certs_from_BIO(in);
	BIO_free(in);
	fclose(cfile);
	return sk;
}

/* Load a set of certs from a file into a stack. The file may contain
 * other X509 objects (e.g., CRLs), but we'll ignore those.
 *
 * Return NULL if there are no certs in the file or on error.
 */
CLIENT_STATIC STACK_OF(X509) *
tqsl_ssl_load_certs_from_BIO(BIO *in) {
	STACK_OF(X509_INFO) *sk = NULL;
	STACK_OF(X509) *stack = NULL;
	X509_INFO *xi;

	if (tqsl_init())
		return NULL;
	if (!(stack = sk_X509_new_null())) {
		tqslTrace("tqsl_ssl_load_certs_from_BIO", "bio_new_fp err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	if (!(sk = PEM_X509_INFO_read_bio(in, NULL, NULL, NULL))) {
		sk_X509_free(stack);
		tqslTrace("tqsl_ssl_load_certs_from_BIO", "PEM_X509_INFO_read_bio err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	/* Extract the certs from the X509_INFO objects and put them on a stack */
	while (sk_X509_INFO_num(sk)) {
		xi = sk_X509_INFO_shift(sk);
		if (xi->x509 != NULL) {
			sk_X509_push(stack, xi->x509);
			xi->x509 = NULL;
		}
		X509_INFO_free(xi);
	}
/*	Empty file isn't really an error, is it?
	if(!sk_X509_num(stack)) {
		sk_X509_free(stack);
		stack = NULL;
		strcpy(tQSL_CustomError, "No certificates found");
		tQSL_Error = TQSL_CUSTOM_ERROR;
	}
*/
	sk_X509_INFO_free(sk);
	return stack;
}

/* Chain-verify a cert against a set of CA and a set of trusted root certs.
 *
 * Returns NULL if cert verifies, an error message if it does not.
 */
CLIENT_STATIC const char *
tqsl_ssl_verify_cert(X509 *cert, STACK_OF(X509) *cacerts, STACK_OF(X509) *rootcerts, int purpose,
	int (*cb)(int ok, X509_STORE_CTX *ctx), STACK_OF(X509) **chain) {
	X509_STORE *store;
	X509_STORE_CTX *ctx;
	int rval;
	const char *errm;

	if (cert == NULL) {
		tqslTrace("tqsl_ssl_verify_cert", "No certificate to verify");
		return "No certificate to verify";
	}
	if (tqsl_init())
		return NULL;
	store = X509_STORE_new();
	if (store == NULL) {
		tqslTrace("tqsl_ssl_verify_cert", "out of memory");
		return "Out of memory";
	}
	if (cb != NULL)
		X509_STORE_set_verify_cb(store, cb);
	ctx = X509_STORE_CTX_new();
	if (ctx == NULL) {
		X509_STORE_free(store);
		tqslTrace("tqsl_ssl_verify_cert", "store_ctx_new out of memory");
		return "Out of memory";
	}
	X509_STORE_CTX_init(ctx, store, cert, cacerts);
	if (cb != NULL)
		X509_STORE_CTX_set_verify_cb(ctx, cb);
	if (rootcerts)
		X509_STORE_CTX_trusted_stack(ctx, rootcerts);
	if (purpose >= 0)
		X509_STORE_CTX_set_purpose(ctx, purpose);
	X509_STORE_CTX_set_flags(ctx, X509_V_FLAG_CB_ISSUER_CHECK);
	rval = X509_verify_cert(ctx);
	errm = X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx));
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define X509_STORE_CTX_get0_chain(o) ((o)->chain)
#endif
	if (chain) {
		if (rval && X509_STORE_CTX_get0_chain(ctx))
			*chain = sk_X509_dup(X509_STORE_CTX_get0_chain(ctx));
		else
			*chain = 0;
	}
	X509_STORE_CTX_free(ctx);
	if (rval)
		return NULL;
	if (errm != NULL) {
		tqslTrace("tqsl_ssl_verify_cert", "err %s", errm);
		return errm;
	}
	return "Verification failed";
}

/* [static] - Grab the data from an X509_NAME_ENTRY and put it into
 * a TQSL_X509_NAME_ITEM object, checking buffer sizes.
 *
 * Returns 0 on error, 1 if okay.
 *
 * It's okay for the name_buf or value_buf item of the object to
 * be NULL; it'll just be skipped.
 */
static int
tqsl_get_name_stuff(X509_NAME_ENTRY *entry, TQSL_X509_NAME_ITEM *name_item) {
	ASN1_OBJECT *obj;
	ASN1_STRING *value;
	const char *val;
	unsigned int len;

	if (entry == NULL) {
		tqslTrace("tqsl_get_name_stuff", "entry=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 0;
	}
	obj = X509_NAME_ENTRY_get_object(entry);
	if (obj == NULL) {
		tqslTrace("tqsl_get_name_stuff", "get_object err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 0;
	}
	if (name_item->name_buf != NULL) {
		len = i2t_ASN1_OBJECT(name_item->name_buf, name_item->name_buf_size, obj);
		if (len <= 0 || len > strlen(name_item->name_buf)) {
			tqslTrace("tqsl_get_name_stuff", "len error len= %d need=%d", len, strlen(name_item->name_buf));
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 0;
		}
	}
	if (name_item->value_buf != NULL) {
		value = X509_NAME_ENTRY_get_data(entry);
		val = (const char *)ASN1_STRING_get0_data(value);
		strncpy(name_item->value_buf, val, name_item->value_buf_size);
		name_item->value_buf[name_item->value_buf_size-1] = '\0';
		if (strlen(val) > strlen(name_item->value_buf)) {
			tqslTrace("tqsl_get_name_stuff", "len error len= %d need=%d", strlen(val), strlen(name_item->value_buf));
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 0;
		}
	}
	return 1;
}

/* Get a name entry from an X509_NAME by its name.
 */
CLIENT_STATIC int
tqsl_get_name_entry(X509_NAME *name, const char *obj_name, TQSL_X509_NAME_ITEM *name_item) {
	X509_NAME_ENTRY *entry;
	int num_entries, i;

	if (tqsl_init())
		return 0;
	num_entries = X509_NAME_entry_count(name);
	if (num_entries <= 0)
		return 0;
	/* Loop through the name entries */
	for (i = 0; i < num_entries; i++) {
		entry = X509_NAME_get_entry(name, i);
		if (!tqsl_get_name_stuff(entry, name_item))
			continue;
		if (name_item->name_buf != NULL && !strcmp(name_item->name_buf, obj_name)) {
			/* Found the wanted entry */
			return 1;
		}
	}
	return 0;
}

/* Get a name entry from a cert's subject name by its name.
 */
CLIENT_STATIC int
tqsl_cert_get_subject_name_entry(X509 *cert, const char *obj_name, TQSL_X509_NAME_ITEM *name_item) {
	X509_NAME *name;

	if (cert == NULL)
		return 0;
	if (tqsl_init())
		return 0;
	if ((name = X509_get_subject_name(cert)) == NULL)
		return 0;
	return tqsl_get_name_entry(name, obj_name, name_item);
}

/* Initialize the tQSL (really OpenSSL) random number generator
 * Return 0 on error.
 */
CLIENT_STATIC int
tqsl_init_random() {
	char fname[TQSL_MAX_PATH_LEN];
	static int initialized = 0;

	if (initialized)
		return 1;
	if (RAND_file_name(fname, sizeof fname) != NULL)
		RAND_load_file(fname, -1);
	initialized = RAND_status();
	if (!initialized) {
		tqslTrace("tqsl_init_random", "init error %s", tqsl_openssl_error());
		tQSL_Error = TQSL_RANDOM_ERROR;
	}
	return initialized;
}

/* Generate an RSA key of at least 1024 bits length
 */
CLIENT_STATIC EVP_PKEY *
tqsl_new_rsa_key(int nbits) {
	EVP_PKEY *pkey;

	if (nbits < 1024) {
		tqslTrace("tqsl_new_rsa_key", "nbits too small %d", nbits);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return NULL;
	}
	if ((pkey = EVP_PKEY_new()) == NULL) {
		tqslTrace("tqsl_new_rsa_key", "EVP_PKEY_new err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	if (!tqsl_init_random())	/* Unable to init RN generator */
		return NULL;
	RSA *rsa = RSA_new();
	if (rsa == NULL) {
		EVP_PKEY_free(pkey);
		tqslTrace("tqsl_new_rsa_key", "RSA_new err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	BIGNUM *pubexp = BN_new();
	if (pubexp == NULL) {
		EVP_PKEY_free(pkey);
		RSA_free(rsa);
		tqslTrace("tqsl_new_rsa_key", "BN_new err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	if (BN_set_word(pubexp, 0x10001) != 1) {
		EVP_PKEY_free(pkey);
		RSA_free(rsa);
		BN_free(pubexp);
		tqslTrace("tqsl_new_rsa_key", "BN_set_word err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	if (RSA_generate_key_ex(rsa, nbits, pubexp, NULL) != 1) {
		EVP_PKEY_free(pkey);
		RSA_free(rsa);
		BN_free(pubexp);
		tqslTrace("tqsl_new_rsa_key", "RSA_generate_key err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	if (!EVP_PKEY_assign_RSA(pkey, rsa)) {
		EVP_PKEY_free(pkey);
		RSA_free(rsa);
		BN_free(pubexp);
		tqslTrace("tqsl_new_rsa_key", "EVP_PKEY_assign_RSA err %s", tqsl_openssl_error());
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	BN_free(pubexp);
	return pkey;
}


/* Output an ADIF field to a file descriptor.
 */
CLIENT_STATIC int
tqsl_write_adif_field(FILE *fp, const char *fieldname, char type, const unsigned char *value, int len) {
	if (fieldname == NULL)	/* Silly caller */
		return 0;
	if (fputc('<', fp) == EOF)
		return 1;
	if (fputs(fieldname, fp) == EOF)
		return 1;
	if (type && type != ' ' && type != '\0') {
		if (fputc(':', fp) == EOF)
			return 1;
		if (fputc(type, fp) == EOF)
			return 1;
	}
	if (value != NULL && len != 0) {
		if (len < 0)
			len = strlen((const char *)value);
		if (fputc(':', fp) == EOF)
			return 1;
		fprintf(fp, "%d>", len);
		if (fwrite(value, 1, len, fp) != (unsigned int) len)
			return 1;
	} else if (fputc('>', fp) == EOF) {
			return 1;
	}
	if (fputs("\n\n", fp) == EOF)
		return 1;
	return 0;
}

/* Output an ADIF field to a BIO
 */
CLIENT_STATIC int
tqsl_bio_write_adif_field(BIO *bio, const char *fieldname, char type, const unsigned char *value, int len) {
	int bret;
	if (fieldname == NULL)	/* Silly caller */
		return 0;
	if ((bret = BIO_write(bio, "<", 1)) <= 0)
		return 1;
	if ((bret = BIO_write(bio, fieldname, strlen(fieldname))) <= 0)
		return 1;
	if (type && type != ' ' && type != '\0') {
		if ((bret = BIO_write(bio, ":", 1)) <= 0)
			return 1;
		if ((bret = BIO_write(bio, &type, 1)) <= 0)
			return 1;
	}
	if (value != NULL && len != 0) {
		if (len < 0)
			len = strlen((const char *)value);
		if ((bret = BIO_write(bio, ":", 1)) <= 0)
			return 1;
		char numbuf[20];
		snprintf(numbuf, sizeof numbuf, "%d>", len);
		if ((bret = BIO_write(bio, numbuf, strlen(numbuf))) <= 0)
			return 1;
		if ((bret = BIO_write(bio, value, len)) != len)
			return 1;
	} else if ((bret = BIO_write(bio, ">", 1)) <= 0) {
			return 1;
	}
	if ((bret = BIO_write(bio, "\n\n", 2)) <= 0)
		return 1;
	return 0;
}

static int
tqsl_self_signed_is_ok(int ok, X509_STORE_CTX *ctx) {
	if (X509_STORE_CTX_get_error(ctx) == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT)
		return 1;
	if (X509_STORE_CTX_get_error(ctx) == X509_V_ERR_CERT_UNTRUSTED)
		return 1;
	return ok;
}

static int
tqsl_expired_is_ok(int ok, X509_STORE_CTX *ctx) {
	if (X509_STORE_CTX_get_error(ctx) == X509_V_ERR_CERT_HAS_EXPIRED ||
	    X509_STORE_CTX_get_error(ctx) == X509_V_ERR_CERT_UNTRUSTED)
		return 1;
	return ok;
}

static char *
tqsl_make_cert_path(const char *filename, char *path, int size) {
	strncpy(path, tQSL_BaseDir, size);
#ifdef _WIN32
	strncat(path, "\\certs", size - strlen(path));
	wchar_t* wpath = utf8_to_wchar(path);
	if (MKDIR(wpath, 0700) && errno != EEXIST) {
		free_wchar(wpath);
#else
	strncat(path, "/certs", size - strlen(path));
	if (MKDIR(path, 0700) && errno != EEXIST) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_make_cert_path", "Making path %s failed with %s", path, strerror(errno));
		return NULL;
	}
#ifdef _WIN32
	free_wchar(wpath);
	strncat(path, "\\", size - strlen(path));
#else
	strncat(path, "/", size - strlen(path));
#endif
	strncat(path, filename, size - strlen(path));
	return path;
}

static int
tqsl_clean_call(const char *callsign, char *buf, int size) {
	if ((static_cast<int>(strlen(callsign))) > size-1) {
		tQSL_Error = TQSL_BUFFER_ERROR;
		return 1;
	}
	const char *cp;
	for (cp = callsign; *cp; cp++) {
		if (!isdigit(*cp) && !isalpha(*cp))
			*buf = '_';
		else
			*buf = *cp;
		++buf;
	}
	*buf = 0;
	return 0;
}

static char *
tqsl_make_key_path(const char *callsign, char *path, int size) {
	char fixcall[256];

	tqsl_clean_call(callsign, fixcall, sizeof fixcall);
	strncpy(path, tQSL_BaseDir, size);
#ifdef _WIN32
	strncat(path, "\\keys", size - strlen(path));
	wchar_t* wpath = utf8_to_wchar(path);
	if (MKDIR(wpath, 0700) && errno != EEXIST) {
		free_wchar(wpath);
#else
	strncat(path, "/keys", size - strlen(path));
	if (MKDIR(path, 0700) && errno != EEXIST) {
#endif
		strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_make_key_path", "Making path %s failed with %s", path, strerror(errno));
		return 0;
	}
#ifdef _WIN32
	free_wchar(wpath);
	strncat(path, "\\", size - strlen(path));
#else
	strncat(path, "/", size - strlen(path));
#endif
	strncat(path, fixcall, size - strlen(path));
	return path;
}

static char *
tqsl_make_backup_path(const char *callsign, char *path, int size) {
	char fixcall[256];

	tqsl_clean_call(callsign, fixcall, sizeof fixcall);
	strncpy(path, tQSL_BaseDir, size);
#ifdef _WIN32
	strncat(path, "\\certtrash", size - strlen(path));
	wchar_t* wpath = utf8_to_wchar(path);
	if (MKDIR(wpath, 0700) && errno != EEXIST) {
		free_wchar(wpath);
#else
	strncat(path, "/certtrash", size - strlen(path));
	if (MKDIR(path, 0700) && errno != EEXIST) {
#endif
		strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_make_backup_path", "Making path %s failed with %s", path, strerror(errno));
		return 0;
	}
#ifdef _WIN32
	free_wchar(wpath);
	strncat(path, "\\", size - strlen(path));
#else
	strncat(path, "/", size - strlen(path));
#endif
	strncat(path, fixcall, size - strlen(path));
	return path;
}

static int
tqsl_handle_root_cert(const char *pem, X509 *x, int (*cb)(int, const char *, void *), void *userdata) {
	const char *cp;

	/* Verify self-signature on the root certificate */
	if ((cp = tqsl_ssl_verify_cert(x, NULL, NULL, 0, &tqsl_self_signed_is_ok)) != NULL) {
		strncpy(tQSL_CustomError, cp, sizeof tQSL_CustomError);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		tqslTrace("tqsl_handle_root_cert", "sig verify err %s", tQSL_CustomError);
		return 1;
	}
	return tqsl_store_cert(pem, x, "root", TQSL_CERT_CB_ROOT, false, cb, userdata);
}

static int
tqsl_ssl_error_is_nofile() {
	if (tQSL_Error == TQSL_SYSTEM_ERROR && tQSL_Errno == ENOENT)
		return 1;
	return 0;
}

static int
tqsl_handle_ca_cert(const char *pem, X509 *x, int (*cb)(int, const char *, void *), void *userdata) {
	STACK_OF(X509) *root_sk;
	char rootpath[TQSL_MAX_PATH_LEN];
	const char *cp;

	tqsl_make_cert_path("root", rootpath, sizeof rootpath);
	if ((root_sk = tqsl_ssl_load_certs_from_file(rootpath)) == NULL) {
		if (!tqsl_ssl_error_is_nofile()) {
			tqslTrace("tqsl_handle_ca_cert", "error not nofile - %d", errno);
			return 1;
		}
	}
	cp = tqsl_ssl_verify_cert(x, NULL, root_sk, 0, &tqsl_expired_is_ok);
	sk_X509_free(root_sk);
	if (cp) {
		strncpy(tQSL_CustomError, cp, sizeof tQSL_CustomError);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		tqslTrace("tqsl_handle_ca_cert", "verify error %s", tQSL_CustomError);
		return 1;
	}
	return tqsl_store_cert(pem, x, "authorities", TQSL_CERT_CB_CA, false, cb, userdata);
}

static int
tqsl_handle_user_cert(const char *cpem, X509 *x, int (*cb)(int, const char *, void *), void *userdata) {
	STACK_OF(X509) *root_sk, *ca_sk;
	char rootpath[TQSL_MAX_PATH_LEN], capath[TQSL_MAX_PATH_LEN];
	char pem[sizeof tqsl_static_buf];
	const char *cp;

	strncpy(pem, cpem, sizeof pem);
	/* Match the public key in the supplied certificate with a
	 * private key in the key store.
	 */
	if (tqsl_find_matching_key(x, NULL, NULL, "", NULL, NULL)) {
		if (tQSL_Error != TQSL_PASSWORD_ERROR) {
			tqslTrace("tqsl_handle_user_cert", "match error %s", tqsl_openssl_error());
			return 1;
		}
		tQSL_Error = TQSL_NO_ERROR;	/* clear error */
	}

	/* Check the chain of authority back to a trusted root */
	tqsl_make_cert_path("root", rootpath, sizeof rootpath);
	if ((root_sk = tqsl_ssl_load_certs_from_file(rootpath)) == NULL) {
		if (!tqsl_ssl_error_is_nofile()) {
			tqslTrace("tqsl_handle_user_cert", "Error loading certs %s", tqsl_openssl_error());
			return 1;
		}
	}
	tqsl_make_cert_path("authorities", capath, sizeof capath);
	if ((ca_sk = tqsl_ssl_load_certs_from_file(capath)) == NULL) {
		if (!tqsl_ssl_error_is_nofile()) {
			sk_X509_free(root_sk);
			tqslTrace("tqsl_handle_user_cert", "Error loading authorities %s", tqsl_openssl_error());
			return 1;
		}
	}
	cp = tqsl_ssl_verify_cert(x, ca_sk, root_sk, 0, &tqsl_expired_is_ok);
	sk_X509_free(ca_sk);
	sk_X509_free(root_sk);
	if (cp) {
		strncpy(tQSL_CustomError, cp, sizeof tQSL_CustomError);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		tqslTrace("tqsl_handle_user_cert", "verify error %s", cp);
		return 1;
	}
	return tqsl_store_cert(pem, x, "user", TQSL_CERT_CB_USER, false, cb, userdata);
}

CLIENT_STATIC int
tqsl_store_cert(const char *pem, X509 *cert, const char *certfile, int type, bool force,
	int (*cb)(int, const char *, void *), void *userdata) {
	STACK_OF(X509) *sk;
	char path[TQSL_MAX_PATH_LEN];
	char issuer[256];
	char name[256];
	char value[256];
	FILE *out;
	BIGNUM *bserial, *oldserial;
	string subjid, msg, callsign;
	TQSL_X509_NAME_ITEM item;
	int len, rval;
	tQSL_Date newExpires;
	string stype = "Unknown";
	const ASN1_TIME *tm;

	if (type == TQSL_CERT_CB_ROOT) {
		stype = "Trusted Root Authority";
	} else if (type == TQSL_CERT_CB_CA) {
		stype = "Certificate Authority";
	} else if (type == TQSL_CERT_CB_USER) {
		stype = "Callsign";
		// Invalidate the cached user certs
		if (xcerts != NULL) {
			sk_X509_free(xcerts);
			xcerts = NULL;
		}
	}

	tqsl_make_cert_path(certfile, path, sizeof path);
	item.name_buf = name;
	item.name_buf_size = sizeof name;
	item.value_buf = value;
	item.value_buf_size = sizeof value;
	if (tqsl_cert_get_subject_name_entry(cert, "AROcallsign", &item)) {
		// Subject contains a call sign (probably a user cert)
		callsign = value;
		strncpy(ImportCall, callsign.c_str(), sizeof(tQSL_ImportCall));
		tQSL_ImportSerial = ASN1_INTEGER_get(X509_get_serialNumber(cert));
		subjid = string("  ") + value;
		tm = X509_get_notAfter(cert);
		if (tm) {
			tqsl_get_asn1_date(tm, &newExpires);
		} else {
			newExpires.year = 9999;
			newExpires.month = 1;
			newExpires.day = 1;
		}
		if (tqsl_cert_get_subject_name_entry(cert, "commonName", &item))
			subjid += string(" (") + value + ")";
		len = sizeof value-1;
		if (!tqsl_get_cert_ext(cert, "dxccEntity", (unsigned char *)value, &len, NULL)) {
			value[len] = 0;
			subjid += string("  DXCC = ") + value;
		}
	} else if (tqsl_cert_get_subject_name_entry(cert, "organizationName", &item)) {
		// Subject contains an organization (probably a CA or root CA cert)
		subjid = string("  ") + value;
		if (tqsl_cert_get_subject_name_entry(cert, "organizationalUnitName", &item))
			subjid += string(" ") + value;
	}
	if (subjid == "") {
		// If haven't found a displayable subject name we understand, use the raw DN
		X509_NAME_oneline(X509_get_subject_name(cert), issuer, sizeof issuer);
		subjid = string("  ") + issuer;
	}

	X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof issuer);

	/* Check for dupes */
	if ((sk = tqsl_ssl_load_certs_from_file(path)) == NULL) {
		if (!tqsl_ssl_error_is_nofile()) {
			tqslTrace("tqsl_store_cert", "unexpected openssl err %s", tqsl_openssl_error());
			return 1;	/* Unexpected OpenSSL error */
		}
	}
	/* Check each certificate */
	if (sk != NULL) {
		int i, n;
		tQSL_Date expires;
		bserial = BN_new();
		ASN1_INTEGER_to_BN(X509_get_serialNumber(cert), bserial);

		n = sk_X509_num(sk);
		for (i = 0; i < n; i++) {
			char buf[256];
			X509 *x;
			const char *cp;

			x = sk_X509_value(sk, i);
			cp = X509_NAME_oneline(X509_get_issuer_name(x), buf, sizeof buf);
			if (cp != NULL && !strcmp(cp, issuer)) {
				oldserial = BN_new();
				ASN1_INTEGER_to_BN(X509_get_serialNumber(x), oldserial);
				int result = BN_ucmp(bserial, oldserial);
				BN_free(oldserial);
				if (result == 0)
					break;	/* We have a match */
			}

			if (!force && type == TQSL_CERT_CB_USER) { // Don't check for newer certs on restore
				item.name_buf = name;
				item.name_buf_size = sizeof name;
				item.value_buf = value;
				item.value_buf_size = sizeof value;
				if (tqsl_cert_get_subject_name_entry(x, "AROcallsign", &item)) {
					if (value == callsign) {
						/*
						 * If it's another cert for 
						 * this call, is it older?
						 */
						tm = X509_get_notAfter(x);
						if (tm) {
							tqsl_get_asn1_date(tm, &expires);
						} else {
							expires.year = 0;
							expires.month = 0;
							expires.day = 0;
						}
						if (tqsl_compareDates(&newExpires, &expires) < 0) {
							tQSL_Error = TQSL_CUSTOM_ERROR;
							strncpy(tQSL_CustomError, "A newer certificate for this callsign is already installed",
								sizeof tQSL_CustomError);
							tqslTrace("tqsl_load_cert", tQSL_CustomError);
							BN_free(bserial);
							sk_X509_free(sk);
							return 1;
						}
					}
				}
			}
		}
		BN_free(bserial);
		sk_X509_free(sk);
		if (i < n) {	/* Have a match -- cert is already in the file */
			if (cb != NULL) {
				int rval;
				string msg = "Duplicate " + stype + " certificate: " + subjid;

				rval = (*cb)(TQSL_CERT_CB_RESULT | type | TQSL_CERT_CB_DUPLICATE, msg.c_str(), userdata);
				if (rval) {
					tQSL_Error = TQSL_CUSTOM_ERROR;
					strncpy(tQSL_CustomError, "Duplicate Callsign certificate",
						sizeof tQSL_CustomError);
					tqslTrace("tqsl_load_cert", tQSL_CustomError);
					return 1;
				}
			}
			if (tQSL_Error == 0) {
				tQSL_Error = TQSL_CERT_ERROR;
			}
			return 1;
		}
	}
	/* Cert is not a duplicate. Append it to the certificate file */
	if (cb != NULL) {
		msg = "Adding " + stype + " Certificate for: " + subjid;

		tqslTrace("tqsl_load_cert", msg.c_str());
		rval = (*cb)(TQSL_CERT_CB_MILESTONE | type | TQSL_CERT_CB_PROMPT, msg.c_str(), userdata);
		if (rval) {
			tqslTrace("tqsl_load_cert", "operator aborted");
			tQSL_Error = TQSL_OPERATOR_ABORT;
			return 1;
		}
	}
#ifdef _WIN32
	wchar_t* wpath = utf8_to_wchar(path);
	if ((out = _wfopen(wpath, L"a")) == NULL) {
		free_wchar(wpath);
#else
	if ((out = fopen(path, "a")) == NULL) {
#endif
		strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_load_cert", "opening file err %s", strerror(errno));
		return 1;
	}
#ifdef _WIN32
	free_wchar(wpath);
#endif
	// Make sure there's always a newline between certs
	size_t pemlen = strlen(pem);
	if (fwrite("\n", 1, 1, out) != 1 || fwrite(pem, 1, pemlen, out) != pemlen) {
		strncpy(tQSL_ErrorFile, certfile, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_load_cert", "writing file err %s", strerror(errno));
		return 1;
	}
	if (fclose(out) == EOF) {
		strncpy(tQSL_ErrorFile, certfile, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_load_cert", "writing file err %s", strerror(errno));
		return 1;
	}
	msg = "Loaded: " + subjid;
	if (cb)
		rval = (*cb)(TQSL_CERT_CB_RESULT | type | TQSL_CERT_CB_LOADED, msg.c_str(), userdata);
	else
		rval = 0;
	if (rval) {
		tQSL_Error = TQSL_OPERATOR_ABORT;
		return 1;
	}
	strncpy(tQSL_ImportCall, ImportCall, sizeof tQSL_ImportCall);
	return 0;
}

static int pw_aborted;

static int
fixed_password_callback(char *buf, int bufsiz, int verify, void *userdata) {
	pw_aborted = 0;
	if (userdata != NULL)
		strncpy(buf, reinterpret_cast<char *>(userdata), bufsiz);
	else
		buf[0] = 0;
	return strlen(buf);
}

static void *prompt_userdata;

static int
prompted_password_callback(char *buf, int bufsiz, int verify, void *userfunc) {
	pw_aborted = 0;
	if (userfunc != NULL) {
		int (*cb)(char *, int, void *) = (int (*)(char *, int, void *))userfunc;
		if ((*cb)(buf, bufsiz, prompt_userdata)) {
			pw_aborted = 1;
			return 0;
		}
	} else {
		buf[0] = 0;
	}
	return strlen(buf);
}

static tQSL_ADIF keyf_adif = 0;

static int
tqsl_open_key_file(const char *path) {
	if (keyf_adif)
		tqsl_endADIF(&keyf_adif);
	return tqsl_beginADIF(&keyf_adif, path);
}

static int
tqsl_read_key(map<string, string>& fields) {
	TQSL_ADIF_GET_FIELD_ERROR adif_status;
	tqsl_adifFieldResults field;
	static tqsl_adifFieldDefinitions adif_fields[] = {
		 { "PUBLIC_KEY", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "PRIVATE_KEY", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "CALLSIGN", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "TQSL_CRQ_PROVIDER", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "TQSL_CRQ_PROVIDER_UNIT", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "TQSL_CRQ_ADDRESS1", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "TQSL_CRQ_ADDRESS2", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "TQSL_CRQ_CITY", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "TQSL_CRQ_STATE", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "TQSL_CRQ_POSTAL", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "TQSL_CRQ_COUNTRY", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "TQSL_CRQ_DXCC_ENTITY", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "TQSL_CRQ_QSO_NOT_BEFORE", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "TQSL_CRQ_QSO_NOT_AFTER", "", TQSL_ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		 { "DELETED", "", TQSL_ADIF_RANGE_TYPE_NONE, 200, 0, 0, NULL, NULL },
		 { "eor", "", TQSL_ADIF_RANGE_TYPE_NONE, 0, 0, 0, NULL, NULL },
		 { "", "", TQSL_ADIF_RANGE_TYPE_NONE, 0, 0, 0, NULL, NULL },
	};

	fields.clear();
	do {
		if (tqsl_getADIFField(keyf_adif, &field, &adif_status, adif_fields, notypes, &tqsl_static_alloc))
			return 1;
		if (adif_status == TQSL_ADIF_GET_FIELD_EOF)
			return 1;
		if (!strcasecmp(field.name, "eor"))
			return 0;
		if (adif_status == TQSL_ADIF_GET_FIELD_SUCCESS) {
			char *cp;
			for (cp = field.name; *cp; cp++)
				*cp = toupper(*cp);
			fields[field.name] = reinterpret_cast<char *>(field.data);
		}
	} while (adif_status == TQSL_ADIF_GET_FIELD_SUCCESS || adif_status == TQSL_ADIF_GET_FIELD_NO_NAME_MATCH);
	tQSL_Error = TQSL_ADIF_ERROR;
	return 1;
}

static void
tqsl_close_key_file(void) {
	tqsl_endADIF(&keyf_adif);
}

static int
tqsl_replace_key(const char *callsign, const char *path, map<string, string>& newfields, int (*cb)(int, const char *, void *), void *userdata) {
	char newpath[TQSL_MAX_PATH_LEN];
	char savepath[TQSL_MAX_PATH_LEN];
#ifdef _WIN32
	wchar_t* wnewpath = NULL;
#endif
	map<string, string> fields;
	vector< map<string, string> > records;
	vector< map<string, string> >::iterator it;
	EVP_PKEY *new_key = NULL, *key = NULL;
	BIO *bio = 0;
	FILE *out = 0;
	int rval = 1;

	if ((bio = BIO_new_mem_buf(reinterpret_cast<void *>(const_cast<char *>(newfields["PUBLIC_KEY"].c_str())),
				   newfields["PUBLIC_KEY"].length())) == NULL) {
		tqslTrace("tqsl_replace_key", "BIO_new_mem_buf err %s", tqsl_openssl_error());
		goto trk_end;
	}
	if ((new_key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL)) == NULL) {
		tqslTrace("tqsl_replace_key", "PEM_read_bio_PUBKEY err %s", tqsl_openssl_error());
		goto trk_end;
	}
	BIO_free(bio);
	bio = 0;
	if (tqsl_open_key_file(path)) {
		if (tQSL_Error != TQSL_SYSTEM_ERROR || tQSL_Errno != ENOENT) {
			tqslTrace("tqsl_replace_key", "error opening key file %s: %s", path, strerror(tQSL_Errno));
			return 1;
		}
		tQSL_Error = TQSL_NO_ERROR;
	}
	while (tqsl_read_key(fields) == 0) {
		vector<string>seen;
		bool match = false;
		for (size_t i = 0; i < seen.size(); i++) {
			if (seen[i] == fields["PUBLIC_KEY"]) {
				match = true;
				break;
			}
		}
		if (match) continue;			// Drop dupes
		seen.push_back(fields["PUBLIC_KEY"]);
		if ((bio = BIO_new_mem_buf(reinterpret_cast<void *>(const_cast<char *>(fields["PUBLIC_KEY"].c_str())),
					   fields["PUBLIC_KEY"].length())) == NULL) {
			tqslTrace("tqsl_replace_key", "BIO_new_mem_buf error %s", tqsl_openssl_error());
			goto trk_end;
		}
		if ((key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL)) == NULL) {
			tqslTrace("tqsl_replace_key", "Pem_read_bio_rsa_pubkey error %s", tqsl_openssl_error());
			goto trk_end;
		}
		BIO_free(bio);
		bio = NULL;
		if (EVP_PKEY_cmp(key, new_key) == 1) {
			fields["DELETED"] = "True";	// Mark as deleted
		}
		records.push_back(fields);
	}
	tqsl_close_key_file();
	if (newfields["PRIVATE_KEY"] != "")
		records.push_back(newfields);
	strncpy(newpath, path, sizeof newpath);
	strncat(newpath, ".new", sizeof newpath - strlen(newpath)-1);
	strncpy(savepath, path, sizeof savepath);
	strncat(savepath, ".save", sizeof savepath - strlen(savepath)-1);
#ifdef _WIN32
	wnewpath = utf8_to_wchar(newpath);
	if ((out = _wfopen(wnewpath, TQSL_OPEN_WRITE)) == NULL) {
		free_wchar(wnewpath);
#else
	if ((out = fopen(newpath, TQSL_OPEN_WRITE)) == NULL) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_replace_key", "open file %s: %s", newpath, strerror(tQSL_Errno));
		goto trk_end;
	}
	for (it = records.begin(); it != records.end(); it++) {
		map<string, string>::iterator mit;
		for (mit = it->begin(); mit != it->end(); mit++) {
			if (tqsl_write_adif_field(out, mit->first.c_str(), 0, (const unsigned char *)mit->second.c_str(), -1)) {
				tqslTrace("tqsl_replace_key", "error writing %s", strerror(tQSL_Errno));
#ifdef _WIN32
				free_wchar(wnewpath);
#endif
				goto trk_end;
			}
		}
		tqsl_write_adif_field(out, "eor", 0, NULL, 0);
	}
	if (fclose(out) == EOF) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_replace_key", "error closing %s", strerror(tQSL_Errno));
#ifdef _WIN32
		free_wchar(wnewpath);
#endif
		goto trk_end;
	}
	out = 0;

	/* Output file looks okay. Replace the old file with the new one. */
#ifdef _WIN32
	wchar_t* wsavepath = utf8_to_wchar(savepath);
	if (_wunlink(wsavepath) && errno != ENOENT) {
		free_wchar(wsavepath);
		free_wchar(wnewpath);
#else
	if (unlink(savepath) && errno != ENOENT) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_replace_key", "unlink file %s: %s", savepath, strerror(tQSL_Errno));
		goto trk_end;
	}
#ifdef _WIN32
	wchar_t* wpath = utf8_to_wchar(path);
	if (_wrename(wpath, wsavepath) && errno != ENOENT) {
		free_wchar(wpath);
		free_wchar(wsavepath);
		free_wchar(wnewpath);
#else
	if (rename(path, savepath) && errno != ENOENT) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_replace_key", "rename file %s->%s: %s", path, savepath, strerror(tQSL_Errno));
		goto trk_end;
	}
#ifdef _WIN32
	if (_wrename(wnewpath, wpath)) {
		free_wchar(wnewpath);
		free_wchar(wpath);
		free_wchar(wsavepath);
#else
	if (rename(newpath, path)) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_replace_key", "rename file %s->%s: %s", newpath, path, strerror(tQSL_Errno));
		goto trk_end;
	}
#ifdef _WIN32
	free_wchar(wnewpath);
	free_wchar(wpath);
	free_wchar(wsavepath);
#endif

	tqslTrace("tqsl_replace_key", "loaded private key for: %s", callsign);
	if (cb) {
		string msg = string("Loaded private key for: ") + callsign;
		(*cb)(TQSL_CERT_CB_RESULT + TQSL_CERT_CB_PKEY + TQSL_CERT_CB_LOADED, msg.c_str(), userdata);
	}
	rval = 0;
 trk_end:
	tqsl_close_key_file();
	if (out)
		fclose(out);
	if (new_key)
		EVP_PKEY_free(new_key);
	if (key)
		EVP_PKEY_free(key);
	if (bio)
		BIO_free(bio);
	return rval;
}

static int
tqsl_unlock_key(const char *pem, EVP_PKEY **keyp, const char *password, int (*cb)(char *, int, void *), void *userdata) {
	RSA *prsa = NULL;
	BIO *bio;
	int (*ssl_cb)(char *, int, int, void *) = NULL;
	void *cb_user = NULL;
	long e;
	int rval = 1;

	if ((bio = BIO_new_mem_buf(reinterpret_cast<void *>(const_cast<char *>(pem)), strlen(pem))) == NULL) {
		tqslTrace("tqsl_unlock_key", "BIO_new_mem_buf err %s", tqsl_openssl_error());
		goto err;
	}
	if (password != NULL) {
		ssl_cb = &fixed_password_callback;
		cb_user = reinterpret_cast<void *>(const_cast<char *>(password));
	} else if (cb != NULL) {
		prompt_userdata = userdata;
		ssl_cb = &prompted_password_callback;
		cb_user = reinterpret_cast<void *>(cb);
	}
	if ((prsa = PEM_read_bio_RSAPrivateKey(bio, NULL, ssl_cb, cb_user)) == NULL) {
		tqslTrace("tqsl_unlock_key", "PEM_read_bio_RSAPrivateKey err %s", tqsl_openssl_error());
		goto err;
	}
	if (keyp != NULL) {
		if ((*keyp = EVP_PKEY_new()) == NULL)
		goto err;
		EVP_PKEY_assign_RSA(*keyp, prsa);
		prsa = NULL;
	}
	rval = 0;
	goto end;
 err:
	e = ERR_peek_error();
	if ((ERR_GET_LIB(e) == ERR_LIB_EVP && ERR_GET_REASON(e) == EVP_R_BAD_DECRYPT)
		|| (ERR_GET_LIB(e) == ERR_LIB_PEM && ERR_GET_REASON(e) == PEM_R_BAD_PASSWORD_READ)
		|| (ERR_GET_LIB(e) == ERR_LIB_PKCS12 && ERR_GET_REASON(e) == PKCS12_R_PKCS12_CIPHERFINAL_ERROR)) {
		tqsl_getErrorString();	/* clear error */
		tQSL_Error = pw_aborted ? TQSL_OPERATOR_ABORT : TQSL_PASSWORD_ERROR;
		ERR_clear_error();
	} else {
		tQSL_Error = TQSL_OPENSSL_ERROR;
	}
	tqslTrace("tqsl_unlock_key", "Key read error %d", tQSL_Error);
 end:
	if (prsa != NULL)
		RSA_free(prsa);
	if (bio != NULL)
		BIO_free(bio);
	return rval;
}

static int
tqsl_find_matching_key(X509 *cert, EVP_PKEY **keyp, TQSL_CERT_REQ **crq, const char *password, int (*cb)(char *, int, void *), void *userdata) {
	char path[TQSL_MAX_PATH_LEN];
	char aro[256];
	TQSL_X509_NAME_ITEM item = { path, sizeof path, aro, sizeof aro };
	EVP_PKEY *cert_key = NULL;
	EVP_PKEY *curkey = NULL;
	int rval = 0;
	int match = 0;
	int deleted = 0;
	BIO *bio = NULL;
	map<string, string> fields;
	bool finddeleted = false;

	if (keyp != NULL)
		*keyp = NULL;

	if (!tqsl_cert_get_subject_name_entry(cert, "AROcallsign", &item)) {
		tqslTrace("tqsl_find_matching_key", "get subj name err %d", tQSL_Error);
		return 1;
	}
	tQSL_ImportSerial = ASN1_INTEGER_get(X509_get_serialNumber(cert));
	if (!tqsl_make_key_path(aro, path, sizeof path)) {
		tqslTrace("tqsl_find_matching_key", "key path err %d", tQSL_Error);
		rval = 1;
		goto end_nokey;
	}
	strncpy(ImportCall, aro, sizeof ImportCall);
 again:
	if (tqsl_open_key_file(path)) {
		/* Friendly error for file not found */
		if (tQSL_Error == TQSL_SYSTEM_ERROR) {
			if (tQSL_Errno == ENOENT) {
				snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
					"You can only open this callsign certificate by running TQSL on the computer where you created the certificate request for %s.", aro);
			} else {
				snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
					"Can't open %s: %s\nThis file is needed to open this callsign certificate.",
					aro, strerror(tQSL_Errno));
			}
			tQSL_Error = TQSL_CUSTOM_ERROR;
			tqslTrace("tqsl_find_matching_key", "opening file path err %s", tQSL_CustomError);
		}
		return 1;
	}
	if ((cert_key = X509_get_pubkey(cert)) == NULL) {
		tqslTrace("tqsl_find_matching_key", "error getting public key %s", tqsl_openssl_error());
		goto err;
	}
	if (crq != NULL) {
		if (*crq != NULL)
			tqsl_free_cert_req(*crq, 0);
		*crq = reinterpret_cast<TQSL_CERT_REQ *>(tqsl_calloc(1, sizeof(TQSL_CERT_REQ)));
	}
	while (!tqsl_read_key(fields)) {
		/* Compare the keys */
		if ((bio = BIO_new_mem_buf(reinterpret_cast<void *>(const_cast<char *>(fields["PUBLIC_KEY"].c_str())),
					   fields["PUBLIC_KEY"].length())) == NULL) {
			tqslTrace("tqsl_find_matching_key", "BIO_new_mem_buf err %s", tqsl_openssl_error());
			goto err;
		}
		if ((curkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL)) == NULL) {
			tqslTrace("tqsl_find_matching_key", "PEM_read_bio_RSA_PUBKEY err %s", tqsl_openssl_error());
			goto err;
		}
		BIO_free(bio);
		bio = NULL;
		if (EVP_PKEY_cmp(curkey, cert_key) == 1) {
			if (fields["DELETED"] == "True") {
				if (finddeleted) {
					match = 1;
					deleted = 1;
				}
			} else {
				match = 1;
			}
		}
		if (match) {
			/* We have a winner */
			if (tqsl_unlock_key(fields["PRIVATE_KEY"].c_str(), keyp, password, cb, userdata)) {
				tqslTrace("tqsl_find_matching_key", "tqsl_unlock_key err %d", tQSL_Error);
				rval = 1;
				goto end;
			}
			if (crq != NULL) {
				tQSL_Error = TQSL_BUFFER_ERROR;
				if (!safe_strncpy((*crq)->providerName, fields["TQSL_CRQ_PROVIDER"].c_str(), sizeof (*crq)->providerName))
					goto end;
				if (!safe_strncpy((*crq)->providerUnit, fields["TQSL_CRQ_PROVIDER_UNIT"].c_str(), sizeof (*crq)->providerUnit))
					goto end;
				if (!safe_strncpy((*crq)->address1, fields["TQSL_CRQ_ADDRESS1"].c_str(), sizeof (*crq)->address1))
					goto end;
				if (!safe_strncpy((*crq)->address2, fields["TQSL_CRQ_ADDRESS2"].c_str(), sizeof (*crq)->address2))
					goto end;
				if (!safe_strncpy((*crq)->city, fields["TQSL_CRQ_CITY"].c_str(), sizeof (*crq)->city))
					goto end;
				if (!safe_strncpy((*crq)->state, fields["TQSL_CRQ_STATE"].c_str(), sizeof (*crq)->state))
					goto end;
				if (!safe_strncpy((*crq)->postalCode, fields["TQSL_CRQ_POSTAL"].c_str(), sizeof (*crq)->postalCode))
					goto end;
				if (!safe_strncpy((*crq)->country, fields["TQSL_CRQ_COUNTRY"].c_str(), sizeof (*crq)->country))
					goto end;
				tQSL_Error = 0;
			}
			rval = 0;
			break;
		}
	}
	if (match)
		goto end;
	if (!finddeleted) {
		finddeleted = true;
		tqsl_close_key_file();
		goto again;
	}
	tqslTrace("tqsl_find_matching_key", "No matching private key found");
	rval = 1;
	tQSL_Error = TQSL_CERT_NOT_FOUND;
	strncpy(tQSL_ImportCall, ImportCall, sizeof tQSL_ImportCall);
	goto end;
 err:
	rval = 1;
	tQSL_Error = TQSL_OPENSSL_ERROR;
 end:
	tqsl_close_key_file();
	if (deleted) {
		int savedErr = tQSL_Error;
		tqsl_clear_deleted(aro, path, cert_key);
		tQSL_Error = savedErr;
	}
 end_nokey:
	if (curkey != NULL)
		EVP_PKEY_free(curkey);
	if (bio != NULL)
		BIO_free(bio);
	if (cert_key != NULL)
		EVP_PKEY_free(cert_key);
//	if (in != NULL)
//		fclose(in);
	if (rval == 0) {
		strncpy(tQSL_ImportCall, ImportCall, sizeof tQSL_ImportCall);
	}
	return rval;
}

static int
tqsl_make_key_list(vector< map<string, string> > & keys) {
	keys.clear();

	string path = tQSL_BaseDir;
#ifdef _WIN32
	path += "\\keys";
	wchar_t* wpath = utf8_to_wchar(path.c_str());
	MKDIR(wpath, 0700);
#else
	path += "/keys";
	MKDIR(path.c_str(), 0700);
#endif

#ifdef _WIN32
	WDIR *dir = wopendir(wpath);
	free_wchar(wpath);
#else
	DIR *dir = opendir(path.c_str());
#endif
	if (dir == NULL) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_make_key_list", "Opendir %s error %s", path.c_str(), strerror(errno));
		return 1;
	}
#ifdef _WIN32
	struct wdirent *ent;
#else
	struct dirent *ent;
#endif
	int rval = 0;
	int savedError = 0;
	int savedErrno = 0;
	char *savedFile = NULL;

#ifdef _WIN32
	while ((ent = wreaddir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;
		if (wcsstr(ent->d_name, L".save") || wcsstr(ent->d_name, L".new"))
			continue;
		char dname[TQSL_MAX_PATH_LEN];
		wcstombs(dname, ent->d_name, TQSL_MAX_PATH_LEN);
		string filename = path + "\\" + dname;
#else
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.')
			continue;
		if (strstr(ent->d_name, ".save") || strstr(ent->d_name, ".new"))
			continue;
		string filename = path + "/" + ent->d_name;
#endif
		char fixcall[256];
#ifdef _WIN32
		struct _stat32 s;
		wchar_t* wfilename = utf8_to_wchar(filename.c_str());
		if (_wstat32(wfilename, &s) == 0) {
			if (S_ISDIR(s.st_mode)) {
				free_wchar(wfilename);
				continue;		// If it's a directory, skip it.
			}
		}
#else
		struct stat s;
		if (stat(filename.c_str(), &s) == 0) {
			if (S_ISDIR(s.st_mode))
				continue;		// If it's a directory, skip it.
		}
#endif
		if (!tqsl_open_key_file(filename.c_str())) {
			vector<string>seen;
			map<string, string> fields;
			while (!tqsl_read_key(fields)) {
				if (fields["DELETED"] == "True")
					continue;		// Skip this one
				bool match = false;
				for (size_t i = 0; i < seen.size(); i++) {
					if (seen[i] == fields["PUBLIC_KEY"]) {
						match = true;
						break;
					}
				}
				if (match) continue;
				seen.push_back(fields["PUBLIC_KEY"]);
				if (tqsl_clean_call(fields["CALLSIGN"].c_str(), fixcall, sizeof fixcall)) {
					rval = 1;
					savedError = tQSL_Error;
					savedErrno = tQSL_Errno;
					if (savedFile)
						free(savedFile);
					savedFile = strdup(tQSL_ErrorFile);
					continue;	// Keep looking for keys
				}
#ifdef _WIN32
				wchar_t* wfixcall = utf8_to_wchar(fixcall);
				if (wcscmp(wfixcall, ent->d_name)) {
					free_wchar(wfixcall);
#else
				if (strcasecmp(fixcall, ent->d_name)) {
#endif
					continue;
				}
				keys.push_back(fields);
			}
			tqsl_close_key_file();
		} else {
			rval = 1;			// Unable to open - remember that
			savedErrno = tQSL_Errno;
			savedError = tQSL_Error;
			if (savedFile)
				free(savedFile);
			savedFile = strdup(tQSL_ErrorFile);
		}
	}
#ifdef _WIN32
	_wclosedir(dir);
#else
	closedir(dir);
#endif
	if (rval) {
		tQSL_Error = savedError;
		tQSL_Errno = savedErrno;
		if (savedFile) {
			strncpy(tQSL_ErrorFile, savedFile, sizeof tQSL_ErrorFile);
			free(savedFile);
		}
		tqslTrace("tqsl_make_key_list", "error %s %s", tQSL_ErrorFile, strerror(tQSL_Errno));
	}
	return rval;
}

static int
tqsl_get_cert_ext(X509 *cert, const char *ext, unsigned char *userbuf, int *buflen, int *crit) {
	int i, n, datasiz;
	X509_EXTENSION *xe;
	char buf[256];
	ASN1_OBJECT *obj;
	ASN1_OCTET_STRING *data;

	if (tqsl_init())
		return 1;
	if (cert == NULL || ext == NULL || userbuf == NULL || buflen == NULL) {
		tqslTrace("tqsl_get_cert_ext", "arg error cert=0x%lx, ext=0x%lx userbuf=0x%lx, buflen=0x%lx crit=0x%lx", cert, ext, userbuf, buflen, crit);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	n = X509_get_ext_count(cert);
	for (i = 0; i < n; i++) {
		xe = X509_get_ext(cert, i);
		if (xe == NULL) {
			tqslTrace("tqsl_get_cert_ext", "X509_get_ext error %s", tqsl_openssl_error());
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 1;
		}
		buf[0] = '\0';
		obj = X509_EXTENSION_get_object(xe);
		if (obj)
			OBJ_obj2txt(buf, sizeof buf, obj, 0);
		if (strcmp(buf, ext))
			continue;
		/* This is the desired extension */
		data = X509_EXTENSION_get_data(xe);
		if (data != NULL) {
			datasiz = ASN1_STRING_length(data);
			if (datasiz > *buflen-1) {
				tqslTrace("tqsl_get_cert_ext", "buffer len %d needed %d", *buflen, datasiz);
				tQSL_Error = TQSL_BUFFER_ERROR;
				return 1;
			}
			*buflen = datasiz;
			if (datasiz)
				memcpy(userbuf, ASN1_STRING_get0_data(data), datasiz);
			userbuf[datasiz] = '\0';
		}
		if (crit != NULL)
			*crit = X509_EXTENSION_get_critical(xe);
		return 0;
	}
	snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
		"Certificate Extension not found: %s", ext);
	tQSL_Error = TQSL_CUSTOM_ERROR;
	if (strcmp(ext,  "supercededCertificate"))
		tqslTrace("tqsl_get_cert_ext", "Err %s", tQSL_CustomError);
	return 1;
}

CLIENT_STATIC int
tqsl_get_asn1_date(const ASN1_TIME *tm, tQSL_Date *date) {
	char *v;
	int i;

	i = tm->length;
	v = reinterpret_cast<char *>(tm->data);

	if (i >= 14) {
		for (i = 0; i < 12; i++)
			if ((v[i] > '9') || (v[i] < '0')) goto err;
		date->year =  (v[0]-'0')*1000+(v[1]-'0')*100 + (v[2]-'0')*10+(v[3]-'0');
		date->month = (v[4]-'0')*10+(v[5]-'0');
		if ((date->month > 12) || (date->month < 1)) goto err;
		date->day = (v[6]-'0')*10+(v[7]-'0');
	} else if (i < 12) {
		goto err;
	} else {
		for (i = 0; i < 10; i++)
			if ((v[i] > '9') || (v[i] < '0')) goto err;
		date->year = (v[0]-'0')*10+(v[1]-'0');
		if (date->year < 50) date->year+=100;
		date->year += 1900;
		date->month = (v[2]-'0')*10+(v[3]-'0');
		if ((date->month > 12) || (date->month < 1)) goto err;
		date->day = (v[4]-'0')*10+(v[5]-'0');
	}
	return 0;

 err:
	tqslTrace("tqsl_get_asn1_date", "invalid date");
	tQSL_Error = TQSL_INVALID_DATE;
	return 1;
}

static char *
tqsl_sign_base64_data(tQSL_Cert cert, char *b64data) {
	int len;
	static unsigned char sig[256];
	int siglen = sizeof sig;

	if (b64data && !strncmp(b64data, "-----", 5)) {
		b64data = strchr(b64data, '\n');
		if (b64data == NULL)
			return NULL;
		b64data++;
	}
	len = sizeof tqsl_static_buf;
	if (tqsl_decodeBase64(b64data, tqsl_static_buf, &len))
		return NULL;
	if (tqsl_signDataBlock(cert, tqsl_static_buf, len, sig, &siglen))
		return NULL;
	if (tqsl_encodeBase64(sig, siglen, reinterpret_cast<char *>(tqsl_static_buf), sizeof tqsl_static_buf))
		return NULL;
	return reinterpret_cast<char *>(tqsl_static_buf);
}

static bool
safe_strncpy(char *dest, const char *src, int size) {
	strncpy(dest, src, size);
	dest[size-1] = 0;
	return ((static_cast<int>((strlen(src))) < size));
}

static string
tqsl_cert_status_filename(const char *f = "cert_status.xml") {
	string s = tQSL_BaseDir;
#ifdef _WIN32
	s += "\\";
#else
	s += "/";
#endif
	s += f;
	return s;
}

static int
tqsl_load_cert_status_data(XMLElement &xel) {
	int status = xel.parseFile(tqsl_cert_status_filename().c_str());
	if (status) {
		if (errno == ENOENT) {		// No file is OK
			tqslTrace("tqsl_load_cert_status_data", "FNF");
			return 0;
		}
		strncpy(tQSL_ErrorFile, tqsl_cert_status_filename().c_str(), sizeof tQSL_ErrorFile);
		if (status == XML_PARSE_SYSTEM_ERROR) {
			tQSL_Error = TQSL_FILE_SYSTEM_ERROR;
			tQSL_Errno = errno;
			tqslTrace("tqsl_load_cert_status_data", "open error %s: %s", tqsl_cert_status_filename().c_str(), strerror(tQSL_Errno));
		} else {
			tqslTrace("tqsl_load_cert_status_data", "syntax error %s", tqsl_cert_status_filename().c_str());
			tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
		}
		return 1;
	}
	return status;
}

static int
tqsl_dump_cert_status_data(XMLElement &xel) {
	ofstream out;
	string fn = tqsl_cert_status_filename();

	out.exceptions(std::ios::failbit | std::ios::eofbit | std::ios::badbit);
	try {
#ifdef _WIN32
		wchar_t* wfn = utf8_to_wchar(fn.c_str());
		out.open(wfn);
		free_wchar(wfn);
#else
		out.open(fn.c_str());
#endif
		out << xel << endl;
		out.close();
	}
	catch(exception& x) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
			"Error writing certificate status file (%s): %s/%s",
			fn.c_str(), x.what(), strerror(errno));
		tqslTrace("tqsl_dump_cert_status_data", "write error %s", tQSL_CustomError);
		return 1;
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCertificateStatus(long serial) {
	XMLElement top_el;
	if (tqsl_load_cert_status_data(top_el))
		return TQSL_CERT_STATUS_UNK;
	XMLElement sfile;
	if (top_el.getFirstElement(sfile)) {
		XMLElement cd;
		bool ok = sfile.getFirstElement("Cert", cd);
		while (ok && cd.getElementName() == "Cert") {
			pair<string, bool> s = cd.getAttribute("serial");
			if (s.second && strtol(s.first.c_str(), NULL, 10) == serial) {
				XMLElement xs;
				if (cd.getFirstElement("status", xs)) {
					if (!strcasecmp(xs.getText().c_str(), "Bad serial"))
						return TQSL_CERT_STATUS_INV;
					else if (!strcasecmp(xs.getText().c_str(), "Superceded"))
						return TQSL_CERT_STATUS_SUP;
					else if (!strcasecmp(xs.getText().c_str(), "Expired"))
						return TQSL_CERT_STATUS_EXP;
					else if (!strcasecmp(xs.getText().c_str(), "Unrevoked"))
						return TQSL_CERT_STATUS_OK;
					else
						return TQSL_CERT_STATUS_UNK;
				}
			}
			ok = sfile.getNextElement(cd);
		}
	}
	return TQSL_CERT_STATUS_UNK;
}

DLLEXPORT int CALLCONVENTION
tqsl_setCertificateStatus(long serial, const char *status) {
	if (status == NULL) {
		tqslTrace("tqsl_setCertificateStatus", "status=null");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	char sstr[32];
	snprintf(sstr, sizeof sstr, "%ld", serial);

	XMLElement top_el;
	int stat = tqsl_load_cert_status_data(top_el);
	if (stat == TQSL_FILE_SYSTEM_ERROR) {
		tqslTrace("tqsl_setCertificateStatus", "error %d", tQSL_Error);
		return 1;
	}
	XMLElement sfile;
	if (!top_el.getFirstElement(sfile))
		sfile.setElementName("CertStatus");

	XMLElementList& ellist = sfile.getElementList();
	bool exists = false;
	XMLElementList::iterator ep;
	for (ep = ellist.find("Cert"); ep != ellist.end(); ep++) {
		if (ep->first != "Cert")
			break;
		pair<string, bool> rval = ep->second->getAttribute("serial");
		if (rval.second && strtol(rval.first.c_str(), NULL, 10) == serial) {
			exists = true;
			break;
		}
	}

	XMLElement *cs = new XMLElement("Cert");
	cs->setPretext("\n  ");
	XMLElement *se = new XMLElement;
	se->setPretext(cs->getPretext() + "  ");
	se->setElementName("status");
	se->setText(status);
	cs->addElement(se);

	cs->setAttribute("serial", sstr);
	cs->setText("\n  ");

	if (exists)
		ellist.erase(ep);

	sfile.addElement(cs);
	sfile.setText("\n");
	return tqsl_dump_cert_status_data(sfile);
}

static int
tqsl_clear_deleted(const char *callsign, const char *path, EVP_PKEY *cert_key) {
	char newpath[TQSL_MAX_PATH_LEN];
	char savepath[TQSL_MAX_PATH_LEN];
#ifdef _WIN32
	wchar_t* wnewpath = NULL;
#endif
	map<string, string> fields;
	vector< map<string, string> > records;
	vector< map<string, string> >::iterator it;
	EVP_PKEY *new_key = NULL, *key = NULL;
	BIO *bio = 0;
	FILE *out = 0;
	int rval = 1;

	if (tqsl_open_key_file(path)) {
		if (tQSL_Error != TQSL_SYSTEM_ERROR || tQSL_Errno != ENOENT) {
			tqslTrace("tqsl_clear_deleted", "error opening key file %s: %s", path, strerror(tQSL_Errno));
			return 1;
		}
		tQSL_Error = TQSL_NO_ERROR;
	}
	while (tqsl_read_key(fields) == 0) {
		if ((bio = BIO_new_mem_buf(reinterpret_cast<void *>(const_cast<char *>(fields["PUBLIC_KEY"].c_str())),
					   fields["PUBLIC_KEY"].length())) == NULL) {
			tqslTrace("tqsl_clear_deleted", "BIO_new_mem_buf error %s", tqsl_openssl_error());
			goto trk_end;
		}
		if ((key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL)) == NULL) {
			tqslTrace("tqsl_clear_deleted", "Pem_read_bio_rsa_pubkey error %s", tqsl_openssl_error());
			goto trk_end;
		}
		BIO_free(bio);
		bio = NULL;
		if (EVP_PKEY_cmp(key, cert_key) == 1) {
			fields["DELETED"] = "False";
		}
		records.push_back(fields);
	}
	tqsl_close_key_file();
	strncpy(newpath, path, sizeof newpath);
	strncat(newpath, ".new", sizeof newpath - strlen(newpath)-1);
	strncpy(savepath, path, sizeof savepath);
	strncat(savepath, ".save", sizeof savepath - strlen(savepath)-1);
#ifdef _WIN32
	wnewpath = utf8_to_wchar(newpath);
	if ((out = _wfopen(wnewpath, TQSL_OPEN_WRITE)) == NULL) {
		free_wchar(wnewpath);
#else
	if ((out = fopen(newpath, TQSL_OPEN_WRITE)) == NULL) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_clear_deleted", "open file %s: %s", newpath, strerror(tQSL_Errno));
		goto trk_end;
	}
	for (it = records.begin(); it != records.end(); it++) {
		map<string, string>::iterator mit;
		for (mit = it->begin(); mit != it->end(); mit++) {
			if (tqsl_write_adif_field(out, mit->first.c_str(), 0, (const unsigned char *)mit->second.c_str(), -1)) {
				tqslTrace("tqsl_clear_deleted", "error writing %s", strerror(tQSL_Errno));
#ifdef _WIN32
				free_wchar(wnewpath);
#endif
				goto trk_end;
			}
		}
		tqsl_write_adif_field(out, "eor", 0, NULL, 0);
	}
	if (fclose(out) == EOF) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_clear_deleted", "error closing %s", strerror(tQSL_Errno));
#ifdef _WIN32
		free_wchar(wnewpath);
#endif
		goto trk_end;
	}
	out = 0;

	/* Output file looks okay. Replace the old file with the new one. */
#ifdef _WIN32
	wchar_t* wsavepath = utf8_to_wchar(savepath);
	if (_wunlink(wsavepath) && errno != ENOENT) {
		free_wchar(wsavepath);
		free_wchar(wnewpath);
#else
	if (unlink(savepath) && errno != ENOENT) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_clear_deleted", "unlink file %s: %s", savepath, strerror(tQSL_Errno));
		goto trk_end;
	}
#ifdef _WIN32
	wchar_t* wpath = utf8_to_wchar(path);
	if (_wrename(wpath, wsavepath) && errno != ENOENT) {
		free_wchar(wpath);
		free_wchar(wsavepath);
		free_wchar(wnewpath);
#else
	if (rename(path, savepath) && errno != ENOENT) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_clear_deleted", "rename file %s->%s: %s", path, savepath, strerror(tQSL_Errno));
		goto trk_end;
	}
#ifdef _WIN32
	if (_wrename(wnewpath, wpath)) {
		free_wchar(wnewpath);
		free_wchar(wpath);
		free_wchar(wsavepath);
#else
	if (rename(newpath, path)) {
#endif
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_clear_deleted", "rename file %s->%s: %s", newpath, path, strerror(tQSL_Errno));
		goto trk_end;
	}
#ifdef _WIN32
	free_wchar(wnewpath);
	free_wchar(wpath);
	free_wchar(wsavepath);
#endif

	rval = 0;
 trk_end:
	tqsl_close_key_file();
	if (out)
		fclose(out);
	if (new_key)
		EVP_PKEY_free(new_key);
	if (key)
		EVP_PKEY_free(key);
	if (bio)
		BIO_free(bio);
	return rval;
}

static int
tqsl_key_exists(const char *callsign, EVP_PKEY *cert_key) {
	map<string, string> fields;
	vector< map<string, string> >::iterator it;
	EVP_PKEY *key = NULL;
	BIO *bio = 0;
	int rval = 0;
	char path[TQSL_MAX_PATH_LEN];

	if (!tqsl_make_key_path(callsign, path, sizeof path)) {
		tqslTrace("tqsl_createCertRequest", "make_key_path error %d", errno);
		return 0;
	}

	if (tqsl_open_key_file(path)) {
		return 0;
	}

	while (tqsl_read_key(fields) == 0) {
		if ((bio = BIO_new_mem_buf(reinterpret_cast<void *>(const_cast<char *>(fields["PUBLIC_KEY"].c_str())),
					   fields["PUBLIC_KEY"].length())) == NULL) {
			tqslTrace("tqsl_clear_deleted", "BIO_new_mem_buf error %s", tqsl_openssl_error());
			goto trk_end;
		}
		if ((key = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL)) == NULL) {
			tqslTrace("tqsl_clear_deleted", "Pem_read_bio_rsa_pubkey error %s", tqsl_openssl_error());
			goto trk_end;
		}
		BIO_free(bio);
		bio = NULL;
		if (EVP_PKEY_cmp(key, cert_key) == 1) {
			rval = 1;
		}
	}
 trk_end:
	tqsl_close_key_file();
	if (key)
		EVP_PKEY_free(key);
	if (bio)
		BIO_free(bio);
	return rval;
}
/** Save the json results for a given callsign location Detail. */
DLLEXPORT int CALLCONVENTION
tqsl_saveCallsignLocationInfo(const char *callsign, const char *json) {
	FILE *out;

	if (callsign == NULL || json == NULL) {
		tqslTrace("tqsl_saveCallsinLocationInfo", "arg error callsign=0x%lx, json=0x%lx", callsign, json);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	char fixcall[256];
	char path[TQSL_MAX_PATH_LEN];
	size_t size = sizeof path;

	tqsl_clean_call(callsign, fixcall, sizeof fixcall);
	strncpy(path, tQSL_BaseDir, size);
#ifdef _WIN32
	strncat(path, "\\", size - strlen(path));
#else
	strncat(path, "/", size - strlen(path));
#endif
	strncat(path, fixcall, size - strlen(path));
	strncat(path, ".json", size - strlen(path));
	/* Try opening the output stream */

#ifdef _WIN32
	wchar_t* wfilename = utf8_to_wchar(path);
	if ((out = _wfopen(wfilename, TQSL_OPEN_WRITE)) == NULL) {
		free_wchar(wfilename);
#else
	if ((out = fopen(path, TQSL_OPEN_WRITE)) == NULL) {
#endif
		strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
		tqslTrace("tqsl_saveCallsignLocationInfo", "Open file - system error %s", strerror(errno));
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		return 1;
	}
#ifdef _WIN32
	free_wchar(wfilename);
#endif
	if (fputs(json, out) == EOF) {
		strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
		tqslTrace("tqsl_createCertRequest", "Write request file - system error %s", strerror(errno));
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		return 1;
	}
	if (fclose(out) == EOF) {
		strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_saveCallsignLocationInfo", "write error %d", errno);
		return 1;
	}
	return 0;
}

/** Retrieve the json results for a given callsign location Detail. */
DLLEXPORT int CALLCONVENTION
tqsl_getCallsignLocationInfo(const char *callsign, char **buf) {
	FILE *in;
	static void* mybuf = NULL;
	static size_t bufsize = 0;

	if (bufsize == 0) {
		bufsize = 4096;
		mybuf = tqsl_malloc(bufsize);
	}
	if (callsign == NULL || buf == NULL) {
		tqslTrace("tqsl_getCallsinLocationInfo", "arg error callsign=0x%lx, buf=0x%lx", callsign, buf);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	char fixcall[256];
	char path[TQSL_MAX_PATH_LEN];
	size_t size = sizeof path;

	tqsl_clean_call(callsign, fixcall, sizeof fixcall);
	strncpy(path, tQSL_BaseDir, size);
#ifdef _WIN32
	strncat(path, "\\", size - strlen(path));
#else
	strncat(path, "/", size - strlen(path));
#endif
	strncat(path, fixcall, size - strlen(path));
	strncat(path, ".json", size - strlen(path));

	size_t buflen = bufsize;
#ifdef _WIN32
	struct _stat32 s;
	wchar_t* wfilename = utf8_to_wchar(path);
	if (_wstat32(wfilename, &s) == 0) {
		buflen = s.st_size + 512;
	}
	if ((in = _wfopen(wfilename, TQSL_OPEN_READ)) == NULL) {
		free_wchar(wfilename);
#else
	struct stat s;
	if (stat(path, &s) == 0) {
		buflen = s.st_size + 512;
	}
	if ((in = fopen(path, TQSL_OPEN_READ)) == NULL) {
#endif
		strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
		tqslTrace("tqsl_getCallsignLocationInfo", "Open file - system error %s", strerror(errno));
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		return 1;
	}
#ifdef _WIN32
	free_wchar(wfilename);
#endif
	if (buflen > bufsize) {
		bufsize = buflen + 512;
		mybuf = tqsl_realloc(mybuf, bufsize);
	}
	*buf = reinterpret_cast<char *>(mybuf);
	size_t len;
	if ((len = fread(mybuf, 1, buflen, in)) == 0) {
		strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
		tqslTrace("tqsl_getCallsignLocationInformation", "Read file - system error %s", strerror(errno));
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		return 1;
	}
	if (fclose(in) == EOF) {
		strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		tqslTrace("tqsl_getCallsignLocationInformation", "read error %d", errno);
		return 1;
	}
	if (len < (size_t)buflen) {
		char *t = reinterpret_cast<char *>(mybuf);
		t[len] = '\0';
	}
	return 0;
}


