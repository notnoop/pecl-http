/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2006, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#define HTTP_WANT_SAPI
#define HTTP_WANT_CURL
#include "php_http.h"

#ifdef HTTP_HAVE_CURL

#include "php_http_api.h"
#include "php_http_request_api.h"
#include "php_http_url_api.h"

#ifdef ZEND_ENGINE_2
#	include "php_http_request_object.h"
#endif

/* {{{ cruft for thread safe SSL crypto locks */
#if defined(ZTS) && defined(HTTP_HAVE_SSL)
#	ifdef PHP_WIN32
#		define HTTP_NEED_OPENSSL_TSL
#		include <openssl/crypto.h>
#	else /* !PHP_WIN32 */
#		if defined(HTTP_HAVE_OPENSSL)
#			if defined(HAVE_OPENSSL_CRYPTO_H)
#				define HTTP_NEED_OPENSSL_TSL
#				include <openssl/crypto.h>
#			else
#				warning \
					"libcurl was compiled with OpenSSL support, but configure could not find " \
					"openssl/crypto.h; thus no SSL crypto locking callbacks will be set, which may " \
					"cause random crashes on SSL requests"
#			endif
#		elif defined(HTTP_HAVE_GNUTLS)
#			if defined(HAVE_GCRYPT_H)
#				define HTTP_NEED_GNUTLS_TSL
#				include <gcrypt.h>
#			else
#				warning \
					"libcurl was compiled with GnuTLS support, but configure could not find " \
					"gcrypt.h; thus no SSL crypto locking callbacks will be set, which may " \
					"cause random crashes on SSL requests"
#			endif
#		else
#			warning \
				"libcurl was compiled with SSL support, but configure could not determine which" \
				"library was used; thus no SSL crypto locking callbacks will be set, which may " \
				"cause random crashes on SSL requests"
#		endif /* HTTP_HAVE_OPENSSL || HTTP_HAVE_GNUTLS */
#	endif /* PHP_WIN32 */
#endif /* ZTS && HTTP_HAVE_SSL */

#ifdef HTTP_NEED_OPENSSL_TSL
static MUTEX_T *http_openssl_tsl = NULL;

static void http_openssl_thread_lock(int mode, int n, const char * file, int line)
{
	if (mode & CRYPTO_LOCK) {
		tsrm_mutex_lock(http_openssl_tsl[n]);
	} else {
		tsrm_mutex_unlock(http_openssl_tsl[n]);
	}
}

static ulong http_openssl_thread_id(void)
{
	return (ulong) tsrm_thread_id();
}
#endif
#ifdef HTTP_NEED_GNUTLS_TSL
static int http_gnutls_mutex_create(void **m)
{
	if (*((MUTEX_T *) m) = tsrm_mutex_alloc()) {
		return SUCCESS;
	} else {
		return FAILURE;
	}
}

static int http_gnutls_mutex_destroy(void **m)
{
	tsrm_mutex_free(*((MUTEX_T *) m));
	return SUCCESS;
}

static int http_gnutls_mutex_lock(void **m)
{
	return tsrm_mutex_lock(*((MUTEX_T *) m));
}

static int http_gnutls_mutex_unlock(void **m)
{
	return tsrm_mutex_unlock(*((MUTEX_T *) m));
}

static struct gcry_thread_cbs http_gnutls_tsl = {
	GCRY_THREAD_OPTION_USER,
	NULL,
	http_gnutls_mutex_create,
	http_gnutls_mutex_destroy,
	http_gnutls_mutex_lock,
	http_gnutls_mutex_unlock
};
#endif
/* }}} */

/* {{{ MINIT */
PHP_MINIT_FUNCTION(http_request)
{
#ifdef HTTP_NEED_OPENSSL_TSL
	int i, c = CRYPTO_num_locks();
	
	http_openssl_tsl = malloc(c * sizeof(MUTEX_T));
	
	for (i = 0; i < c; ++i) {
		http_openssl_tsl[i] = tsrm_mutex_alloc();
	}
	
	CRYPTO_set_id_callback(http_openssl_thread_id);
	CRYPTO_set_locking_callback(http_openssl_thread_lock);
#endif
#ifdef HTTP_NED_GNUTLS_TSL
	gcry_control(GCRYCTL_SET_THREAD_CBS, &http_gnutls_tsl);
#endif

	if (CURLE_OK != curl_global_init(CURL_GLOBAL_ALL)) {
		return FAILURE;
	}
	
	HTTP_LONG_CONSTANT("HTTP_AUTH_BASIC", CURLAUTH_BASIC);
	HTTP_LONG_CONSTANT("HTTP_AUTH_DIGEST", CURLAUTH_DIGEST);
	HTTP_LONG_CONSTANT("HTTP_AUTH_NTLM", CURLAUTH_NTLM);
	HTTP_LONG_CONSTANT("HTTP_AUTH_ANY", CURLAUTH_ANY);
	
	HTTP_LONG_CONSTANT("HTTP_VERSION_NONE", CURL_HTTP_VERSION_NONE);
	HTTP_LONG_CONSTANT("HTTP_VERSION_1_0", CURL_HTTP_VERSION_1_0);
	HTTP_LONG_CONSTANT("HTTP_VERSION_1_1", CURL_HTTP_VERSION_1_1);

	return SUCCESS;
}
/* }}} */

/* {{{ MSHUTDOWN */
PHP_MSHUTDOWN_FUNCTION(http_request)
{
#ifdef HTTP_NEED_OPENSSL_TSL
	CRYPTO_set_id_callback(http_openssl_thread_id);
	CRYPTO_set_locking_callback(http_openssl_thread_lock);
#endif
	curl_global_cleanup();
#ifdef HTTP_NEED_OPENSSL_TSL
	if (http_openssl_tsl) {
		int i, c = CRYPTO_num_locks();
			
		CRYPTO_set_id_callback(NULL);
		CRYPTO_set_locking_callback(NULL);
			
		for (i = 0; i < c; ++i) {
			tsrm_mutex_free(http_openssl_tsl[i]);
		}
			
		free(http_openssl_tsl);
		http_openssl_tsl = NULL;
	}
#endif
	return SUCCESS;
}
/* }}} */

/* {{{ MACROS */
#ifndef HAVE_CURL_EASY_STRERROR
#	define curl_easy_strerror(dummy) "unknown error"
#endif

#define HTTP_CURL_INFO(I) \
	{ \
		char *N = #I; \
		HTTP_CURL_INFO_EX(I, N+lenof("CURLINFO_")); \
	}
#define HTTP_CURL_INFO_EX(I, X) \
	switch (I & ~CURLINFO_MASK) \
	{ \
		case CURLINFO_STRING: \
		{ \
			char *c; \
			if (CURLE_OK == curl_easy_getinfo(request->ch, I, &c)) { \
				char *key = estrndup(X, strlen(X)); \
				add_assoc_string(&array, pretty_key(key, strlen(X), 0, 0), c ? c : "", 1); \
				efree(key); \
			} \
		} \
		break; \
\
		case CURLINFO_DOUBLE: \
		{ \
			double d; \
			if (CURLE_OK == curl_easy_getinfo(request->ch, I, &d)) { \
				char *key = estrndup(X, strlen(X)); \
				add_assoc_double(&array, pretty_key(key, strlen(X), 0, 0), d); \
				efree(key); \
			} \
		} \
		break; \
\
		case CURLINFO_LONG: \
		{ \
			long l; \
			if (CURLE_OK == curl_easy_getinfo(request->ch, I, &l)) { \
				char *key = estrndup(X, strlen(X)); \
				add_assoc_long(&array, pretty_key(key, strlen(X), 0, 0), l); \
				efree(key); \
			} \
		} \
		break; \
\
		case CURLINFO_SLIST: \
		{ \
			struct curl_slist *l, *p; \
			if (CURLE_OK == curl_easy_getinfo(request->ch, I, &l)) { \
				zval *subarray; \
				char *key = estrndup(X, strlen(X)); \
				MAKE_STD_ZVAL(subarray); \
				array_init(subarray); \
				for (p = l; p; p = p->next) { \
					add_next_index_string(subarray, p->data, 1); \
				} \
				add_assoc_zval(&array, pretty_key(key, strlen(X), 0, 0), subarray); \
				curl_slist_free_all(l); \
				efree(key); \
			} \
		} \
	}

#define HTTP_CURL_OPT(OPTION, p) HTTP_CURL_OPT_EX(request->ch, OPTION, (p))
#define HTTP_CURL_OPT_EX(ch, OPTION, p) curl_easy_setopt((ch), OPTION, (p))

#define HTTP_CURL_OPT_STRING(OPTION, ldiff, obdc) \
	{ \
		char *K = #OPTION; \
		HTTP_CURL_OPT_STRING_EX(K+lenof("CURLOPT_KEY")+ldiff, OPTION, obdc); \
	}
#define HTTP_CURL_OPT_STRING_EX(keyname, optname, obdc) \
	if (!strcasecmp(key, keyname)) { \
		zval *copy = http_request_option_cache(request, keyname, zval_copy(IS_STRING, *param)); \
		if (obdc) { \
			HTTP_CHECK_OPEN_BASEDIR(Z_STRVAL_P(copy), return FAILURE); \
		} \
		HTTP_CURL_OPT(optname, Z_STRVAL_P(copy)); \
		key = NULL; \
		continue; \
	}
#define HTTP_CURL_OPT_LONG(OPTION, ldiff) \
	{ \
		char *K = #OPTION; \
		HTTP_CURL_OPT_LONG_EX(K+lenof("CURLOPT_KEY")+ldiff, OPTION); \
	}
#define HTTP_CURL_OPT_LONG_EX(keyname, optname) \
	if (!strcasecmp(key, keyname)) { \
		zval *copy = http_request_option_cache(request, keyname, zval_copy(IS_LONG, *param)); \
		HTTP_CURL_OPT(optname, Z_LVAL_P(copy)); \
		key = NULL; \
		continue; \
	}
/* }}} */

/* {{{ forward declarations */
#define http_request_option(r, o, k, t) _http_request_option_ex((r), (o), (k), sizeof(k), (t) TSRMLS_CC)
#define http_request_option_ex(r, o, k, l, t) _http_request_option_ex((r), (o), (k), (l), (t) TSRMLS_CC)
static inline zval *_http_request_option_ex(http_request *request, HashTable *options, char *key, size_t keylen, int type TSRMLS_DC);
#define http_request_option_cache(r, k, z) _http_request_option_cache_ex((r), (k), sizeof(k), 0, (z) TSRMLS_CC)
#define http_request_option_cache_ex(r, k, kl, h, z) _http_request_option_cache_ex((r), (k), (kl), (h), (z) TSRMLS_CC)
static inline zval *_http_request_option_cache_ex(http_request *r, char *key, size_t keylen, ulong h, zval *opt TSRMLS_DC);

static size_t http_curl_read_callback(void *, size_t, size_t, void *);
static int http_curl_progress_callback(void *, double, double, double, double);
static int http_curl_raw_callback(CURL *, curl_infotype, char *, size_t, void *);
static int http_curl_dummy_callback(char *data, size_t n, size_t l, void *s) { return n*l; }
static curlioerr http_curl_ioctl_callback(CURL *, curliocmd, void *);
/* }}} */

/* {{{ CURL *http_curl_init(http_request *) */
PHP_HTTP_API CURL * _http_curl_init_ex(CURL *ch, http_request *request)
{
	if (ch || (ch = curl_easy_init())) {
#if defined(ZTS)
		HTTP_CURL_OPT_EX(ch, CURLOPT_NOSIGNAL, 1);
#endif
		HTTP_CURL_OPT_EX(ch, CURLOPT_HEADER, 0);
		HTTP_CURL_OPT_EX(ch, CURLOPT_FILETIME, 1);
		HTTP_CURL_OPT_EX(ch, CURLOPT_AUTOREFERER, 1);
		HTTP_CURL_OPT_EX(ch, CURLOPT_VERBOSE, 1);
		HTTP_CURL_OPT_EX(ch, CURLOPT_HEADERFUNCTION, NULL);
		HTTP_CURL_OPT_EX(ch, CURLOPT_DEBUGFUNCTION, http_curl_raw_callback);
		HTTP_CURL_OPT_EX(ch, CURLOPT_READFUNCTION, http_curl_read_callback);
		HTTP_CURL_OPT_EX(ch, CURLOPT_IOCTLFUNCTION, http_curl_ioctl_callback);
		HTTP_CURL_OPT_EX(ch, CURLOPT_WRITEFUNCTION, http_curl_dummy_callback);
		
		/* set context */
		if (request) {
			HTTP_CURL_OPT_EX(ch, CURLOPT_PRIVATE, request);
			HTTP_CURL_OPT_EX(ch, CURLOPT_DEBUGDATA, request);
			HTTP_CURL_OPT_EX(ch, CURLOPT_ERRORBUFFER, request->_error);
			
			/* attach curl handle */
			request->ch = ch;
			/* set defaults (also in http_request_reset()) */
			http_request_defaults(request);
		}
	}
	
	return ch;
}
/* }}} */

/* {{{ void http_curl_free(CURL **) */
PHP_HTTP_API void _http_curl_free(CURL **ch)
{
	if (*ch) {
		/* avoid nasty segfaults with already cleaned up callbacks */
		HTTP_CURL_OPT_EX(*ch, CURLOPT_NOPROGRESS, 1);
		HTTP_CURL_OPT_EX(*ch, CURLOPT_PROGRESSFUNCTION, NULL);
		HTTP_CURL_OPT_EX(*ch, CURLOPT_VERBOSE, 0);
		HTTP_CURL_OPT_EX(*ch, CURLOPT_DEBUGFUNCTION, NULL);
		curl_easy_cleanup(*ch);
		*ch = NULL;
	}
}
/* }}} */

/* {{{ http_request *http_request_init(http_request *) */
PHP_HTTP_API http_request *_http_request_init_ex(http_request *request, CURL *ch, http_request_method meth, const char *url ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	http_request *r;
	
	if (request) {
		r = request;
	} else {
		r = emalloc_rel(sizeof(http_request));
	}
	memset(r, 0, sizeof(http_request));
	
	r->ch = ch;
	r->url = (url) ? http_absolute_url(url) : NULL;
	r->meth = (meth > 0) ? meth : HTTP_GET;
	
	phpstr_init(&r->conv.request);
	phpstr_init_ex(&r->conv.response, HTTP_CURLBUF_SIZE, 0);
	phpstr_init(&r->_cache.cookies);
	zend_hash_init(&r->_cache.options, 0, NULL, ZVAL_PTR_DTOR, 0);
	
	TSRMLS_SET_CTX(r->tsrm_ls);
	
	return r;
}
/* }}} */

/* {{{ void http_request_dtor(http_request *) */
PHP_HTTP_API void _http_request_dtor(http_request *request)
{
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	http_curl_free(&request->ch);
	http_request_reset(request);
	
	phpstr_dtor(&request->_cache.cookies);
	zend_hash_destroy(&request->_cache.options);
	if (request->_cache.headers) {
		curl_slist_free_all(request->_cache.headers);
		request->_cache.headers = NULL;
	}
	if (request->_progress_callback) {
		zval_ptr_dtor(&request->_progress_callback);
		request->_progress_callback = NULL;
	}
}
/* }}} */

/* {{{ void http_request_free(http_request **) */
PHP_HTTP_API void _http_request_free(http_request **request)
{
	if (*request) {
		TSRMLS_FETCH_FROM_CTX((*request)->tsrm_ls);
		http_request_body_free(&(*request)->body);
		http_request_dtor(*request);
		efree(*request);
		*request = NULL;
	}
}
/* }}} */

/* {{{ void http_request_reset(http_request *) */
PHP_HTTP_API void _http_request_reset(http_request *request)
{
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	STR_SET(request->url, NULL);
	request->conv.last_type = 0;
	phpstr_dtor(&request->conv.request);
	phpstr_dtor(&request->conv.response);
	http_request_body_dtor(request->body);
	
	if (request->ch) {
		http_request_defaults(request);
	}
	request->_error[0] = '\0';
}
/* }}} */

/* {{{ void http_request_defaults(http_request *) */
PHP_HTTP_API void _http_request_defaults(http_request *request)
{
	if (request->ch) {
		HTTP_CURL_OPT(CURLOPT_PROGRESSFUNCTION, NULL);
		HTTP_CURL_OPT(CURLOPT_URL, NULL);
		HTTP_CURL_OPT(CURLOPT_NOPROGRESS, 1);
		HTTP_CURL_OPT(CURLOPT_PROXY, NULL);
		HTTP_CURL_OPT(CURLOPT_PROXYPORT, 0);
		HTTP_CURL_OPT(CURLOPT_PROXYUSERPWD, NULL);
		HTTP_CURL_OPT(CURLOPT_PROXYAUTH, 0);
		HTTP_CURL_OPT(CURLOPT_INTERFACE, NULL);
		HTTP_CURL_OPT(CURLOPT_PORT, 0);
		HTTP_CURL_OPT(CURLOPT_USERPWD, NULL);
		HTTP_CURL_OPT(CURLOPT_HTTPAUTH, 0);
		HTTP_CURL_OPT(CURLOPT_ENCODING, NULL);
		HTTP_CURL_OPT(CURLOPT_FOLLOWLOCATION, 0);
		HTTP_CURL_OPT(CURLOPT_UNRESTRICTED_AUTH, 0);
		HTTP_CURL_OPT(CURLOPT_REFERER, NULL);
		HTTP_CURL_OPT(CURLOPT_USERAGENT, "PECL::HTTP/" PHP_EXT_HTTP_VERSION " (PHP/" PHP_VERSION ")");
		HTTP_CURL_OPT(CURLOPT_HTTPHEADER, NULL);
		HTTP_CURL_OPT(CURLOPT_COOKIE, NULL);
#if LIBCURL_VERSION_NUM >= 0x070e01
		HTTP_CURL_OPT(CURLOPT_COOKIELIST, NULL);
#endif
		HTTP_CURL_OPT(CURLOPT_COOKIEFILE, NULL);
		HTTP_CURL_OPT(CURLOPT_COOKIEJAR, NULL);
		HTTP_CURL_OPT(CURLOPT_RESUME_FROM, 0);
		HTTP_CURL_OPT(CURLOPT_MAXFILESIZE, 0);
		HTTP_CURL_OPT(CURLOPT_TIMECONDITION, 0);
		HTTP_CURL_OPT(CURLOPT_TIMEVALUE, 0);
		HTTP_CURL_OPT(CURLOPT_TIMEOUT, 0);
		HTTP_CURL_OPT(CURLOPT_CONNECTTIMEOUT, 3);
		HTTP_CURL_OPT(CURLOPT_SSLCERT, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLCERTTYPE, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLCERTPASSWD, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLKEY, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLKEYTYPE, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLKEYPASSWD, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLENGINE, NULL);
		HTTP_CURL_OPT(CURLOPT_SSLVERSION, 0);
		HTTP_CURL_OPT(CURLOPT_SSL_VERIFYPEER, 0);
		HTTP_CURL_OPT(CURLOPT_SSL_VERIFYHOST, 0);
		HTTP_CURL_OPT(CURLOPT_SSL_CIPHER_LIST, NULL);
		HTTP_CURL_OPT(CURLOPT_CAINFO, NULL);
		HTTP_CURL_OPT(CURLOPT_CAPATH, NULL);
		HTTP_CURL_OPT(CURLOPT_RANDOM_FILE, NULL);
		HTTP_CURL_OPT(CURLOPT_EGDSOCKET, NULL);
		HTTP_CURL_OPT(CURLOPT_POSTFIELDS, NULL);
		HTTP_CURL_OPT(CURLOPT_POSTFIELDSIZE, 0);
		HTTP_CURL_OPT(CURLOPT_HTTPPOST, NULL);
		HTTP_CURL_OPT(CURLOPT_IOCTLDATA, NULL);
		HTTP_CURL_OPT(CURLOPT_READDATA, NULL);
		HTTP_CURL_OPT(CURLOPT_INFILESIZE, 0);
		HTTP_CURL_OPT(CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_NONE);
		HTTP_CURL_OPT(CURLOPT_CUSTOMREQUEST, NULL);
		HTTP_CURL_OPT(CURLOPT_NOBODY, 0);
		HTTP_CURL_OPT(CURLOPT_POST, 0);
		HTTP_CURL_OPT(CURLOPT_UPLOAD, 0);
		HTTP_CURL_OPT(CURLOPT_HTTPGET, 1);
	}
}
/* }}} */

PHP_HTTP_API void _http_request_set_progress_callback(http_request *request, zval *cb)
{
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	if (request->_progress_callback) {
		zval_ptr_dtor(&request->_progress_callback);
	}
	if ((request->_progress_callback = cb)) {
		ZVAL_ADDREF(cb);
		HTTP_CURL_OPT(CURLOPT_NOPROGRESS, 0);
		HTTP_CURL_OPT(CURLOPT_PROGRESSDATA, request);
		HTTP_CURL_OPT(CURLOPT_PROGRESSFUNCTION, http_curl_progress_callback);
	} else {
		HTTP_CURL_OPT(CURLOPT_NOPROGRESS, 1);
		HTTP_CURL_OPT(CURLOPT_PROGRESSDATA, NULL);
		HTTP_CURL_OPT(CURLOPT_PROGRESSFUNCTION, NULL);
	}
}

/* {{{ STATUS http_request_prepare(http_request *, HashTable *) */
PHP_HTTP_API STATUS _http_request_prepare(http_request *request, HashTable *options)
{
	zval *zoption;
	zend_bool range_req = 0;

	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	HTTP_CHECK_CURL_INIT(request->ch, http_curl_init(request), return FAILURE);
	
	/* set options */
	HTTP_CURL_OPT(CURLOPT_URL, request->url);

	/* progress callback */
	if ((zoption = http_request_option(request, options, "onprogress", -1))) {
		http_request_set_progress_callback(request, zoption);
	}

	/* proxy */
	if ((zoption = http_request_option(request, options, "proxyhost", IS_STRING))) {
		if (Z_STRLEN_P(zoption)) {
			HTTP_CURL_OPT(CURLOPT_PROXY, Z_STRVAL_P(zoption));
		}

		/* port */
		if ((zoption = http_request_option(request, options, "proxyport", IS_LONG))) {
			HTTP_CURL_OPT(CURLOPT_PROXYPORT, Z_LVAL_P(zoption));
		}
		/* user:pass */
		if ((zoption = http_request_option(request, options, "proxyauth", IS_STRING)) && Z_STRLEN_P(zoption)) {
			HTTP_CURL_OPT(CURLOPT_PROXYUSERPWD, Z_STRVAL_P(zoption));
		}
		/* auth method */
		if ((zoption = http_request_option(request, options, "proxyauthtype", IS_LONG))) {
			HTTP_CURL_OPT(CURLOPT_PROXYAUTH, Z_LVAL_P(zoption));
		}
	}

	/* outgoing interface */
	if ((zoption = http_request_option(request, options, "interface", IS_STRING))) {
		HTTP_CURL_OPT(CURLOPT_INTERFACE, Z_STRVAL_P(zoption));
	}

	/* another port */
	if ((zoption = http_request_option(request, options, "port", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_PORT, Z_LVAL_P(zoption));
	}

	/* auth */
	if ((zoption = http_request_option(request, options, "httpauth", IS_STRING)) && Z_STRLEN_P(zoption)) {
		HTTP_CURL_OPT(CURLOPT_USERPWD, Z_STRVAL_P(zoption));
	}
	if ((zoption = http_request_option(request, options, "httpauthtype", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_HTTPAUTH, Z_LVAL_P(zoption));
	}

	/* redirects, defaults to 0 */
	if ((zoption = http_request_option(request, options, "redirect", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_FOLLOWLOCATION, Z_LVAL_P(zoption) ? 1 : 0);
		HTTP_CURL_OPT(CURLOPT_MAXREDIRS, Z_LVAL_P(zoption));
		if ((zoption = http_request_option(request, options, "unrestrictedauth", IS_BOOL))) {
			HTTP_CURL_OPT(CURLOPT_UNRESTRICTED_AUTH, Z_LVAL_P(zoption));
		}
	}

	/* referer */
	if ((zoption = http_request_option(request, options, "referer", IS_STRING)) && Z_STRLEN_P(zoption)) {
		HTTP_CURL_OPT(CURLOPT_REFERER, Z_STRVAL_P(zoption));
	}

	/* useragent, default "PECL::HTTP/version (PHP/version)" */
	if ((zoption = http_request_option(request, options, "useragent", IS_STRING))) {
		/* allow to send no user agent, not even default one */
		if (Z_STRLEN_P(zoption)) {
			HTTP_CURL_OPT(CURLOPT_USERAGENT, Z_STRVAL_P(zoption));
		} else {
			HTTP_CURL_OPT(CURLOPT_USERAGENT, NULL);
		}
	}

	/* resume */
	if ((zoption = http_request_option(request, options, "resume", IS_LONG)) && (Z_LVAL_P(zoption) != 0)) {
		range_req = 1;
		HTTP_CURL_OPT(CURLOPT_RESUME_FROM, Z_LVAL_P(zoption));
	}

	/* additional headers, array('name' => 'value') */
	if (request->_cache.headers) {
		curl_slist_free_all(request->_cache.headers);
		request->_cache.headers = NULL;
	}
	if ((zoption = http_request_option(request, options, "headers", IS_ARRAY))) {
		char *header_key;
		ulong header_idx;
		HashPosition pos;

		FOREACH_KEY(pos, zoption, header_key, header_idx) {
			if (header_key) {
				zval **header_val;
				if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_P(zoption), (void **) &header_val, &pos)) {
					char header[1024] = {0};
					
					ZVAL_ADDREF(*header_val);
					convert_to_string_ex(header_val);
					if (!strcasecmp(header_key, "range")) {
						range_req = 1;
					}
					snprintf(header, lenof(header), "%s: %s", header_key, Z_STRVAL_PP(header_val));
					request->_cache.headers = curl_slist_append(request->_cache.headers, header);
					zval_ptr_dtor(header_val);
				}

				/* reset */
				header_key = NULL;
			}
		}
	}
	/* etag */
	if ((zoption = http_request_option(request, options, "etag", IS_STRING)) && Z_STRLEN_P(zoption)) {
		char match_header[1024] = {0}, *quoted_etag = NULL;
		
		if ((Z_STRVAL_P(zoption)[0] != '"') || (Z_STRVAL_P(zoption)[Z_STRLEN_P(zoption)-1] != '"')) {
			spprintf(&quoted_etag, 0, "\"%s\"", Z_STRVAL_P(zoption));
		}
		snprintf(match_header, lenof(match_header), "%s: %s", range_req?"If-Match":"If-None-Match", quoted_etag?quoted_etag:Z_STRVAL_P(zoption));
		request->_cache.headers = curl_slist_append(request->_cache.headers, match_header);
		STR_FREE(quoted_etag);
	}
	/* compression */
	if ((zoption = http_request_option(request, options, "compress", IS_BOOL)) && Z_LVAL_P(zoption)) {
		request->_cache.headers = curl_slist_append(request->_cache.headers, "Accept-Encoding: gzip;q=1.0,deflate;q=0.5");
	}
	HTTP_CURL_OPT(CURLOPT_HTTPHEADER, request->_cache.headers);

	/* lastmodified */
	if ((zoption = http_request_option(request, options, "lastmodified", IS_LONG))) {
		if (Z_LVAL_P(zoption)) {
			if (Z_LVAL_P(zoption) > 0) {
				HTTP_CURL_OPT(CURLOPT_TIMEVALUE, Z_LVAL_P(zoption));
			} else {
				HTTP_CURL_OPT(CURLOPT_TIMEVALUE, HTTP_GET_REQUEST_TIME() + Z_LVAL_P(zoption));
			}
			HTTP_CURL_OPT(CURLOPT_TIMECONDITION, range_req ? CURL_TIMECOND_IFUNMODSINCE : CURL_TIMECOND_IFMODSINCE);
		} else {
			HTTP_CURL_OPT(CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
		}
	}

	/* cookies, array('name' => 'value') */
	if ((zoption = http_request_option(request, options, "cookies", IS_ARRAY))) {
		phpstr_dtor(&request->_cache.cookies);
		if (zend_hash_num_elements(Z_ARRVAL_P(zoption))) {
			zval *urlenc_cookies = NULL;
			/* check whether cookies should not be urlencoded; default is to urlencode them */
			if ((!(urlenc_cookies = http_request_option(request, options, "encodecookies", IS_BOOL))) || Z_BVAL_P(urlenc_cookies)) {
				if (SUCCESS == http_urlencode_hash_recursive(HASH_OF(zoption), &request->_cache.cookies, "; ", lenof("; "), NULL, 0)) {
					phpstr_fix(&request->_cache.cookies);
					HTTP_CURL_OPT(CURLOPT_COOKIE, request->_cache.cookies.data);
				}
			} else {
				HashPosition pos;
				zval *cookie_val = NULL;
				char *cookie_key = NULL;
				ulong cookie_idx;
				
				FOREACH_KEY(pos, zoption, cookie_key, cookie_idx) {
					if (cookie_key) {
						zval **cookie_val;
						if (SUCCESS == zend_hash_get_current_data_ex(Z_ARRVAL_P(zoption), (void **) &cookie_val, &pos)) {
							zval *val = zval_copy(IS_STRING, *cookie_val);
							phpstr_appendf(&request->_cache.cookies, "%s=%s; ", cookie_key, Z_STRVAL_P(val));
							zval_free(&val);
						}

						/* reset */
						cookie_key = NULL;
					}
				}
				
				phpstr_fix(&request->_cache.cookies);
				if (PHPSTR_LEN(&request->_cache.cookies)) {
					HTTP_CURL_OPT(CURLOPT_COOKIE, PHPSTR_VAL(&request->_cache.cookies));
				}
			}
		}
	}

#if LIBCURL_VERSION_NUM >= 0x070e01
	/* reset cookies */
	if ((zoption = http_request_option(request, options, "resetcookies", IS_BOOL)) && Z_LVAL_P(zoption)) {
		HTTP_CURL_OPT(CURLOPT_COOKIELIST, "ALL");
	}
#endif
	
	/* session cookies */
	if ((zoption = http_request_option(request, options, "cookiesession", IS_BOOL))) {
		if (Z_LVAL_P(zoption)) {
			/* accept cookies for this session */
			HTTP_CURL_OPT(CURLOPT_COOKIEFILE, "");
		} else {
			/* reset session cookies */
			HTTP_CURL_OPT(CURLOPT_COOKIESESSION, 1);
		}
	}

	/* cookiestore, read initial cookies from that file and store cookies back into that file */
	if ((zoption = http_request_option(request, options, "cookiestore", IS_STRING)) && Z_STRLEN_P(zoption)) {
		HTTP_CHECK_OPEN_BASEDIR(Z_STRVAL_P(zoption), return FAILURE);
		HTTP_CURL_OPT(CURLOPT_COOKIEFILE, Z_STRVAL_P(zoption));
		HTTP_CURL_OPT(CURLOPT_COOKIEJAR, Z_STRVAL_P(zoption));
	}

	/* maxfilesize */
	if ((zoption = http_request_option(request, options, "maxfilesize", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_MAXFILESIZE, Z_LVAL_P(zoption));
	}

	/* http protocol */
	if ((zoption = http_request_option(request, options, "protocol", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_HTTP_VERSION, Z_LVAL_P(zoption));
	}

	/* timeout, defaults to 0 */
	if ((zoption = http_request_option(request, options, "timeout", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_TIMEOUT, Z_LVAL_P(zoption));
	}

	/* connecttimeout, defaults to 0 */
	if ((zoption = http_request_option(request, options, "connecttimeout", IS_LONG))) {
		HTTP_CURL_OPT(CURLOPT_CONNECTTIMEOUT, Z_LVAL_P(zoption));
	}

	/* ssl */
	if ((zoption = http_request_option(request, options, "ssl", IS_ARRAY))) {
		ulong idx;
		char *key = NULL;
		zval **param;
		HashPosition pos;

		FOREACH_KEYVAL(pos, zoption, key, idx, param) {
			if (key) {
				HTTP_CURL_OPT_STRING(CURLOPT_SSLCERT, 0, 1);
				HTTP_CURL_OPT_STRING(CURLOPT_SSLCERTTYPE, 0, 0);
				HTTP_CURL_OPT_STRING(CURLOPT_SSLCERTPASSWD, 0, 0);

				HTTP_CURL_OPT_STRING(CURLOPT_SSLKEY, 0, 0);
				HTTP_CURL_OPT_STRING(CURLOPT_SSLKEYTYPE, 0, 0);
				HTTP_CURL_OPT_STRING(CURLOPT_SSLKEYPASSWD, 0, 0);

				HTTP_CURL_OPT_STRING(CURLOPT_SSLENGINE, 0, 0);
				HTTP_CURL_OPT_LONG(CURLOPT_SSLVERSION, 0);

				HTTP_CURL_OPT_LONG(CURLOPT_SSL_VERIFYPEER, 1);
				HTTP_CURL_OPT_LONG(CURLOPT_SSL_VERIFYHOST, 1);
				HTTP_CURL_OPT_STRING(CURLOPT_SSL_CIPHER_LIST, 1, 0);

				HTTP_CURL_OPT_STRING(CURLOPT_CAINFO, -3, 1);
				HTTP_CURL_OPT_STRING(CURLOPT_CAPATH, -3, 1);
				HTTP_CURL_OPT_STRING(CURLOPT_RANDOM_FILE, -3, 1);
				HTTP_CURL_OPT_STRING(CURLOPT_EGDSOCKET, -3, 1);

				/* reset key */
				key = NULL;
			}
		}
	}

	/* request method */
	switch (request->meth)
	{
		case HTTP_GET:
			HTTP_CURL_OPT(CURLOPT_HTTPGET, 1);
		break;

		case HTTP_HEAD:
			HTTP_CURL_OPT(CURLOPT_NOBODY, 1);
		break;

		case HTTP_POST:
			HTTP_CURL_OPT(CURLOPT_POST, 1);
		break;

		case HTTP_PUT:
			HTTP_CURL_OPT(CURLOPT_UPLOAD, 1);
		break;

		default:
			if (http_request_method_exists(0, request->meth, NULL)) {
				HTTP_CURL_OPT(CURLOPT_CUSTOMREQUEST, http_request_method_name(request->meth));
			} else {
				http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Unsupported request method: %d (%s)", request->meth, request->url);
				return FAILURE;
			}
		break;
	}

	/* attach request body */
	if (request->body && (request->meth != HTTP_GET) && (request->meth != HTTP_HEAD) && (request->meth != HTTP_OPTIONS)) {
		switch (request->body->type)
		{
			case HTTP_REQUEST_BODY_EMPTY:
				/* nothing */
			break;
			
			case HTTP_REQUEST_BODY_CSTRING:
				HTTP_CURL_OPT(CURLOPT_POSTFIELDS, request->body->data);
				HTTP_CURL_OPT(CURLOPT_POSTFIELDSIZE, request->body->size);
			break;

			case HTTP_REQUEST_BODY_CURLPOST:
				HTTP_CURL_OPT(CURLOPT_HTTPPOST, (struct curl_httppost *) request->body->data);
			break;

			case HTTP_REQUEST_BODY_UPLOADFILE:
				HTTP_CURL_OPT(CURLOPT_IOCTLDATA, request);
				HTTP_CURL_OPT(CURLOPT_READDATA, request);
				HTTP_CURL_OPT(CURLOPT_INFILESIZE, request->body->size);
			break;

			default:
				/* shouldn't ever happen */
				http_error_ex(HE_ERROR, 0, "Unknown request body type: %d (%s)", request->body->type, request->url);
				return FAILURE;
			break;
		}
	}

	return SUCCESS;
}
/* }}} */

/* {{{ void http_request_exec(http_request *) */
PHP_HTTP_API void _http_request_exec(http_request *request)
{
	CURLcode result;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	if (CURLE_OK != (result = curl_easy_perform(request->ch))) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST, "%s; %s (%s)", curl_easy_strerror(result), request->_error, request->url);
	}
}
/* }}} */

/* {{{ void http_request_info(http_request *, HashTable *) */
PHP_HTTP_API void _http_request_info(http_request *request, HashTable *info)
{
	zval array;
	INIT_ZARR(array, info);

	HTTP_CURL_INFO(CURLINFO_EFFECTIVE_URL);
	HTTP_CURL_INFO(CURLINFO_RESPONSE_CODE);
	HTTP_CURL_INFO_EX(CURLINFO_HTTP_CONNECTCODE, "connect_code");
	HTTP_CURL_INFO(CURLINFO_FILETIME);
	HTTP_CURL_INFO(CURLINFO_TOTAL_TIME);
	HTTP_CURL_INFO(CURLINFO_NAMELOOKUP_TIME);
	HTTP_CURL_INFO(CURLINFO_CONNECT_TIME);
	HTTP_CURL_INFO(CURLINFO_PRETRANSFER_TIME);
	HTTP_CURL_INFO(CURLINFO_STARTTRANSFER_TIME);
	HTTP_CURL_INFO(CURLINFO_REDIRECT_TIME);
	HTTP_CURL_INFO(CURLINFO_REDIRECT_COUNT);
	HTTP_CURL_INFO(CURLINFO_SIZE_UPLOAD);
	HTTP_CURL_INFO(CURLINFO_SIZE_DOWNLOAD);
	HTTP_CURL_INFO(CURLINFO_SPEED_DOWNLOAD);
	HTTP_CURL_INFO(CURLINFO_SPEED_UPLOAD);
	HTTP_CURL_INFO(CURLINFO_HEADER_SIZE);
	HTTP_CURL_INFO(CURLINFO_REQUEST_SIZE);
	HTTP_CURL_INFO(CURLINFO_SSL_VERIFYRESULT);
	HTTP_CURL_INFO(CURLINFO_SSL_ENGINES);
	HTTP_CURL_INFO(CURLINFO_CONTENT_LENGTH_DOWNLOAD);
	HTTP_CURL_INFO(CURLINFO_CONTENT_LENGTH_UPLOAD);
	HTTP_CURL_INFO(CURLINFO_CONTENT_TYPE);
	HTTP_CURL_INFO(CURLINFO_HTTPAUTH_AVAIL);
	HTTP_CURL_INFO(CURLINFO_PROXYAUTH_AVAIL);
	HTTP_CURL_INFO(CURLINFO_OS_ERRNO);
	add_assoc_string(&array, "error", request->_error, 1);
	HTTP_CURL_INFO(CURLINFO_NUM_CONNECTS);
#if LIBCURL_VERSION_NUM >= 0x070e01
	HTTP_CURL_INFO_EX(CURLINFO_COOKIELIST, "cookies");
#endif
}
/* }}} */

/* {{{ static size_t http_curl_read_callback(void *, size_t, size_t, void *) */
static size_t http_curl_read_callback(void *data, size_t len, size_t n, void *ctx)
{
	http_request *request = (http_request *) ctx;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);

	if (request->body == NULL || request->body->type != HTTP_REQUEST_BODY_UPLOADFILE) {
		return 0;
	}
	return php_stream_read((php_stream *) request->body->data, data, len * n);
}
/* }}} */

/* {{{ static int http_curl_progress_callback(void *, double, double, double, double) */
static int http_curl_progress_callback(void *ctx, double dltotal, double dlnow, double ultotal, double ulnow)
{
	zval *param, retval;
	http_request *request = (http_request *) ctx;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);

	INIT_PZVAL(&retval);
	ZVAL_NULL(&retval);

	MAKE_STD_ZVAL(param);
	array_init(param);
	add_assoc_double(param, "dltotal", dltotal);
	add_assoc_double(param, "dlnow", dlnow);
	add_assoc_double(param, "ultotal", ultotal);
	add_assoc_double(param, "ulnow", ulnow);
	
	call_user_function(EG(function_table), NULL, request->_progress_callback, &retval, 1, &param TSRMLS_CC);
	
	zval_ptr_dtor(&param);
	zval_dtor(&retval);
	
	return 0;
}
/* }}} */

/* {{{ static curlioerr http_curl_ioctl_callback(CURL *, curliocmd, void *) */
static curlioerr http_curl_ioctl_callback(CURL *ch, curliocmd cmd, void *ctx)
{
	http_request *request = (http_request *) ctx;
	TSRMLS_FETCH_FROM_CTX(request->tsrm_ls);
	
	if (cmd != CURLIOCMD_RESTARTREAD) {
		return CURLIOE_UNKNOWNCMD;
	}
	if (	request->body == NULL || 
			request->body->type != HTTP_REQUEST_BODY_UPLOADFILE ||
			SUCCESS != php_stream_rewind((php_stream *) request->body->data)) {
		return CURLIOE_FAILRESTART;
	}
	return CURLIOE_OK;
}
/* }}} */

/* {{{ static int http_curl_raw_callback(CURL *, curl_infotype, char *, size_t, void *) */
static int http_curl_raw_callback(CURL *ch, curl_infotype type, char *data, size_t length, void *ctx)
{
	http_request *request = (http_request *) ctx;

	switch (type)
	{
		case CURLINFO_DATA_IN:
			if (request->conv.last_type == CURLINFO_HEADER_IN) {
				phpstr_appends(&request->conv.response, HTTP_CRLF);
			}
		case CURLINFO_HEADER_IN:
			phpstr_append(&request->conv.response, data, length);
		break;
		case CURLINFO_DATA_OUT:
			if (request->conv.last_type == CURLINFO_HEADER_OUT) {
				phpstr_appends(&request->conv.request, HTTP_CRLF);
			}
		case CURLINFO_HEADER_OUT:
			phpstr_append(&request->conv.request, data, length);
		break;
		default:
#if 0
			fprintf(stderr, "## ", type);
			if (!type) {
				fprintf(stderr, "%s", data);
			} else {
				ulong i;
				for (i = 1; i <= length; ++i) {
					fprintf(stderr, "%02X ", data[i-1] & 0xFF);
					if (!(i % 20)) {
						fprintf(stderr, "\n## ");
					}
				}
				fprintf(stderr, "\n");
			}
			if (data[length-1] != 0xa) {
				fprintf(stderr, "\n");
			}
#endif
		break;
	}

	if (type) {
		request->conv.last_type = type;
	}
	return 0;
}
/* }}} */

/* {{{ static inline zval *http_request_option(http_request *, HashTable *, char *, size_t, int) */
static inline zval *_http_request_option_ex(http_request *r, HashTable *options, char *key, size_t keylen, int type TSRMLS_DC)
{
	zval **zoption;
#ifdef ZEND_ENGINE_2
	ulong h = zend_get_hash_value(key, keylen);
#else
	ulong h = 0;
#endif
	
	if (!options || 
#ifdef ZEND_ENGINE_2
			(SUCCESS != zend_hash_quick_find(options, key, keylen, h, (void **) &zoption))
#else
			(SUCCESS != zend_hash_find(options, key, keylen, (void **) &zoption))
#endif
	) {
		return NULL;
	}
	
	return http_request_option_cache_ex(r, key, keylen, h, zval_copy(type, *zoption));
}
/* }}} */

/* {{{ static inline zval *http_request_option_cache(http_request *, char *key, zval *) */
static inline zval *_http_request_option_cache_ex(http_request *r, char *key, size_t keylen, ulong h, zval *opt TSRMLS_DC)
{
	ZVAL_ADDREF(opt);
	
#ifdef ZEND_ENGINE_2
	if (h) {
		_zend_hash_quick_add_or_update(&r->_cache.options, key, keylen, h, &opt, sizeof(zval *), NULL, 
			zend_hash_quick_exists(&r->_cache.options, key, keylen, h)?HASH_UPDATE:HASH_ADD ZEND_FILE_LINE_CC);
	}
	else
#endif
	{
		if (zend_hash_exists(&r->_cache.options, key, keylen)) {
			zend_hash_update(&r->_cache.options, key, keylen, &opt, sizeof(zval *), NULL);
		} else {
			zend_hash_add(&r->_cache.options, key, keylen, &opt, sizeof(zval *), NULL);
		}
	}
	
	return opt;
}
/* }}} */

#endif /* HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

