/***************************************************************************
                          openssl_cert.h  -  description
                             -------------------
    begin                : Tue May 14 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef OPENSSL_CERT_H
#define OPENSSL_CERT_H

/** \file
  * OpenSSL X509 certificate interface functions.
  */

#include <openssl/opensslv.h>
#if (OPENSSL_VERSION_NUMBER == 0x10000003L)
/* broken header file - fix by override */
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
		   void (*free_func)(void*), int ex_tag, // NOLINT(readability/casting)
		   int ex_class);
void *ASN1_seq_unpack(const unsigned char *buf, int len,
		      d2i_of_void *d2i, void (*free_func)(void* dummy));
unsigned char *ASN1_seq_pack(void *safes, i2d_of_void *i2d,
			     unsigned char **buf, int *len);
#ifdef __cplusplus
}
#endif
#endif /* buggy openssl header */
#include <openssl/x509.h>

#undef CLIENT_STATIC
#ifndef LOTW_SERVER
#define CLIENT_STATIC static
#else
#define CLIENT_STATIC
#endif

typedef STACK_OF(X509) TQSL_X509_STACK;

typedef struct {
	char *name_buf;
	int name_buf_size;
	char *value_buf;
	int value_buf_size;
} TQSL_X509_NAME_ITEM;

namespace tqsllib {

typedef enum { ROOTCERT = 0, CACERT, USERCERT } certtype;

int tqsl_import_cert(const char *cert, certtype type, int(*cb)(int, const char *, void *), void *);

int tqsl_get_pem_serial(const char *pem, long *serial);

}  // namespace tqsllib

#if defined(LOTW_SERVER) || defined(OPENSSL_CERT_SOURCE)

#ifdef __cplusplus
extern "C" {
#endif

/// Loads a stack of certificates from the caller-supplied BIO
/** See the OpenSSL documentation for background on BIO operations.
  *
  * Returns a pointer to an OpenSSL X509 stack, as used by
  * tqsl_ssl_verify_cert()
  */
CLIENT_STATIC TQSL_X509_STACK *tqsl_ssl_load_certs_from_BIO(BIO *in);
/// Loads a stack of certificates from a file
/** See tqsl_ssl_load_certs_from_BIO()
  */
CLIENT_STATIC TQSL_X509_STACK *tqsl_ssl_load_certs_from_file(const char *filename);

/// Verifies a certificate using stacks of certificates
/** The user supplies the X509 certificate to verify (the test certificate)
  * along with two stacks of certificates. The \c cacerts stack is a list
  * of certificates, one of which was used to sign the test certificate.
  * The \c rootcerts are considered "trusted." One of them must have been used
  * to sign either the test certificate itself or the CA cert that signed
  * the test certificate.
  *
  * Returns NULL if the test certificate is valid, otherwise returns an error message.
  */
CLIENT_STATIC const char *tqsl_ssl_verify_cert(X509 *cert, TQSL_X509_STACK *cacerts, TQSL_X509_STACK *rootcerts, int purpose,
	int (*cb)(int ok, X509_STORE_CTX *ctx), TQSL_X509_STACK **chain = 0);

/// Retrieve a name entry from an X509 name object by name
CLIENT_STATIC int tqsl_get_name_entry(X509_NAME *name, const char *obj_name, TQSL_X509_NAME_ITEM *name_item);

/// Retrieve a name entry from an X509 cert's subject name by name
CLIENT_STATIC int tqsl_cert_get_subject_name_entry(X509 *cert, const char *obj_name, TQSL_X509_NAME_ITEM *name_item);

/// Convert an ASN date
CLIENT_STATIC int tqsl_get_asn1_date(const ASN1_TIME *tm, tQSL_Date *date);

/// Filter a list (stack) of certs based on (optional) call sign, qso date and issuer criteria
/** Returns a (possibly empty) stack of certificates that match the criteria. Returns NULL
  * on error.
  *
  * The returned stack contains \em copies of the certs from the input stack. The input
  * stack is not altered.
  */
CLIENT_STATIC TQSL_X509_STACK *tqsl_filter_cert_list(TQSL_X509_STACK *sk, const char *callsign,
	int dxcc, const tQSL_Date *date, const TQSL_PROVIDER *issuer, int isvalid);

CLIENT_STATIC EVP_PKEY *tqsl_new_rsa_key(int nbits);

CLIENT_STATIC int tqsl_store_cert(const char *pem, X509 *cert, const char *certfile,
	int type, bool force, int (*cb)(int, const char *, void *), void *);

CLIENT_STATIC int tqsl_write_adif_field(FILE *fp, const char *fieldname, char type, const unsigned char *value, int len);
CLIENT_STATIC int tqsl_bio_write_adif_field(BIO *bio, const char *fieldname, char type, const unsigned char *value, int len);

#ifdef __cplusplus
}
#endif

#endif /* defined(LOTW_SERVER) || defined(OPENSSL_CERT_SOURCE) */

#endif /* OPENSSL_CERT_H */
