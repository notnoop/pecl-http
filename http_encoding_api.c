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

#define HTTP_WANT_ZLIB
#include "php_http.h"

#include "php_http_api.h"
#include "php_http_encoding_api.h"
#include "php_http_send_api.h"
#include "php_http_headers_api.h"

/* {{{ */
#ifdef HTTP_HAVE_ZLIB
PHP_MINIT_FUNCTION(http_encoding)
{
	HTTP_LONG_CONSTANT("HTTP_DEFLATE_LEVEL_DEF", HTTP_DEFLATE_LEVEL_DEF);
	HTTP_LONG_CONSTANT("HTTP_DEFLATE_LEVEL_MIN", HTTP_DEFLATE_LEVEL_MIN);
	HTTP_LONG_CONSTANT("HTTP_DEFLATE_LEVEL_MAX", HTTP_DEFLATE_LEVEL_MAX);
	HTTP_LONG_CONSTANT("HTTP_DEFLATE_TYPE_ZLIB", HTTP_DEFLATE_TYPE_ZLIB);
	HTTP_LONG_CONSTANT("HTTP_DEFLATE_TYPE_GZIP", HTTP_DEFLATE_TYPE_GZIP);
	HTTP_LONG_CONSTANT("HTTP_DEFLATE_TYPE_RAW", HTTP_DEFLATE_TYPE_RAW);
	HTTP_LONG_CONSTANT("HTTP_DEFLATE_STRATEGY_DEF", HTTP_DEFLATE_STRATEGY_DEF);
	HTTP_LONG_CONSTANT("HTTP_DEFLATE_STRATEGY_FILT", HTTP_DEFLATE_STRATEGY_FILT);
	HTTP_LONG_CONSTANT("HTTP_DEFLATE_STRATEGY_HUFF", HTTP_DEFLATE_STRATEGY_HUFF);
	HTTP_LONG_CONSTANT("HTTP_DEFLATE_STRATEGY_RLE", HTTP_DEFLATE_STRATEGY_RLE);
	HTTP_LONG_CONSTANT("HTTP_DEFLATE_STRATEGY_FIXED", HTTP_DEFLATE_STRATEGY_FIXED);
	return SUCCESS;
}

PHP_RINIT_FUNCTION(http_encoding)
{
	if (HTTP_G->send.inflate.start_auto) {
		php_ob_set_internal_handler(_http_ob_inflatehandler, HTTP_INFLATE_BUFFER_SIZE, "http inflate", 0 TSRMLS_CC);
	}
	if (HTTP_G->send.deflate.start_auto) {
		php_ob_set_internal_handler(_http_ob_deflatehandler, HTTP_DEFLATE_BUFFER_SIZE, "http deflate", 0 TSRMLS_CC);
	}
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(http_encoding)
{
	if (HTTP_G->send.deflate.stream) {
		http_encoding_deflate_stream_free((http_encoding_stream **) &HTTP_G->send.deflate.stream);
	}
	if (HTTP_G->send.inflate.stream) {
		http_encoding_inflate_stream_free((http_encoding_stream **) &HTTP_G->send.inflate.stream);
	}
	return SUCCESS;
}
#endif
/* }}} */

/* {{{ eol_match(char **, int *) */
static inline int eol_match(char **line, int *eol_len)
{
	char *ptr = *line;
	
	while (' ' == *ptr) ++ptr;

	if (ptr == http_locate_eol(*line, eol_len)) {
		*line = ptr;
		return 1;
	} else {
		return 0;
	}
}
/* }}} */
			
/* {{{ char *http_encoding_dechunk(char *, size_t, char **, size_t *) */
PHP_HTTP_API const char *_http_encoding_dechunk(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	int eol_len = 0;
	char *n_ptr = NULL;
	const char *e_ptr = encoded;
	
	*decoded_len = 0;
	*decoded = ecalloc(1, encoded_len);

	while ((encoded + encoded_len - e_ptr) > 0) {
		ulong chunk_len = 0, rest;

		chunk_len = strtoul(e_ptr, &n_ptr, 16);

		/* we could not read in chunk size */
		if (n_ptr == e_ptr) {
			/*
			 * if this is the first turn and there doesn't seem to be a chunk
			 * size at the begining of the body, do not fail on apparently
			 * not encoded data and return a copy
			 */
			if (e_ptr == encoded) {
				http_error(HE_NOTICE, HTTP_E_ENCODING, "Data does not seem to be chunked encoded");
				memcpy(*decoded, encoded, encoded_len);
				*decoded_len = encoded_len;
				return encoded + encoded_len;
			} else {
				efree(*decoded);
				http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Expected chunk size at pos %tu of %zu but got trash", n_ptr - encoded, encoded_len);
				return NULL;
			}
		}
		
		/* reached the end */
		if (!chunk_len) {
			/* move over '0' chunked encoding terminator */
			while (*e_ptr == '0') ++e_ptr;
			break;
		}

		/* there should be CRLF after the chunk size, but we'll ignore SP+ too */
		if (*n_ptr && !eol_match(&n_ptr, &eol_len)) {
			if (eol_len == 2) {
				http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Expected CRLF at pos %tu of %zu but got 0x%02X 0x%02X", n_ptr - encoded, encoded_len, *n_ptr, *(n_ptr + 1));
			} else {
				http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Expected LF at pos %tu of %zu but got 0x%02X", n_ptr - encoded, encoded_len, *n_ptr);
			}
		}
		n_ptr += eol_len;
		
		/* chunk size pretends more data than we actually got, so it's probably a truncated message */
		if (chunk_len > (rest = encoded + encoded_len - n_ptr)) {
			http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Truncated message: chunk size %lu exceeds remaining data size %lu at pos %tu of %zu", chunk_len, rest, n_ptr - encoded, encoded_len);
			chunk_len = rest;
		}

		/* copy the chunk */
		memcpy(*decoded + *decoded_len, n_ptr, chunk_len);
		*decoded_len += chunk_len;
		
		if (chunk_len == rest) {
			e_ptr = n_ptr + chunk_len;
			break;
		} else {
			/* advance to next chunk */
			e_ptr = n_ptr + chunk_len + eol_len;
		}
	}

	return e_ptr;
}
/* }}} */

/* {{{ int http_encoding_response_start(size_t) */
PHP_HTTP_API int _http_encoding_response_start(size_t content_length TSRMLS_DC)
{
	if (	php_ob_handler_used("ob_gzhandler" TSRMLS_CC) ||
			php_ob_handler_used("zlib output compression" TSRMLS_CC)) {
		HTTP_G->send.deflate.encoding = 0;
	} else {
		if (!HTTP_G->send.deflate.encoding) {
			/* emit a content-length header */
			if (content_length) {
				char cl_header_str[128];
				size_t cl_header_len;
				cl_header_len = snprintf(cl_header_str, lenof(cl_header_str), "Content-Length: %zu", content_length);
				http_send_header_string_ex(cl_header_str, cl_header_len, 1);
			}
		} else {
#ifndef HTTP_HAVE_ZLIB
			HTTP_G->send.deflate.encoding = 0;
			php_start_ob_buffer_named("ob_gzhandler", 0, 0 TSRMLS_CC);
#else
			HashTable *selected;
			zval zsupported;
			
			INIT_PZVAL(&zsupported);
			array_init(&zsupported);
			add_next_index_stringl(&zsupported, "gzip", lenof("gzip"), 1);
			add_next_index_stringl(&zsupported, "x-gzip", lenof("x-gzip"), 1);
			add_next_index_stringl(&zsupported, "deflate", lenof("deflate"), 1);
			
			HTTP_G->send.deflate.encoding = 0;
			
			if ((selected = http_negotiate_encoding(&zsupported))) {
				STATUS hs = FAILURE;
				char *encoding = NULL;
				ulong idx;
				
				if (HASH_KEY_IS_STRING == zend_hash_get_current_key(selected, &encoding, &idx, 0) && encoding) {
					if (!strcmp(encoding, "gzip") || !strcmp(encoding, "x-gzip")) {
						if (SUCCESS == (hs = http_send_header_string("Content-Encoding: gzip"))) {
							HTTP_G->send.deflate.encoding = HTTP_ENCODING_GZIP;
						}
					} else if (!strcmp(encoding, "deflate")) {
						if (SUCCESS == (hs = http_send_header_string("Content-Encoding: deflate"))) {
							HTTP_G->send.deflate.encoding = HTTP_ENCODING_DEFLATE;
						}
					}
					if (SUCCESS == hs) {
						http_send_header_string("Vary: Accept-Encoding");
					}
				}
				
				zend_hash_destroy(selected);
				FREE_HASHTABLE(selected);
			}
			
			zval_dtor(&zsupported);
			return HTTP_G->send.deflate.encoding;
#endif
		}
	}
	return 0;
}
/* }}} */

#ifdef HTTP_HAVE_ZLIB

/* {{{ */
#define HTTP_DEFLATE_LEVEL_SET(flags, level) \
	switch (flags & 0xf) \
	{ \
		default: \
			if ((flags & 0xf) < 10) { \
				level = flags & 0xf; \
				break; \
			} \
		case HTTP_DEFLATE_LEVEL_DEF: \
			level = Z_DEFAULT_COMPRESSION; \
		break; \
	}
	
#define HTTP_DEFLATE_WBITS_SET(flags, wbits) \
	switch (flags & 0xf0) \
	{ \
		case HTTP_DEFLATE_TYPE_GZIP: \
			wbits = HTTP_WINDOW_BITS_GZIP; \
		break; \
		case HTTP_DEFLATE_TYPE_RAW: \
			wbits = HTTP_WINDOW_BITS_RAW; \
		break; \
		default: \
			wbits = HTTP_WINDOW_BITS_ZLIB; \
		break; \
	}

#define HTTP_INFLATE_WBITS_SET(flags, wbits) \
	if (flags & HTTP_INFLATE_TYPE_RAW) { \
		wbits = HTTP_WINDOW_BITS_RAW; \
	} else { \
		wbits = HTTP_WINDOW_BITS_ANY; \
	}

#define HTTP_DEFLATE_STRATEGY_SET(flags, strategy) \
	switch (flags & 0xf00) \
	{ \
		case HTTP_DEFLATE_STRATEGY_FILT: \
			strategy = Z_FILTERED; \
		break; \
		case HTTP_DEFLATE_STRATEGY_HUFF: \
			strategy = Z_HUFFMAN_ONLY; \
		break; \
		case HTTP_DEFLATE_STRATEGY_RLE: \
			strategy = Z_RLE; \
		break; \
		case HTTP_DEFLATE_STRATEGY_FIXED: \
			strategy = Z_FIXED; \
		break; \
		default: \
			strategy = Z_DEFAULT_STRATEGY; \
		break; \
	}

#define HTTP_WINDOW_BITS_ZLIB	0x0000000f
#define HTTP_WINDOW_BITS_GZIP	0x0000001f
#define HTTP_WINDOW_BITS_ANY	0x0000002f
#define HTTP_WINDOW_BITS_RAW	-0x000000f
/* }}} */

/* {{{ STATUS http_encoding_deflate(int, char *, size_t, char **, size_t *) */
PHP_HTTP_API STATUS _http_encoding_deflate(int flags, const char *data, size_t data_len, char **encoded, size_t *encoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	int status, level, wbits, strategy;
	z_stream Z;
	
	HTTP_DEFLATE_LEVEL_SET(flags, level);
	HTTP_DEFLATE_WBITS_SET(flags, wbits);
	HTTP_DEFLATE_STRATEGY_SET(flags, strategy);
	
	memset(&Z, 0, sizeof(z_stream));
	*encoded = NULL;
	*encoded_len = 0;
	
	status = deflateInit2(&Z, level, Z_DEFLATED, wbits, MAX_MEM_LEVEL, strategy);
	if (Z_OK == status) {
		*encoded_len = HTTP_DEFLATE_BUFFER_SIZE_GUESS(data_len);
		*encoded = emalloc_rel(*encoded_len);
		
		Z.next_in = (Bytef *) data;
		Z.next_out = (Bytef *) *encoded;
		Z.avail_in = data_len;
		Z.avail_out = *encoded_len;
		
		status = deflate(&Z, Z_FINISH);
		deflateEnd(&Z);
		
		if (Z_STREAM_END == status) {
			/* size buffer down to actual length */
			*encoded = erealloc_rel(*encoded, Z.total_out + 1);
			(*encoded)[*encoded_len = Z.total_out] = '\0';
			return SUCCESS;
		} else {
			STR_SET(*encoded, NULL);
			*encoded_len = 0;
		}
	}
	
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not deflate data: %s", zError(status));
	return FAILURE;
}
/* }}} */

/* {{{ STATUS http_encoding_inflate(char *, size_t, char **, size_t) */
PHP_HTTP_API STATUS _http_encoding_inflate(const char *data, size_t data_len, char **decoded, size_t *decoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	int status, round = 0, wbits = HTTP_WINDOW_BITS_ANY;
	z_stream Z;
	phpstr buffer;
	
	memset(&Z, 0, sizeof(z_stream));
	*decoded = NULL;
	*decoded_len = 0;
	
	phpstr_init_ex(&buffer, data_len << 2, PHPSTR_INIT_PREALLOC);
	buffer.size = data_len;
	
retry_inflate:			
	status = inflateInit2(&Z, wbits);
	if (Z_OK == status) {
		Z.next_in = (Bytef *) data;
		Z.avail_in = data_len;
		
		do {
			phpstr_resize(&buffer, data_len << 2);

			do {
				Z.avail_out = (buffer.free -= Z.total_out - buffer.used);
				Z.next_out = (Bytef *) buffer.data + (buffer.used = Z.total_out);
				status = inflate(&Z, Z_NO_FLUSH);
			} while (Z_OK == status);
		} while (Z_BUF_ERROR == status && ++round < HTTP_INFLATE_ROUNDS);
		
		if (Z_DATA_ERROR == status && HTTP_WINDOW_BITS_ANY == wbits) {
			/* raw deflated data? */
			inflateEnd(&Z);
			wbits = HTTP_WINDOW_BITS_RAW;
			goto retry_inflate;
		}
		
		inflateEnd(&Z);
		
		if (Z_STREAM_END == status) {
			*decoded_len = Z.total_out;
			*decoded = erealloc_rel(buffer.data, *decoded_len + 1);
			(*decoded)[*decoded_len] = '\0';
			return SUCCESS;
		} else {
			phpstr_dtor(&buffer);
		}
	}
	
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not inflate data: %s", zError(status));
	return FAILURE;
}
/* }}} */

/* {{{ http_encoding_stream *_http_encoding_deflate_stream_init(http_encoding_stream *, int) */
PHP_HTTP_API http_encoding_stream *_http_encoding_deflate_stream_init(http_encoding_stream *s, int flags ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	int status, level, wbits, strategy, free_stream;
	
	if ((free_stream = !s)) {
		s = pemalloc_rel(sizeof(http_encoding_stream), (flags & HTTP_ENCODING_STREAM_PERSISTENT));
	}
	memset(s, 0, sizeof(http_encoding_stream));
	s->flags = flags;
	
	HTTP_DEFLATE_LEVEL_SET(flags, level);
	HTTP_DEFLATE_WBITS_SET(flags, wbits);
	HTTP_DEFLATE_STRATEGY_SET(flags, strategy);
	
	if (Z_OK == (status = deflateInit2(&s->stream, level, Z_DEFLATED, wbits, MAX_MEM_LEVEL, strategy))) {
		int p = (flags & HTTP_ENCODING_STREAM_PERSISTENT) ? PHPSTR_INIT_PERSISTENT:0;
		
		if ((s->stream.opaque = phpstr_init_ex(NULL, HTTP_DEFLATE_BUFFER_SIZE, p))) {
			return s;
		}
		deflateEnd(&s->stream);
		status = Z_MEM_ERROR;
	}
	
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Failed to initialize deflate encoding stream: %s", zError(status));
	if (free_stream) {
		efree(s);
	}
	return NULL;
}
/* }}} */

/* {{{ http_encoding_stream *http_encoding_inflate_stream_init(http_encoding_stream *, int) */
PHP_HTTP_API http_encoding_stream *_http_encoding_inflate_stream_init(http_encoding_stream *s, int flags ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	int status, wbits, free_stream;
	
	if ((free_stream = !s)) {
		s = pemalloc_rel(sizeof(http_encoding_stream), (flags & HTTP_ENCODING_STREAM_PERSISTENT));
	}
	memset(s, 0, sizeof(http_encoding_stream));
	s->flags = flags;
	
	HTTP_INFLATE_WBITS_SET(flags, wbits);
	
	if (Z_OK == (status = inflateInit2(&s->stream, wbits))) {
		int p = (flags & HTTP_ENCODING_STREAM_PERSISTENT) ? PHPSTR_INIT_PERSISTENT:0;
		
		if ((s->stream.opaque = phpstr_init_ex(NULL, HTTP_DEFLATE_BUFFER_SIZE, p))) {
			return s;
		}
		inflateEnd(&s->stream);
		status = Z_MEM_ERROR;
	}
	
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Failed to initialize inflate stream: %s", zError(status));
	if (free_stream) {
		efree(s);
	}
	return NULL;
}
/* }}} */

/* {{{ STATUS http_encoding_deflate_stream_update(http_encoding_stream *, char *, size_t, char **, size_t *) */
PHP_HTTP_API STATUS _http_encoding_deflate_stream_update(http_encoding_stream *s, const char *data, size_t data_len, char **encoded, size_t *encoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	int status;
	
	/* append input to our buffer */
	phpstr_append(PHPSTR(s->stream.opaque), data, data_len);
	
	s->stream.next_in = (Bytef *) PHPSTR_VAL(s->stream.opaque);
	s->stream.avail_in = PHPSTR_LEN(s->stream.opaque);
	
	/* deflate */
	*encoded_len = HTTP_DEFLATE_BUFFER_SIZE_GUESS(data_len);
	*encoded = emalloc_rel(*encoded_len);
	s->stream.avail_out = *encoded_len;
	s->stream.next_out = (Bytef *) *encoded;
	
	switch (status = deflate(&s->stream, Z_NO_FLUSH))
	{
		case Z_OK:
		case Z_STREAM_END:
			/* cut processed chunk off the buffer */
			phpstr_cut(PHPSTR(s->stream.opaque), 0, PHPSTR_LEN(s->stream.opaque) - s->stream.avail_in);
			
			/* size buffer down to actual size */
			*encoded_len -= s->stream.avail_out;
			*encoded = erealloc_rel(*encoded, *encoded_len + 1);
			(*encoded)[*encoded_len] = '\0';
			return SUCCESS;
		break;
	}
	
	STR_SET(*encoded, NULL);
	*encoded_len = 0;
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Failed to update deflate stream: %s", zError(status));
	return FAILURE;
}
/* }}} */

/* {{{ STATUS http_encoding_inflate_stream_update(http_encoding_stream *, char *, size_t, char **, size_t *) */
PHP_HTTP_API STATUS _http_encoding_inflate_stream_update(http_encoding_stream *s, const char *data, size_t data_len, char **decoded, size_t *decoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	int status, round = 0;
	
	/* append input to buffer */
	phpstr_append(PHPSTR(s->stream.opaque), data, data_len);
	
	/* for realloc() */
	*decoded = NULL;
	*decoded_len = data_len << 1;
	
	/* inflate */
	do {
		*decoded_len <<= 1;
		*decoded = erealloc_rel(*decoded, *decoded_len);
		
retry_raw_inflate:
		s->stream.next_in = (Bytef *) PHPSTR_VAL(s->stream.opaque);
		s->stream.avail_in = PHPSTR_LEN(s->stream.opaque);
	
		s->stream.next_out = (Bytef *) *decoded;
		s->stream.avail_out = *decoded_len;
		
		switch (status = inflate(&s->stream, Z_NO_FLUSH))
		{
			case Z_OK:
			case Z_STREAM_END:
				/* cut off */
				phpstr_cut(PHPSTR(s->stream.opaque), 0, PHPSTR_LEN(s->stream.opaque) - s->stream.avail_in);
				
				/* size down */
				*decoded_len -= s->stream.avail_out;
				*decoded = erealloc_rel(*decoded, *decoded_len + 1);
				(*decoded)[*decoded_len] = '\0';
				return SUCCESS;
			break;
			
			case Z_DATA_ERROR:
				/* raw deflated data ? */
				if (!(s->flags & HTTP_INFLATE_TYPE_RAW) && !s->stream.total_out) {
					inflateEnd(&s->stream);
					s->flags |= HTTP_INFLATE_TYPE_RAW;
					inflateInit2(&s->stream, HTTP_WINDOW_BITS_RAW);
					goto retry_raw_inflate;
				}
			break;
		}
	} while (Z_BUF_ERROR == status && ++round < HTTP_INFLATE_ROUNDS);
	
	STR_SET(*decoded, NULL);
	*decoded_len = 0;
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Failed to update inflate stream: %s", zError(status));
	return FAILURE;
}
/* }}} */

/* {{{ STATUS http_encoding_deflate_stream_flush(http_encoding_stream *, char **, size_t *) */
PHP_HTTP_API STATUS _http_encoding_deflate_stream_flush(http_encoding_stream *s, char **encoded, size_t *encoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	int status;
	
	*encoded_len = HTTP_DEFLATE_BUFFER_SIZE;
	*encoded = emalloc_rel(*encoded_len);
	
	s->stream.avail_in = 0;
	s->stream.next_in = NULL;
	s->stream.avail_out = *encoded_len;
	s->stream.next_out = (Bytef *) *encoded;
	
	switch (status = deflate(&s->stream, Z_SYNC_FLUSH))
	{
		case Z_OK:
		case Z_STREAM_END:
			*encoded_len = HTTP_DEFLATE_BUFFER_SIZE - s->stream.avail_out;
			*encoded = erealloc_rel(*encoded, *encoded_len + 1);
			(*encoded)[*encoded_len] = '\0';
			return SUCCESS;
		break;
	}
	
	STR_SET(*encoded, NULL);
	*encoded_len = 0;
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Failed to flush deflate stream: %s", zError(status));
	return FAILURE;
}
/* }}} */

/* {{{ STATUS http_encoding_inflate_straem_flush(http_encoding_stream *, char **, size_t *) */
PHP_HTTP_API STATUS _http_encoding_inflate_stream_flush(http_encoding_stream *s, char **decoded, size_t *decoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	/* noop */
	*decoded = estrndup("", *decoded_len = 0);
	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_encoding_deflate_stream_finish(http_encoding_stream *, char **, size_t *) */
PHP_HTTP_API STATUS _http_encoding_deflate_stream_finish(http_encoding_stream *s, char **encoded, size_t *encoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	int status;
	
	*encoded_len = HTTP_DEFLATE_BUFFER_SIZE;
	*encoded = emalloc_rel(*encoded_len);
	
	/* deflate remaining input */
	s->stream.next_in = (Bytef *) PHPSTR_VAL(s->stream.opaque);
	s->stream.avail_in = PHPSTR_LEN(s->stream.opaque);
	
	s->stream.avail_out = *encoded_len;
	s->stream.next_out = (Bytef *) *encoded;
	
	do {
		status = deflate(&s->stream, Z_FINISH);
	} while (Z_OK == status);
	
	if (Z_STREAM_END == status) {
		/* cut processed intp off */
		phpstr_cut(PHPSTR(s->stream.opaque), 0, PHPSTR_LEN(s->stream.opaque) - s->stream.avail_in);
		
		/* size down */
		*encoded_len -= s->stream.avail_out;
		*encoded = erealloc_rel(*encoded, *encoded_len + 1);
		(*encoded)[*encoded_len] = '\0';
		return SUCCESS;
	}
	
	STR_SET(*encoded, NULL);
	*encoded_len = 0;
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Failed to finish deflate stream: %s", zError(status));
	return FAILURE;
}
/* }}} */

/* {{{ STATUS http_encoding_inflate_stream_finish(http_encoding_stream *, char **, size_t *) */
PHP_HTTP_API STATUS _http_encoding_inflate_stream_finish(http_encoding_stream *s, char **decoded, size_t *decoded_len ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	int status;
	
	*decoded_len = (PHPSTR_LEN(s->stream.opaque) + 1) * HTTP_INFLATE_ROUNDS;
	*decoded = emalloc_rel(*decoded_len);
	
	/* inflate remaining input */
	s->stream.next_in = (Bytef *) PHPSTR_VAL(s->stream.opaque);
	s->stream.avail_in = PHPSTR_LEN(s->stream.opaque);
	
	s->stream.avail_out = *decoded_len;
	s->stream.next_out = (Bytef *) *decoded;
	
	if (Z_STREAM_END == (status = inflate(&s->stream, Z_FINISH))) {
		/* cut processed input off */
		phpstr_cut(PHPSTR(s->stream.opaque), 0, PHPSTR_LEN(s->stream.opaque) - s->stream.avail_in);
		
		/* size down */
		*decoded_len -= s->stream.avail_out;
		*decoded = erealloc_rel(*decoded, *decoded_len + 1);
		(*decoded)[*decoded_len] = '\0';
		return SUCCESS;
	}
	
	STR_SET(*decoded, NULL);
	*decoded_len = 0;
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Failed to finish inflate stream: %s", zError(status));
	return FAILURE;
}
/* }}} */

/* {{{ void http_encoding_deflate_stream_dtor(http_encoding_stream *) */
PHP_HTTP_API void _http_encoding_deflate_stream_dtor(http_encoding_stream *s TSRMLS_DC)
{
	if (s) {
		if (s->stream.opaque) {
			phpstr_free((phpstr **) &s->stream.opaque);
		}
		deflateEnd(&s->stream);
	}
}
/* }}} */

/* {{{ void http_encoding_inflate_stream_dtor(http_encoding_stream *) */
PHP_HTTP_API void _http_encoding_inflate_stream_dtor(http_encoding_stream *s TSRMLS_DC)
{
	if (s) {
		if (s->stream.opaque) {
			phpstr_free((phpstr **) &s->stream.opaque);
		}
		inflateEnd(&s->stream);
	}
}
/* }}} */

/* {{{ void http_encoding_deflate_stream_free(http_encoding_stream **) */
PHP_HTTP_API void _http_encoding_deflate_stream_free(http_encoding_stream **s TSRMLS_DC)
{
	if (s) {
		http_encoding_deflate_stream_dtor(*s);
		if (*s) {
			pefree(*s, (*s)->flags & HTTP_ENCODING_STREAM_PERSISTENT);
		}
		*s = NULL;
	}
}
/* }}} */

/* {{{ void http_encoding_inflate_stream_free(http_encoding_stream **) */
PHP_HTTP_API void _http_encoding_inflate_stream_free(http_encoding_stream **s TSRMLS_DC)
{
	if (s) {
		http_encoding_inflate_stream_dtor(*s);
		if (*s) {
			pefree(*s, (*s)->flags & HTTP_ENCODING_STREAM_PERSISTENT);
		}
		*s = NULL;
	}
}
/* }}} */

/* {{{ void http_ob_deflatehandler(char *, uint, char **, uint *, int) */
void _http_ob_deflatehandler(char *output, uint output_len, char **handled_output, uint *handled_output_len, int mode TSRMLS_DC)
{
	*handled_output = NULL;
	*handled_output_len = 0;
	
	if (mode & PHP_OUTPUT_HANDLER_START) {
		int flags;
		
		if (HTTP_G->send.deflate.stream) {
			zend_error(E_ERROR, "ob_deflatehandler() can only be used once");
			return;
		}
		
		HTTP_G->send.deflate.encoding = !0;
		
		switch (http_encoding_response_start(0))
		{
			case HTTP_ENCODING_GZIP:
				flags = HTTP_DEFLATE_TYPE_GZIP;
			break;
			
			case HTTP_ENCODING_DEFLATE:
				flags = HTTP_DEFLATE_TYPE_ZLIB;
			break;
			
			default:
				goto deflate_passthru_plain;
			break;
		}
		
		flags |= (HTTP_G->send.deflate.start_flags &~ 0xf0);
		HTTP_G->send.deflate.stream = http_encoding_deflate_stream_init(NULL, flags);
	}
	
	if (HTTP_G->send.deflate.stream) {
		http_encoding_deflate_stream_update((http_encoding_stream *) HTTP_G->send.deflate.stream, output, output_len, handled_output, handled_output_len);
		
		if (mode & PHP_OUTPUT_HANDLER_END) {
			char *remaining = NULL;
			size_t remaining_len = 0;
			
			http_encoding_deflate_stream_finish((http_encoding_stream *) HTTP_G->send.deflate.stream, &remaining, &remaining_len);
			http_encoding_deflate_stream_free((http_encoding_stream **) &HTTP_G->send.deflate.stream);
			if (remaining) {
				*handled_output = erealloc(*handled_output, *handled_output_len + remaining_len + 1);
				memcpy(*handled_output + *handled_output_len, remaining, remaining_len);
				(*handled_output)[*handled_output_len += remaining_len] = '\0';
				efree(remaining);
			}
		}
	} else {
deflate_passthru_plain:
		*handled_output = estrndup(output, *handled_output_len = output_len);
	}
}
/* }}} */

/* {{{ void http_ob_inflatehandler(char *, uint, char **, uint *, int) */
void _http_ob_inflatehandler(char *output, uint output_len, char **handled_output, uint *handled_output_len, int mode TSRMLS_DC)
{
	*handled_output = NULL;
	*handled_output_len = 0;
	
	if (mode & PHP_OUTPUT_HANDLER_START) {
		if (HTTP_G->send.inflate.stream) {
			zend_error(E_ERROR, "ob_inflatehandler() can only be used once");
			return;
		}
		HTTP_G->send.inflate.stream = http_encoding_inflate_stream_init(NULL, (HTTP_G->send.inflate.start_flags &~ 0xf0));
	}
	
	if (HTTP_G->send.inflate.stream) {
		http_encoding_inflate_stream_update((http_encoding_stream *) HTTP_G->send.inflate.stream, output, output_len, handled_output, handled_output_len);
		
		if (mode & PHP_OUTPUT_HANDLER_END) {
			char *remaining = NULL;
			size_t remaining_len = 0;
			
			http_encoding_inflate_stream_finish((http_encoding_stream *) HTTP_G->send.inflate.stream, &remaining, &remaining_len);
			http_encoding_inflate_stream_free((http_encoding_stream **) &HTTP_G->send.inflate.stream);
			if (remaining) {
				*handled_output = erealloc(*handled_output, *handled_output_len + remaining_len + 1);
				memcpy(*handled_output + *handled_output_len, remaining, remaining_len);
				(*handled_output)[*handled_output_len += remaining_len] = '\0';
				efree(remaining);
			}
		}
	} else {
		*handled_output = estrndup(output, *handled_output_len = output_len);
	}
}
/* }}} */

#endif /* HTTP_HAVE_ZLIB */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

