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

#ifndef PHP_EXT_HTTP_H
#define PHP_EXT_HTTP_H

#define PHP_EXT_HTTP_VERSION "0.25.0dev"

#ifdef HAVE_CONFIG_H
#	include "config.h"
#else
#	ifndef PHP_WIN32
#		include "php_config.h"
#	endif
#endif

#include "php.h"
#include "php_http_std_defs.h"
#include "phpstr/phpstr.h"
#include "missing.h"

#ifdef HTTP_WANT_SAPI
#	if PHP_API_VERSION > 20041225
#		define HTTP_HAVE_SAPI_RTIME
#		define HTTP_GET_REQUEST_TIME() sapi_get_request_time(TSRMLS_C)
#	else
#		define HTTP_GET_REQUEST_TIME() HTTP_G->request_time
#	endif
#	include "SAPI.h"
#endif

#ifdef HTTP_WANT_NETDB
#	ifdef PHP_WIN32
#		define HTTP_HAVE_NETDB
#		include <winsock2.h>
#	elif defined(HAVE_NETDB_H)
#		define HTTP_HAVE_NETDB
#		include <netdb.h>
#		ifdef HAVE_UNISTD_H
#			include <unistd.h>
#		endif
#	endif
#endif

#if defined(HTTP_WANT_CURL) && defined(HTTP_HAVE_CURL)
#	ifdef PHP_WIN32
#		include <winsock2.h>
#		define CURL_STATICLIB
#	endif
#	include <curl/curl.h>
#	define HTTP_CURL_VERSION(x, y, z) (LIBCURL_VERSION_NUM >= (((x)<<16) + ((y)<<8) + (z)))
#endif

#if defined(HTTP_WANT_MAGIC) && defined(HTTP_HAVE_MAGIC)
#	if defined(PHP_WIN32) && !defined(USE_MAGIC_DLL) && !defined(USE_MAGIC_STATIC)
#		define USE_MAGIC_STATIC
#	endif
#	include <magic.h>
#endif

#if defined(HTTP_WANT_ZLIB) && defined(HTTP_HAVE_ZLIB)
#	include <zlib.h>
#endif

#include <ctype.h>

extern zend_module_entry http_module_entry;
#define phpext_http_ptr &http_module_entry

extern int http_module_number;

ZEND_BEGIN_MODULE_GLOBALS(http)

	struct _http_globals_etag {
		char *mode;
		void *ctx;
		zend_bool started;
	} etag;

	struct _http_globals_log {
		char *cache;
		char *redirect;
		char *allowed_methods;
		char *composite;
	} log;

	struct _http_globals_send {
		double throttle_delay;
		size_t buffer_size;
		char *content_type;
		char *unquoted_etag;
		time_t last_modified;
		struct _http_globals_send_deflate {
			zend_bool start_auto;
			long start_flags;
			int encoding;
			void *stream;
		} deflate;
		struct _http_globals_send_inflate {
			zend_bool start_auto;
			long start_flags;
			void *stream;
		} inflate;
	} send;

	struct _http_globals_request {
		struct _http_globals_request_methods {
			char *allowed;
			struct _http_globals_request_methods_custom {
				int count;
				void *entries;
			} custom;
		} methods;
	} request;

#ifndef HTTP_HAVE_SAPI_RTIME
	time_t request_time;
#endif
#ifdef ZEND_ENGINE_2
	zend_bool only_exceptions;
#endif

	zend_bool force_exit;
	zend_bool read_post_data;

ZEND_END_MODULE_GLOBALS(http)

ZEND_EXTERN_MODULE_GLOBALS(http);

#ifdef ZTS
#	include "TSRM.h"
#	define HTTP_G ((zend_http_globals *) (*((void ***) tsrm_ls))[TSRM_UNSHUFFLE_RSRC_ID(http_globals_id)])
#else
#	define HTTP_G (&http_globals)
#endif

PHP_FUNCTION(http_test);
PHP_FUNCTION(http_date);
PHP_FUNCTION(http_build_url);
PHP_FUNCTION(http_build_str);
PHP_FUNCTION(http_negotiate_language);
PHP_FUNCTION(http_negotiate_charset);
PHP_FUNCTION(http_negotiate_content_type);
PHP_FUNCTION(http_redirect);
PHP_FUNCTION(http_throttle);
PHP_FUNCTION(http_send_status);
PHP_FUNCTION(http_send_last_modified);
PHP_FUNCTION(http_send_content_type);
PHP_FUNCTION(http_send_content_disposition);
PHP_FUNCTION(http_match_modified);
PHP_FUNCTION(http_match_etag);
PHP_FUNCTION(http_cache_last_modified);
PHP_FUNCTION(http_cache_etag);
PHP_FUNCTION(http_send_data);
PHP_FUNCTION(http_send_file);
PHP_FUNCTION(http_send_stream);
PHP_FUNCTION(http_chunked_decode);
PHP_FUNCTION(http_parse_message);
PHP_FUNCTION(http_parse_headers);
PHP_FUNCTION(http_parse_cookie);
PHP_FUNCTION(http_get_request_headers);
PHP_FUNCTION(http_get_request_body);
PHP_FUNCTION(http_get_request_body_stream);
PHP_FUNCTION(http_match_request_header);
#ifdef HTTP_HAVE_CURL
PHP_FUNCTION(http_get);
PHP_FUNCTION(http_head);
PHP_FUNCTION(http_post_data);
PHP_FUNCTION(http_post_fields);
PHP_FUNCTION(http_put_data);
PHP_FUNCTION(http_put_file);
PHP_FUNCTION(http_put_stream);
#endif /* HTTP_HAVE_CURL */
PHP_FUNCTION(http_request_method_register);
PHP_FUNCTION(http_request_method_unregister);
PHP_FUNCTION(http_request_method_exists);
PHP_FUNCTION(http_request_method_name);
PHP_FUNCTION(ob_etaghandler);
#ifdef HTTP_HAVE_ZLIB
PHP_FUNCTION(http_deflate);
PHP_FUNCTION(http_inflate);
PHP_FUNCTION(ob_deflatehandler);
PHP_FUNCTION(ob_inflatehandler);
#endif
PHP_FUNCTION(http_support);

PHP_MINIT_FUNCTION(http);
PHP_MSHUTDOWN_FUNCTION(http);
PHP_RINIT_FUNCTION(http);
PHP_RSHUTDOWN_FUNCTION(http);
PHP_MINFO_FUNCTION(http);

#endif	/* PHP_HTTP_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

