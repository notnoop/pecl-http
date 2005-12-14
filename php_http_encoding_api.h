/*
    +--------------------------------------------------------------------+
    | PECL :: http                                                       |
    +--------------------------------------------------------------------+
    | Redistribution and use in source and binary forms, with or without |
    | modification, are permitted provided that the conditions mentioned |
    | in the accompanying LICENSE file are met.                          |
    +--------------------------------------------------------------------+
    | Copyright (c) 2004-2005, Michael Wallner <mike@php.net>            |
    +--------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_HTTP_ENCODING_API_H
#define PHP_HTTP_ENCODING_API_H

#ifdef HTTP_HAVE_ZLIB
#	include <zlib.h>
#endif

#define http_encoding_dechunk(e, el, d, dl) _http_encoding_dechunk((e), (el), (d), (dl) TSRMLS_CC)
PHP_HTTP_API const char *_http_encoding_dechunk(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC);

#define http_encoding_response_start(cl) _http_encoding_response_start((cl) TSRMLS_CC)
PHP_HTTP_API zend_bool _http_encoding_response_start(size_t content_length TSRMLS_DC);

#ifdef HTTP_HAVE_ZLIB

/* max count of uncompress trials, alloc_size <<= 2 for each try */
#define HTTP_ENCODING_MAXTRY 10
/* safe padding */
#define HTTP_ENCODING_SAFPAD 10
/* add 1% extra space in case we need to encode widely differing (binary) data */
#define HTTP_ENCODING_BUFLEN(l) (l + (l / 100) + HTTP_ENCODING_SAFPAD)

typedef enum {
	HTTP_ENCODING_NONE = 0,
	HTTP_ENCODING_GZIP,
	HTTP_ENCODING_DEFLATE,
} http_encoding_type;

typedef struct {
	z_stream Z;
	int gzip;
	ulong crc;
	phpstr *storage;
} http_encoding_stream;

#define http_encoding_stream_init(s, g, l, e, el) _http_encoding_stream_init((s), (g), (l), (e), (el) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_stream_init(http_encoding_stream *s, int gzip, int level, char **encoded, size_t *encoded_len TSRMLS_DC);
#define http_encoding_stream_update(s, d, dl, e, el) _http_encoding_stream_update((s), (d), (dl), (e), (el) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_stream_update(http_encoding_stream *s, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC);
#define http_encoding_stream_finish(s, e, el) _http_encoding_stream_finish((s), (e), (el) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_stream_finish(http_encoding_stream *s, char **encoded, size_t *encoded_len TSRMLS_DC);

#define http_encoding_gzencode_verify(d, dl, e, el) _http_encoding_gzencode_verify((d), (dl), (e), (el), HE_NOTICE)
PHP_HTTP_API STATUS _http_encoding_gzencode_verify(const char *data, size_t data_len, const char **encoded, size_t *encoded_len, int error_level TSRMLS_DC);
#define http_encoding_gzdecode_verify(e, el, d, dl) _http_encoding_gzdecode_verify((e), (el), (d), (dl), HE_NOTICE)
PHP_HTTP_API STATUS _http_encoding_gzdecode_verify(const char *data, size_t data_len, const char *decoded, size_t decoded_len, int error_level TSRMLS_DC);

#define http_encoding_gzencode(l, m, d, dl, r, rl) _http_encoding_gzencode((l), (m), (d), (dl), (r), (rl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_gzencode(int level, int mtime, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC);
#define http_encoding_gzdecode(d, dl, r, rl) _http_encoding_gzdecode((d), (dl), (r), (rl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_gzdecode(const char *data, size_t data_len, char **decoded, size_t *decoded_len TSRMLS_DC);
#define http_encoding_deflate(l, h, d, dl, r, rl) _http_encoding_deflate((l), (h), (d), (dl), (r), (rl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_deflate(int level, int zhdr, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC);
#define http_encoding_inflate(d, dl, r, rl) _http_encoding_inflate((d), (dl), (r), (rl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_encoding_inflate(const char *data, size_t data_len, char **decoded, size_t *decoded_len TSRMLS_DC);

#endif /* HTTP_HAVE_ZLIB */

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

