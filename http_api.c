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
#include "php_http.h"

#include "php_output.h"
#include "ext/standard/url.h"

#include "php_http_api.h"
#include "php_http_send_api.h"

#ifdef ZEND_ENGINE_2
#	include "php_http_exception_object.h"
#endif

PHP_MINIT_FUNCTION(http_support)
{
	HTTP_LONG_CONSTANT("HTTP_SUPPORT", HTTP_SUPPORT);
	HTTP_LONG_CONSTANT("HTTP_SUPPORT_REQUESTS", HTTP_SUPPORT_REQUESTS);
	HTTP_LONG_CONSTANT("HTTP_SUPPORT_MAGICMIME", HTTP_SUPPORT_MAGICMIME);
	HTTP_LONG_CONSTANT("HTTP_SUPPORT_ENCODINGS", HTTP_SUPPORT_ENCODINGS);
	HTTP_LONG_CONSTANT("HTTP_SUPPORT_SSLREQUESTS", HTTP_SUPPORT_SSLREQUESTS);
	
	return SUCCESS;
}

PHP_HTTP_API long _http_support(long feature)
{
	long support = HTTP_SUPPORT;
	
#ifdef HTTP_HAVE_CURL
	support |= HTTP_SUPPORT_REQUESTS;
#	ifdef HTTP_HAVE_SSL
	support |= HTTP_SUPPORT_SSLREQUESTS;
#	endif
#endif
#ifdef HTTP_HAVE_MAGIC
	support |= HTTP_SUPPORT_MAGICMIME;
#endif
#ifdef HTTP_HAVE_ZLIB
	support |= HTTP_SUPPORT_ENCODINGS;
#endif

	if (feature) {
		return (feature == (support & feature));
	}
	return support;
}

/* char *pretty_key(char *, size_t, zend_bool, zend_bool) */
char *_http_pretty_key(char *key, size_t key_len, zend_bool uctitle, zend_bool xhyphen)
{
	if (key && key_len) {
		size_t i;
		int wasalpha;
		if ((wasalpha = isalpha((int) key[0]))) {
			key[0] = (char) (uctitle ? toupper((int) key[0]) : tolower((int) key[0]));
		}
		for (i = 1; i < key_len; i++) {
			if (isalpha((int) key[i])) {
				key[i] = (char) (((!wasalpha) && uctitle) ? toupper((int) key[i]) : tolower((int) key[i]));
				wasalpha = 1;
			} else {
				if (xhyphen && (key[i] == '_')) {
					key[i] = '-';
				}
				wasalpha = 0;
			}
		}
	}
	return key;
}
/* }}} */

/* {{{ STATUS http_parse_cookie(char *, HashTable *) */
PHP_HTTP_API STATUS _http_parse_cookie(const char *list, HashTable *items TSRMLS_DC)
{
#define ST_QUOTE	1
#define ST_VALUE	2
#define ST_KEY		3
#define ST_ASSIGN	4
#define ST_ADD		5

	zval array;
	int first = 1, st = ST_KEY, keylen = 0, vallen = 0;
	char *s, *c, *key = NULL, *val = NULL;
	
	INIT_ZARR(array, items);
	
	c = s = estrdup(list);
	for(;;) {
#if 0
		char *tk = NULL, *tv = NULL;
		
		if (key) {
			if (keylen) {
				tk= estrndup(key, keylen);
			} else {
				tk = ecalloc(1, 7);
				memcpy(tk, key, 3);
				tk[3]='.'; tk[4]='.'; tk[5]='.';
			}
		}
		if (val) {
			if (vallen) {
				tv = estrndup(val, vallen);
			} else {
				tv = ecalloc(1, 7);
				memcpy(tv, val, 3);
				tv[3]='.'; tv[4]='.'; tv[5]='.';
			}
		}
		fprintf(stderr, "[%6s] %c \"%s=%s\"\n",
				(
						st == ST_QUOTE ? "QUOTE" :
						st == ST_VALUE ? "VALUE" :
						st == ST_KEY ? "KEY" :
						st == ST_ASSIGN ? "ASSIGN" :
						st == ST_ADD ? "ADD":
						"HUH?"
				), *c, tk, tv
		);
		STR_FREE(tk); STR_FREE(tv);
#endif
		switch (st)
		{
			case ST_QUOTE:
				if (*c == '"') {
					if (*(c-1) != '\\') {
						st = ST_ADD;
					} else {
						memmove(c-1, c, strlen(c)+1);
					}
				} else {
					if (!val) {
						val = c;
					}
					if (!*c) {
						--val;
						st = ST_ADD;
					}
				}
			break;
				
			case ST_VALUE:
				switch (*c)
				{
					case '"':
						if (!val) {
							st = ST_QUOTE;
						}
					break;
					
					case ' ':
					break;
					
					case ';':
						goto add;
					break;
				
					case '\0':
						st = ST_ADD;
					break;
					
					default:
						if (!val) {
							val = c;
						}
					break;
				}
			break;
				
			case ST_KEY:
				switch (*c)
				{
					default:
						if (!isalnum(*c)) {
							goto failure;
						}
					case '.':
					case '_':
					case '$':
					case '@':
						if (!key) {
							key = c;
						}
					break;
					
					case ' ':
						if (key) {
							keylen = c - key;
							st = ST_ASSIGN;
						}
					break;
					
					case '=':
						if (key) {
							keylen = c - key;
							st = ST_VALUE;
						} else {
							goto failure;
						}
					break;
					
					case '\0':
						if (key) {
							keylen = c - key;
							st = ST_ADD;
						}
					break;
				}
			break;
				
			case ST_ASSIGN:
				if (*c == '=') {
					st = ST_VALUE;
				} else if (*c == ';') {
					st = ST_ADD;
				} else if (*c != ' ') {
					goto failure;
				}
			break;
				
			case ST_ADD:
			add:
				if (val) {
					vallen = c - val - (*c?1:0);
					while (val[vallen-1] == ' ') --vallen;
				} else {
					val = "";
					vallen = 0;
				}
				if (first) {
					first = 0;
					add_assoc_stringl(&array, "name", key, keylen, 1);
					add_assoc_stringl(&array, "value", val, vallen, 1);
				} else  {
					key = estrndup(key, keylen);
					add_assoc_stringl_ex(&array, key, keylen+1, val, vallen, 1);
					efree(key);
				}
				st = ST_KEY;
				key = val = NULL;
				keylen = vallen = 0;
			break;
		}
		
		if (*c) {
			++c;
		} else if (st == ST_ADD) {
			goto add;
		} else {
			break;
		}
	}
	
	efree(s);
	return SUCCESS;
	
failure:
	http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "Unexpected character (%c) at pos %tu of %zu", *c, c-s, strlen(s));
	efree(s);
	return FAILURE;
}
/* }}} */

/* {{{ void http_error(long, long, char*) */
void _http_error_ex(long type TSRMLS_DC, long code, const char *format, ...)
{
	va_list args;
	
	va_start(args, format);
#ifdef ZEND_ENGINE_2
	if ((type == E_THROW) || (PG(error_handling) == EH_THROW)) {
		char *message;
		
		vspprintf(&message, 0, format, args);
		zend_throw_exception(http_exception_get_for_code(code), message, code TSRMLS_CC);
		efree(message);
	} else
#endif
	php_verror(NULL, "", type, format, args TSRMLS_CC);
	va_end(args);
}
/* }}} */

/* {{{ void http_log(char *, char *, char *) */
void _http_log_ex(char *file, const char *ident, const char *message TSRMLS_DC)
{
	time_t now;
	struct tm nowtm;
	char datetime[20] = {0};
	
	now = HTTP_GET_REQUEST_TIME();
	strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", php_localtime_r(&now, &nowtm));

#define HTTP_LOG_WRITE(file, type, msg) \
	if (file && *file) { \
	 	php_stream *log = php_stream_open_wrapper(file, "ab", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL); \
		 \
		if (log) { \
			php_stream_printf(log TSRMLS_CC, "%s\t[%s]\t%s\t<%s>%s", datetime, type, msg, SG(request_info).request_uri, PHP_EOL); \
			php_stream_close(log); \
		} \
	 \
	}
	
	HTTP_LOG_WRITE(file, ident, message);
	HTTP_LOG_WRITE(HTTP_G->log.composite, ident, message);
}
/* }}} */

static void http_ob_blackhole(char *output, uint output_len, char **handled_output, uint *handled_output_len, int mode TSRMLS_DC)
{
	*handled_output = ecalloc(1,1);
	*handled_output_len = 0;
}

/* {{{ STATUS http_exit(int, char*, char*) */
STATUS _http_exit_ex(int status, char *header, char *body, zend_bool send_header TSRMLS_DC)
{
	if (	(send_header && (SUCCESS != http_send_status_header(status, header))) ||
			(!send_header && status && (SUCCESS != http_send_status(status)))) {
		http_error_ex(HE_WARNING, HTTP_E_HEADER, "Failed to exit with status/header: %d - %s", status, header ? header : "");
		STR_FREE(header);
		STR_FREE(body);
		return FAILURE;
	}
	
	php_end_ob_buffers(0 TSRMLS_CC);
	if ((SUCCESS == sapi_send_headers(TSRMLS_C)) && body) {
		PHPWRITE(body, strlen(body));
	}
	
	switch (status)
	{
		case 301:	http_log(HTTP_G->log.redirect, "301-REDIRECT", header);			break;
		case 302:	http_log(HTTP_G->log.redirect, "302-REDIRECT", header);			break;
		case 303:	http_log(HTTP_G->log.redirect, "303-REDIRECT", header);			break;
		case 305:	http_log(HTTP_G->log.redirect, "305-REDIRECT", header);			break;
		case 307:	http_log(HTTP_G->log.redirect, "307-REDIRECT", header);			break;
		case 304:	http_log(HTTP_G->log.cache, "304-CACHE", header);				break;
		case 405:	http_log(HTTP_G->log.allowed_methods, "405-ALLOWED", header);	break;
		default:	http_log(NULL, header, body);									break;
	}
	
	STR_FREE(header);
	STR_FREE(body);
	
	if (HTTP_G->force_exit) {
		zend_bailout();
	} else {
		php_ob_set_internal_handler(http_ob_blackhole, 4096, "blackhole", 0 TSRMLS_CC);
	}
	
	return SUCCESS;
}
/* }}} */

/* {{{ STATUS http_check_method(char *) */
STATUS _http_check_method_ex(const char *method, const char *methods)
{
	const char *found;

	if (	(found = strstr(methods, method)) &&
			(found == method || !isalpha(found[-1])) &&
			(strlen(found) >= strlen(method) && !isalpha(found[strlen(method)]))) {
		return SUCCESS;
	}
	return FAILURE;
}
/* }}} */

/* {{{ zval *http_get_server_var_ex(char *, size_t) */
PHP_HTTP_API zval *_http_get_server_var_ex(const char *key, size_t key_size, zend_bool check TSRMLS_DC)
{
	zval **hsv;
	zval **var;
	
	if ((SUCCESS != zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &hsv)) || (Z_TYPE_PP(hsv) != IS_ARRAY)) {
		return NULL;
	}
	if ((SUCCESS != zend_hash_find(Z_ARRVAL_PP(hsv), (char *) key, key_size, (void **) &var)) || (Z_TYPE_PP(var) != IS_STRING)) {
		return NULL;
	}
	if (check && !(Z_STRVAL_PP(var) && Z_STRLEN_PP(var))) {
		return NULL;
	}
	return *var;
}
/* }}} */

/* {{{ STATUS http_get_request_body(char **, size_t *) */
PHP_HTTP_API STATUS _http_get_request_body_ex(char **body, size_t *length, zend_bool dup TSRMLS_DC)
{
	*length = 0;
	*body = NULL;
	
	if (SG(request_info).raw_post_data) {
		*length = SG(request_info).raw_post_data_length;
		*body = SG(request_info).raw_post_data;
		
		if (dup) {
			*body = estrndup(*body, *length);
		}
		return SUCCESS;
	} else if (sapi_module.read_post && !HTTP_G->read_post_data) {
		char buf[4096];
		int len;
		
		HTTP_G->read_post_data = 1;
		
		while (0 < (len = sapi_module.read_post(buf, sizeof(buf) TSRMLS_CC))) {
			*body = erealloc(*body, *length + len + 1);
			memcpy(*body + *length, buf, len);
			*length += len;
			(*body)[*length] = '\0';
		}
		
		/* check for error */
		if (len < 0) {
			STR_FREE(*body);
			*length = 0;
			return FAILURE;
		}
		
		SG(request_info).raw_post_data = *body;
		SG(request_info).raw_post_data_length = *length;
		
		if (dup) {
			*body = estrndup(*body, *length);
		}
		return SUCCESS;
	}
	
	return FAILURE;
}
/* }}} */

/* {{{ php_stream *_http_get_request_body_stream(void) */
PHP_HTTP_API php_stream *_http_get_request_body_stream(TSRMLS_D)
{
	php_stream *s = NULL;
	
	if (SG(request_info).raw_post_data) {
		s = php_stream_open_wrapper("php://input", "rb", 0, NULL);
	} else if (sapi_module.read_post && !HTTP_G->read_post_data) {
		HTTP_G->read_post_data = 1;
		
		if ((s = php_stream_temp_new())) {
			char buf[4096];
			int len;
			
			while (0 < (len = sapi_module.read_post(buf, sizeof(buf) TSRMLS_CC))) {
				php_stream_write(s, buf, len);
			}
			
			if (len < 0) {
				php_stream_close(s);
				s = NULL;
			} else {
				php_stream_rewind(s);
			}
		}
	}
	
	return s;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

