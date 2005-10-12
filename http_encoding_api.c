/*
   +----------------------------------------------------------------------+
   | PECL :: http                                                         |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.0 of the PHP license, that  |
   | is bundled with this package in the file LICENSE, and is available   |
   | through the world-wide-web at http://www.php.net/license/3_0.txt.    |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2004-2005 Michael Wallner <mike@php.net>               |
   +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "php.h"

#include "php_http_encoding_api.h"
#include "php_http.h"
#include "php_http_api.h"

ZEND_EXTERN_MODULE_GLOBALS(http);

/* {{{ char *http_encoding_dechunk(char *, size_t, char **, size_t *) */
PHP_HTTP_API const char *_http_encoding_dechunk(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	const char *e_ptr;
	char *d_ptr;
	long rest;
	
	*decoded_len = 0;
	*decoded = ecalloc(1, encoded_len);
	d_ptr = *decoded;
	e_ptr = encoded;

	while ((rest = encoded + encoded_len - e_ptr) > 0) {
		long chunk_len = 0;
		int EOL_len = 0, eol_mismatch = 0;
		char *n_ptr;

		chunk_len = strtol(e_ptr, &n_ptr, 16);

		/* check if:
		 * - we could not read in chunk size
		 * - we got a negative chunk size
		 * - chunk size is greater then remaining size
		 * - chunk size is not followed by (CR)LF|NUL
		 */
		if (	(n_ptr == e_ptr) ||	(chunk_len < 0) || (chunk_len > rest) || 
				(*n_ptr && (eol_mismatch = (n_ptr != http_locate_eol(e_ptr, &EOL_len))))) {
			/* don't fail on apperently not encoded data */
			if (e_ptr == encoded) {
				memcpy(*decoded, encoded, encoded_len);
				*decoded_len = encoded_len;
				return encoded + encoded_len;
			} else {
				efree(*decoded);
				if (eol_mismatch) {
					if (EOL_len == 2) {
						http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Invalid character (expected 0x0D 0x0A; got: 0x%X 0x%X)", *n_ptr, *(n_ptr + 1));
					} else {
						http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Invalid character (expected 0x0A; got: 0x%X)", *n_ptr);
					}
				} else {
					char *error = estrndup(n_ptr, strcspn(n_ptr, "\r\n "));
					http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Invalid chunk size: '%s' at pos %d", error, n_ptr - encoded);
					efree(error);
				}
				return NULL;
			}
		} else {
			e_ptr = n_ptr;
		}

		/* reached the end */
		if (!chunk_len) {
			break;
		}

		memcpy(d_ptr, e_ptr += EOL_len, chunk_len);
		d_ptr += chunk_len;
		e_ptr += chunk_len + EOL_len;
		*decoded_len += chunk_len;
	}

	return e_ptr;
}
/* }}} */

#ifdef HTTP_HAVE_ZLIB
#include <zlib.h>

/* max count of uncompress trials, alloc_size <<= 2 for each try */
#define HTTP_GZMAXTRY 10
/* safe padding */
#define HTTP_GZSAFPAD 10
/* add 1% extra space in case we need to encode widely differing (binary) data */
#define HTTP_GZBUFLEN(l) (l + (l / 100) + HTTP_GZSAFPAD)

static const char http_gzencode_header[] = {
	(const char) 0x1f,			// fixed value
	(const char) 0x8b,			// fixed value
	(const char) Z_DEFLATED,	// compression algorithm
	(const char) 0,				// none of the possible flags defined by the GZIP "RFC"
	(const char) 0,				// no MTIME available (4 bytes)
	(const char) 0,				// =*=
	(const char) 0,				// =*=
	(const char) 0,				// =*=
	(const char) 0,				// two possible flag values for 9 compression levels? o_O
	(const char) 0x03			// assume *nix OS
};

inline void http_init_gzencode_buffer(z_stream *Z, const char *data, size_t data_len, char **buf_ptr)
{
	Z->zalloc = Z_NULL;
	Z->zfree  = Z_NULL;
	Z->opaque = Z_NULL;
	
	Z->next_in   = (Bytef *) data;
	Z->avail_in  = data_len;
	Z->avail_out = HTTP_GZBUFLEN(data_len) + HTTP_GZSAFPAD - 1;
	
	*buf_ptr = emalloc(HTTP_GZBUFLEN(data_len) + sizeof(http_gzencode_header) + HTTP_GZSAFPAD);
	memcpy(*buf_ptr, http_gzencode_header, sizeof(http_gzencode_header));
	
	Z->next_out = *buf_ptr + sizeof(http_gzencode_header);
}

inline void http_init_deflate_buffer(z_stream *Z, const char *data, size_t data_len, char **buf_ptr)
{
	Z->zalloc = Z_NULL;
	Z->zfree  = Z_NULL;
	Z->opaque = Z_NULL;

	Z->data_type = Z_UNKNOWN;
	Z->next_in   = (Bytef *) data;
	Z->avail_in  = data_len;
	Z->avail_out = HTTP_GZBUFLEN(data_len) - 1;
	Z->next_out  = emalloc(HTTP_GZBUFLEN(data_len));
	
	*buf_ptr = Z->next_out;
}

inline void http_init_uncompress_buffer(size_t data_len, char **buf_ptr, size_t *buf_len, int iteration)
{
	if (!iteration) {
		*buf_len = data_len * 2;
		*buf_ptr = emalloc(*buf_len + 1);
	} else {
		*buf_len <<= 2;
		*buf_ptr = erealloc(*buf_ptr, *buf_len + 1);
	}
}

inline void http_init_inflate_buffer(z_stream *Z, const char *data, size_t data_len, char **buf_ptr, size_t *buf_len, int iteration)
{
	Z->zalloc = Z_NULL;
	Z->zfree  = Z_NULL;
	
	http_init_uncompress_buffer(data_len, buf_ptr, buf_len, iteration);
	
	Z->next_in   = (Bytef *) data;
	Z->avail_in  = data_len;
	Z->avail_out = *buf_len;
	Z->next_out  = *buf_ptr;
}

inline size_t http_finish_buffer(size_t buf_len, char **buf_ptr)
{
	(*buf_ptr)[buf_len] = '\0';
	return buf_len;
}

inline size_t http_finish_gzencode_buffer(z_stream *Z, const char *data, size_t data_len, char **buf_ptr)
{
	unsigned long crc;
	char *trailer;
	
	crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, (const Bytef *) data, data_len);
	
	trailer = *buf_ptr + sizeof(http_gzencode_header) + Z->total_out;
	
	/* LSB */
	trailer[0] = (char) (crc & 0xFF);
	trailer[1] = (char) ((crc >> 8) & 0xFF);
	trailer[2] = (char) ((crc >> 16) & 0xFF);
	trailer[3] = (char) ((crc >> 24) & 0xFF);
	trailer[4] = (char) ((Z->total_in) & 0xFF);
	trailer[5] = (char) ((Z->total_in >> 8) & 0xFF);
	trailer[6] = (char) ((Z->total_in >> 16) & 0xFF);
	trailer[7] = (char) ((Z->total_in >> 24) & 0xFF);
	
	return http_finish_buffer(Z->total_out + sizeof(http_gzencode_header) + 8, buf_ptr);
}

inline STATUS http_verify_gzencode_buffer(const char *data, size_t data_len, const char **encoded, size_t *encoded_len, int error_level TSRMLS_DC)
{
	size_t offset = sizeof(http_gzencode_header);
	
	if (data_len < offset) {
		goto really_bad_gzip_header;
	}
	
	if (data[0] != (const char) 0x1F || data[1] != (const char) 0x8B) {
		http_error_ex(error_level TSRMLS_CC, HTTP_E_ENCODING, "Unrecognized GZIP header start: 0x%02X 0x%02X", (int) data[0], (int) (data[1] & 0xFF));
		return FAILURE;
	}
	
	if (data[2] != (const char) Z_DEFLATED) {
		http_error_ex(error_level TSRMLS_CC, HTTP_E_ENCODING, "Unrecognized compression format (%d)", (int) (data[2] & 0xFF));
		/* still try to decode */
	}
	if ((data[3] & 0x3) == 0x3) {
		if (data_len < offset + 2) {
			goto really_bad_gzip_header;
		}
		/* there are extra fields, the length follows the common header as 2 bytes LSB */
		offset += (unsigned) ((data[offset] & 0xFF));
		offset += 1;
		offset += (unsigned) ((data[offset] & 0xFF) << 8);
		offset += 1;
	}
	if ((data[3] & 0x4) == 0x4) {
		if (data_len <= offset) {
			goto really_bad_gzip_header;
		}
		/* there's a file name */
		offset += strlen(&data[offset]) + 1 /*NUL*/;
	}
	if ((data[3] & 0x5) == 0x5) {
		if (data_len <= offset) {
			goto really_bad_gzip_header;
		}
		/* there's a comment */
		offset += strlen(&data[offset]) + 1 /* NUL */;
	}
	if ((data[3] & 0x2) == 0x2) {
		/* there's a CRC16 of the header */
		offset += 2;
		if (data_len <= offset) {
			goto really_bad_gzip_header;
		} else {
			unsigned long crc, cmp;
			
			cmp =  (unsigned) ((data[offset-2] & 0xFF));
			cmp += (unsigned) ((data[offset-1] & 0xFF) << 8);
			
			crc = crc32(0L, Z_NULL, 0);
			crc = crc32(crc, data, sizeof(http_gzencode_header));
			
			if (cmp != (crc & 0xFFFF)) {
				http_error_ex(error_level TSRMLS_CC, HTTP_E_ENCODING, "GZIP headers CRC checksums so not match (%lu, %lu)", cmp, crc & 0xFFFF);
				return FAILURE;
			}
		}
	}
	
	if (encoded) {
		*encoded = data + offset;
	}
	if (encoded_len) {
		*encoded_len = data_len - offset - 8 /* size of the assumed GZIP footer */;	
	}
	
	return SUCCESS;
	
really_bad_gzip_header:
	http_error(error_level TSRMLS_CC, HTTP_E_ENCODING, "Missing or truncated GZIP header");
	return FAILURE;
}

inline STATUS http_verify_gzdecode_buffer(const char *data, size_t data_len, const char *decoded, size_t decoded_len, int error_level TSRMLS_DC)
{
	STATUS status = SUCCESS;
	unsigned long len, cmp, crc;
	
	crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, (const Bytef *) decoded, decoded_len);
	
	cmp  = (unsigned) ((data[data_len-8] & 0xFF));
	cmp += (unsigned) ((data[data_len-7] & 0xFF) << 8);
	cmp += (unsigned) ((data[data_len-6] & 0xFF) << 16);
	cmp += (unsigned) ((data[data_len-5] & 0xFF) << 24);
	len  = (unsigned) ((data[data_len-4] & 0xFF));
	len += (unsigned) ((data[data_len-3] & 0xFF) << 8);
	len += (unsigned) ((data[data_len-2] & 0xFF) << 16);
	len += (unsigned) ((data[data_len-1] & 0xFF) << 24);
	
	if (cmp != crc) {
		http_error_ex(error_level TSRMLS_CC, HTTP_E_ENCODING, "Could not verify data integrity: CRC checksums do not match (%lu, %lu)", cmp, crc);
		status = FAILURE;
	}
	if (len != decoded_len) {
		http_error_ex(error_level TSRMLS_CC, HTTP_E_ENCODING, "Could not verify data integrity: data sizes do not match (%lu, %lu)", len, decoded_len);
		status = FAILURE;
	}
	return status;
}

PHP_HTTP_API STATUS _http_encoding_gzencode(int level, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC)
{
	z_stream Z;
	STATUS status = Z_OK;
	
	http_init_gzencode_buffer(&Z, data, data_len, encoded);
	
	if (	(Z_OK == (status = deflateInit2(&Z, level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY))) &&
			(Z_STREAM_END == (status = deflate(&Z, Z_FINISH))) &&
			(Z_OK == (status = deflateEnd(&Z)))) {
		*encoded_len = http_finish_gzencode_buffer(&Z, data, data_len, encoded);
		return SUCCESS;
	}
	
	efree(*encoded);
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not gzencode data: %s", zError(status));
	return FAILURE;
}

PHP_HTTP_API STATUS _http_encoding_deflate(int level, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC)
{
	z_stream Z;
	STATUS status = Z_OK;
	
	http_init_deflate_buffer(&Z, data, data_len, encoded);
	
	if (	(Z_OK == (status = deflateInit2(&Z, level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY))) &&
			(Z_STREAM_END == (status = deflate(&Z, Z_FINISH))) &&
			(Z_OK == (status = deflateEnd(&Z)))) {
		*encoded_len = http_finish_buffer(Z.total_out, encoded);
		return SUCCESS;
	}
	
	efree(encoded);
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not deflate data: %s", zError(status));
	return FAILURE;
}

PHP_HTTP_API STATUS _http_encoding_compress(int level, const char *data, size_t data_len, char **encoded, size_t *encoded_len TSRMLS_DC)
{
	STATUS status;
	
	*encoded = emalloc(*encoded_len = HTTP_GZBUFLEN(data_len));
	
	if (Z_OK == (status = compress2(*encoded, encoded_len, data, data_len, level))) {
		http_finish_buffer(*encoded_len, encoded);
		return SUCCESS;
	}
	
	efree(encoded);
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not compress data: %s", zError(status));
	return FAILURE;
}

PHP_HTTP_API STATUS _http_encoding_gzdecode(const char *data, size_t data_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	const char *encoded;
	size_t encoded_len;
	
	if (	(SUCCESS == http_verify_gzencode_buffer(data, data_len, &encoded, &encoded_len, HE_NOTICE)) &&
			(SUCCESS == http_encoding_inflate(encoded, encoded_len, decoded, decoded_len))) {
		http_verify_gzdecode_buffer(data, data_len, *decoded, *decoded_len, HE_NOTICE);
		return SUCCESS;
	}
	
	return FAILURE;
}

PHP_HTTP_API STATUS _http_encoding_inflate(const char *data, size_t data_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	int max = 0;
	STATUS status;
	z_stream Z;
	
	do {
		http_init_inflate_buffer(&Z, data, data_len, decoded, decoded_len, max++);
		if (Z_OK == (status = inflateInit2(&Z, -MAX_WBITS))) {
			if (Z_STREAM_END == (status = inflate(&Z, Z_FINISH))) {
				if (Z_OK == (status = inflateEnd(&Z))) {
					*decoded_len = http_finish_buffer(Z.total_out, decoded);
					return SUCCESS;
				}
			}
		}
	} while (max < HTTP_GZMAXTRY && status == Z_BUF_ERROR);
	
	efree(*decoded);
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not inflate data: %s", zError(status));
	return FAILURE;
}

PHP_HTTP_API STATUS _http_encoding_uncompress(const char *data, size_t data_len, char **decoded, size_t *decoded_len TSRMLS_DC)
{
	int max = 0;
	STATUS status;
	
	do {
		http_init_uncompress_buffer(data_len, decoded, decoded_len, max++);
		if (Z_OK == (status = uncompress(*decoded, decoded_len, data, data_len))) {
			http_finish_buffer(*decoded_len, decoded);
			return SUCCESS;
		}
	} while (max < HTTP_GZMAXTRY && status == Z_BUF_ERROR);
	
	efree(*decoded);
	http_error_ex(HE_WARNING, HTTP_E_ENCODING, "Could not uncompress data: %s", zError(status));
	return FAILURE;
}

#endif /* HTTP_HAVE_ZLIB */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

