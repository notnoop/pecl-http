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

#ifndef HAVE_CURL_EASY_STRERROR
#	define curl_easy_strerror(dummy) "unknown error"
#endif

#define HTTP_CURL_OPT(OPTION, p) HTTP_CURL_OPT_EX(request->ch, OPTION, (p))
#define HTTP_CURL_OPT_EX(ch, OPTION, p) curl_easy_setopt((ch), OPTION, (p))

#define HTTP_CURL_OPT_STRING(OPTION, ldiff, obdc) \
	{ \
		char *K = #OPTION; \
		HTTP_CURL_OPT_STRING_EX(K+lenof("CURLOPT_KEY")+ldiff, OPTION, obdc); \
	}
#define HTTP_CURL_OPT_STRING_EX(keyname, optname, obdc) \
	if (!strcasecmp(key, keyname)) { \
		zval *copy = http_request_option_cache_ex(request, keyname, strlen(keyname)+1, 0, zval_copy(IS_STRING, *param)); \
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
		zval *copy = zval_copy(IS_LONG, *param); \
		HTTP_CURL_OPT(optname, Z_LVAL_P(copy)); \
		key = NULL; \
		zval_free(&copy); \
		continue; \
	}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
