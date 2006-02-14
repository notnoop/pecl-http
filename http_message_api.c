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
#define HTTP_WANT_ZLIB
#include "php_http.h"

#include "php_http_api.h"
#include "php_http_encoding_api.h"
#include "php_http_headers_api.h"
#include "php_http_message_api.h"
#include "php_http_request_api.h"
#include "php_http_send_api.h"
#include "php_http_url_api.h"

#define http_message_info_callback _http_message_info_callback
static void _http_message_info_callback(http_message **message, HashTable **headers, http_info *info TSRMLS_DC)
{
	http_message *old = *message;
	
	/* advance message */
	if (old->type || zend_hash_num_elements(&old->hdrs) || PHPSTR_LEN(old)) {
		(*message) = http_message_new();
		(*message)->parent = old;
		(*headers) = &((*message)->hdrs);
	}
	
	http_message_set_info(*message, info);
}

#define http_message_init_type _http_message_init_type
static inline void _http_message_init_type(http_message *message, http_message_type type)
{
	message->http.version = .0;
	
	switch (message->type = type)
	{
		case HTTP_MSG_RESPONSE:
			message->http.info.response.code = 0;
			message->http.info.response.status = NULL;
		break;

		case HTTP_MSG_REQUEST:
			message->http.info.request.method = NULL;
			message->http.info.request.url = NULL;
		break;

		case HTTP_MSG_NONE:
		default:
		break;
	}
}

PHP_HTTP_API http_message *_http_message_init_ex(http_message *message, http_message_type type ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC)
{
	if (!message) {
		message = ecalloc_rel(1, sizeof(http_message));
	}

	http_message_init_type(message, type);
	message->parent = NULL;
	phpstr_init(&message->body);
	zend_hash_init(&message->hdrs, 0, NULL, ZVAL_PTR_DTOR, 0);

	return message;
}


PHP_HTTP_API void _http_message_set_type(http_message *message, http_message_type type)
{
	/* just act if different */
	if (type != message->type) {

		/* free request info */
		switch (message->type)
		{
			case HTTP_MSG_REQUEST:
				STR_FREE(message->http.info.request.method);
				STR_FREE(message->http.info.request.url);
			break;
			
			case HTTP_MSG_RESPONSE:
				STR_FREE(message->http.info.response.status);
			break;
			
			default:
			break;
		}

		/* init */
		http_message_init_type(message, type);
	}
}

PHP_HTTP_API void _http_message_set_info(http_message *message, http_info *info)
{
	message->http.version = info->http.version;
	
	switch (message->type = info->type)
	{
		case IS_HTTP_REQUEST:
			HTTP_INFO(message).request.url = estrdup(HTTP_INFO(info).request.url);
			STR_SET(HTTP_INFO(message).request.method, estrdup(HTTP_INFO(info).request.method));
		break;
		
		case IS_HTTP_RESPONSE:
			HTTP_INFO(message).response.code = HTTP_INFO(info).response.code;
			STR_SET(HTTP_INFO(message).response.status, estrdup(HTTP_INFO(info).response.status));
		break;
		
		default:
		break;
	}
}

PHP_HTTP_API http_message *_http_message_parse_ex(http_message *msg, const char *message, size_t message_length ZEND_FILE_LINE_DC ZEND_FILE_LINE_ORIG_DC TSRMLS_DC)
{
	const char *body = NULL;
	zend_bool free_msg = msg ? 0 : 1;

	if ((!message) || (message_length < HTTP_MSG_MIN_SIZE)) {
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "Empty or too short HTTP message: '%s'", message);
		return NULL;
	}

	msg = http_message_init_rel(msg, 0);

	if (SUCCESS != http_parse_headers_cb(message, &msg->hdrs, 1, (http_info_callback) http_message_info_callback, (void **) &msg)) {
		if (free_msg) {
			http_message_free(&msg);
		}
		http_error(HE_WARNING, HTTP_E_MALFORMED_HEADERS, "Failed to parse message headers");
		return NULL;
	}
	
	/* header parsing stops at (CR)LF (CR)LF */
	if ((body = http_locate_body(message))) {
		zval *c;
		const char *continue_at = NULL;
		size_t remaining = message + message_length - body;

		/* message has chunked transfer encoding */
		if ((c = http_message_header(msg, "Transfer-Encoding")) && (!strcasecmp("chunked", Z_STRVAL_P(c)))) {
			char *decoded;
			size_t decoded_len;

			/* decode and replace Transfer-Encoding with Content-Length header */
			if ((continue_at = http_encoding_dechunk(body, message + message_length - body, &decoded, &decoded_len))) {
				zval *len;
				char *tmp;
				int tmp_len;

				tmp_len = (int) spprintf(&tmp, 0, "%zu", decoded_len);
				MAKE_STD_ZVAL(len);
				ZVAL_STRINGL(len, tmp, tmp_len, 0);

				ZVAL_ADDREF(c);
				zend_hash_add(&msg->hdrs, "X-Original-Transfer-Encoding", sizeof("X-Original-Transfer-Encoding"), (void *) &c, sizeof(zval *), NULL);
				zend_hash_del(&msg->hdrs, "Transfer-Encoding", sizeof("Transfer-Encoding"));
				zend_hash_del(&msg->hdrs, "Content-Length", sizeof("Content-Length"));
				zend_hash_add(&msg->hdrs, "Content-Length", sizeof("Content-Length"), (void *) &len, sizeof(zval *), NULL);
				
				phpstr_from_string_ex(PHPSTR(msg), decoded, decoded_len);
				efree(decoded);
			}
		} else

		/* message has content-length header */
		if ((c = http_message_header(msg, "Content-Length"))) {
			ulong len = strtoul(Z_STRVAL_P(c), NULL, 10);
			if (len > remaining) {
				http_error_ex(HE_NOTICE, HTTP_E_MALFORMED_HEADERS, "The Content-Length header pretends a larger body than actually received (expected %lu bytes; got %lu bytes)", len, remaining);
				len = remaining;
			}
			phpstr_from_string_ex(PHPSTR(msg), body, len);
			continue_at = body + len;
		} else

		/* message has content-range header */
		if ((c = http_message_header(msg, "Content-Range"))) {
			ulong total = 0, start = 0, end = 0, len = 0;
			
			if (!strncasecmp(Z_STRVAL_P(c), "bytes", lenof("bytes")) && 
					(Z_STRVAL_P(c)[lenof("bytes")] == ':' || Z_STRVAL_P(c)[lenof("bytes")] == ' ')) {
				char *total_at = NULL, *end_at = NULL;
				char *start_at = Z_STRVAL_P(c) + sizeof("bytes");
				
				start = strtoul(start_at, &end_at, 10);
				if (end_at) {
					end = strtoul(end_at + 1, &total_at, 10);
					if (total_at && strncmp(total_at + 1, "*", 1)) {
						total = strtoul(total_at + 1, NULL, 10);
					}
					if ((len = (end + 1 - start)) > remaining) {
						http_error_ex(HE_NOTICE, HTTP_E_MALFORMED_HEADERS, "The Content-Range header pretends a larger body than actually received (expected %lu bytes; got %lu bytes)", len, remaining);
						len = remaining;
					}
					if (end >= start && (!total || end < total)) {
						phpstr_from_string_ex(PHPSTR(msg), body, len);
						continue_at = body + len;
					}
				}
			}

			if (!continue_at) {
				http_error_ex(HE_WARNING, HTTP_E_MALFORMED_HEADERS, "Invalid Content-Range header: %s", Z_STRVAL_P(c));
			}
		} else

		/* no headers that indicate content length */
		if (HTTP_MSG_TYPE(RESPONSE, msg)) {
			phpstr_from_string_ex(PHPSTR(msg), body, remaining);
		} else {
			continue_at = body;
		}
		
#ifdef HTTP_HAVE_ZLIB
		/* check for compressed data */
		if (http_message_header(msg, "Vary") && (c = http_message_header(msg, "Content-Encoding"))) {
			char *decoded = NULL;
			size_t decoded_len = 0;

			if (	!strcasecmp(Z_STRVAL_P(c), "gzip") || 
					!strcasecmp(Z_STRVAL_P(c), "x-gzip") ||
					!strcasecmp(Z_STRVAL_P(c), "deflate")) {
				http_encoding_inflate(PHPSTR_VAL(msg), PHPSTR_LEN(msg), &decoded, &decoded_len);
			}
			
			if (decoded) {
				zval *len, **original_len;
				char *tmp;
				int tmp_len;
				
				tmp_len = (int) spprintf(&tmp, 0, "%zu", decoded_len);
				MAKE_STD_ZVAL(len);
				ZVAL_STRINGL(len, tmp, tmp_len, 0);

				ZVAL_ADDREF(c);
				zend_hash_add(&msg->hdrs, "X-Original-Content-Encoding", sizeof("X-Original-Content-Encoding"), (void *) &c, sizeof(zval *), NULL);
				zend_hash_del(&msg->hdrs, "Content-Encoding", sizeof("Content-Encoding"));
				if (SUCCESS == zend_hash_find(&msg->hdrs, "Content-Length", sizeof("Content-Length"), (void **) &original_len)) {
					ZVAL_ADDREF(*original_len);					
					zend_hash_add(&msg->hdrs, "X-Original-Content-Length", sizeof("X-Original-Content-Length"), (void *) original_len, sizeof(zval *), NULL);
					zend_hash_update(&msg->hdrs, "Content-Length", sizeof("Content-Length"), (void *) &len, sizeof(zval *), NULL);
				} else {
					zend_hash_add(&msg->hdrs, "Content-Length", sizeof("Content-Length"), (void *) &len, sizeof(zval *), NULL);
				}

				phpstr_dtor(PHPSTR(msg));
				PHPSTR(msg)->data = decoded;
				PHPSTR(msg)->used = decoded_len;
				PHPSTR(msg)->free = 1;
			}
		}
#endif /* HTTP_HAVE_ZLIB */

		/* check for following messages */
		if (continue_at && (continue_at < (message + message_length))) {
			while (isspace(*continue_at)) ++continue_at;
			if (continue_at < (message + message_length)) {
				http_message *next = NULL, *most = NULL;

				/* set current message to parent of most parent following messages and return deepest */
				if ((most = next = http_message_parse_rel(NULL, continue_at, message + message_length - continue_at))) {
					while (most->parent) most = most->parent;
					most->parent = msg;
					msg = next;
				}
			}
		}
	}

	return msg;
}

PHP_HTTP_API void _http_message_tostring(http_message *msg, char **string, size_t *length)
{
	phpstr str;
	char *key, *data;
	ulong idx;
	zval **header;
	HashPosition pos1;

	phpstr_init_ex(&str, 4096, 0);

	switch (msg->type)
	{
		case HTTP_MSG_REQUEST:
			phpstr_appendf(&str, "%s %s HTTP/%1.1f" HTTP_CRLF,
				msg->http.info.request.method,
				msg->http.info.request.url,
				msg->http.version);
		break;

		case HTTP_MSG_RESPONSE:
			phpstr_appendf(&str, "HTTP/%1.1f %d%s%s" HTTP_CRLF,
				msg->http.version,
				msg->http.info.response.code,
				*msg->http.info.response.status ? " ":"",
				msg->http.info.response.status);
		break;

		case HTTP_MSG_NONE:
		default:
		break;
	}

	FOREACH_HASH_KEYVAL(pos1, &msg->hdrs, key, idx, header) {
		if (key) {
			zval **single_header;

			switch (Z_TYPE_PP(header))
			{
				case IS_STRING:
					phpstr_appendf(&str, "%s: %s" HTTP_CRLF, key, Z_STRVAL_PP(header));
				break;

				case IS_ARRAY:
				{
					HashPosition pos2;
					FOREACH_VAL(pos2, *header, single_header) {
						phpstr_appendf(&str, "%s: %s" HTTP_CRLF, key, Z_STRVAL_PP(single_header));
					}
				}
				break;
			}

			key = NULL;
		}
	}

	if (PHPSTR_LEN(msg)) {
		phpstr_appends(&str, HTTP_CRLF);
		phpstr_append(&str, PHPSTR_VAL(msg), PHPSTR_LEN(msg));
		phpstr_appends(&str, HTTP_CRLF);
	}

	data = phpstr_data(&str, string, length);
	if (!string) {
		efree(data);
	}

	phpstr_dtor(&str);
}

PHP_HTTP_API void _http_message_serialize(http_message *message, char **string, size_t *length)
{
	char *buf;
	size_t len;
	phpstr str;

	phpstr_init(&str);

	do {
		http_message_tostring(message, &buf, &len);
		phpstr_prepend(&str, buf, len);
		efree(buf);
	} while ((message = message->parent));

	buf = phpstr_data(&str, string, length);
	if (!string) {
		efree(buf);
	}

	phpstr_dtor(&str);
}

PHP_HTTP_API http_message *_http_message_reverse(http_message *msg)
{
	int i, c;
	
	http_message_count(c, msg);
	
	if (c > 1) {
		http_message *tmp = msg, **arr = ecalloc(c, sizeof(http_message *));
		
		for (i = 0; i < c; ++i) {
			arr[i] = tmp;
			tmp = tmp->parent;
		}
		arr[0]->parent = NULL;
		for (i = 0; i < c-1; ++i) {
			arr[i+1]->parent = arr[i];
		}
		
		msg = arr[c-1];
		efree(arr);
	}
	
	return msg;
}

PHP_HTTP_API http_message *_http_message_interconnect(http_message *m1, http_message *m2)
{
	if (!m1) {
		return NULL;
	} else if (m2) {
		int i = 0, c1, c2;
		http_message *t1 = m1, *t2 = m2, *p1, *p2;
		
		http_message_count(c1, m1);
		http_message_count(c2, m2);
		
		while (i++ < (c1 - c2)) {
			t1 = t1->parent;
		}
		while (i++ <= c1) {
			p1 = t1->parent;
			p2 = t2->parent;
			t1->parent = t2;
			t2->parent = p1;
			t1 = p1;
			t2 = p2;
		}
	}
	return m1;
}

PHP_HTTP_API void _http_message_tostruct_recursive(http_message *msg, zval *obj TSRMLS_DC)
{
	zval strct;
	zval *headers;
	
	INIT_ZARR(strct, HASH_OF(obj));
	
	add_assoc_long(&strct, "type", msg->type);
	add_assoc_double(&strct, "httpVersion", msg->http.version);
	switch (msg->type)
	{
		case HTTP_MSG_RESPONSE:
			add_assoc_long(&strct, "responseCode", msg->http.info.response.code);
			add_assoc_string(&strct, "responseStatus", msg->http.info.response.status, 1);
		break;
		
		case HTTP_MSG_REQUEST:
			add_assoc_string(&strct, "requestMethod", msg->http.info.request.method, 1);
			add_assoc_string(&strct, "requestUrl", msg->http.info.request.url, 1);
		break;
		
		case HTTP_MSG_NONE:
			/* avoid compiler warning */
		break;
	}
	
	MAKE_STD_ZVAL(headers);
	array_init(headers);
	zend_hash_copy(Z_ARRVAL_P(headers), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	add_assoc_zval(&strct, "headers", headers);
	
	add_assoc_stringl(&strct, "body", PHPSTR_VAL(msg), PHPSTR_LEN(msg), 1);
	
	if (msg->parent) {
		zval *parent;
		
		MAKE_STD_ZVAL(parent);
		if (Z_TYPE_P(obj) == IS_ARRAY) {
			array_init(parent);
		} else {
			object_init(parent);
		}
		add_assoc_zval(&strct, "parentMessage", parent);
		http_message_tostruct_recursive(msg->parent, parent);
	} else {
		add_assoc_null(&strct, "parentMessage");
	}
}

PHP_HTTP_API STATUS _http_message_send(http_message *message TSRMLS_DC)
{
	STATUS rs = FAILURE;

	switch (message->type)
	{
		case HTTP_MSG_RESPONSE:
		{
			char *key;
			ulong idx;
			zval **val;
			HashPosition pos1;

			FOREACH_HASH_KEYVAL(pos1, &message->hdrs, key, idx, val) {
				if (key) {
					if (Z_TYPE_PP(val) == IS_ARRAY) {
						zend_bool first = 1;
						zval **data;
						HashPosition pos2;
						
						FOREACH_VAL(pos2, *val, data) {
							http_send_header_ex(key, strlen(key), Z_STRVAL_PP(data), Z_STRLEN_PP(data), first, NULL);
							first = 0;
						}
					} else {
						http_send_header_ex(key, strlen(key), Z_STRVAL_PP(val), Z_STRLEN_PP(val), 1, NULL);
					}
					key = NULL;
				}
			}
			rs =	SUCCESS == http_send_status(message->http.info.response.code) &&
					SUCCESS == http_send_data(PHPSTR_VAL(message), PHPSTR_LEN(message)) ?
					SUCCESS : FAILURE;
		}
		break;

		case HTTP_MSG_REQUEST:
		{
#ifdef HTTP_HAVE_CURL
			char *uri = NULL;
			http_request request;
			zval **zhost, options, headers;

			INIT_PZVAL(&options);
			INIT_PZVAL(&headers);
			array_init(&options);
			array_init(&headers);
			zend_hash_copy(Z_ARRVAL(headers), &message->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
			add_assoc_zval(&options, "headers", &headers);

			/* check host header */
			if (SUCCESS == zend_hash_find(&message->hdrs, "Host", sizeof("Host"), (void **) &zhost)) {
				char *colon = NULL;
				php_url parts, *url = php_url_parse(message->http.info.request.url);
				
				memset(&parts, 0, sizeof(php_url));

				/* check for port */
				if ((colon = strchr(Z_STRVAL_PP(zhost), ':'))) {
					parts.port = atoi(colon + 1);
					parts.host = estrndup(Z_STRVAL_PP(zhost), (Z_STRVAL_PP(zhost) - colon - 1));
				} else {
					parts.host = estrndup(Z_STRVAL_PP(zhost), Z_STRLEN_PP(zhost));
				}
				
				http_build_url(HTTP_URL_REPLACE, url, &parts, NULL, &uri, NULL);
				php_url_free(url);
				efree(parts.host);
			} else {
				uri = http_absolute_url(message->http.info.request.url);
			}

			if ((request.meth = http_request_method_exists(1, 0, message->http.info.request.method))) {
				http_request_body body;
				
				http_request_init_ex(&request, NULL, request.meth, uri);
				request.body = http_request_body_init_ex(&body, HTTP_REQUEST_BODY_CSTRING, PHPSTR_VAL(message), PHPSTR_LEN(message), 0);
				if (SUCCESS == (rs = http_request_prepare(&request, NULL))) {
					http_request_exec(&request);
				}
				http_request_dtor(&request);
			} else {
				http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD,
					"Cannot send HttpMessage. Request method %s not supported",
					message->http.info.request.method);
			}
			efree(uri);
#else
			http_error(HE_WARNING, HTTP_E_RUNTIME, "HTTP requests not supported - ext/http was not linked against libcurl.");
#endif
		}
		break;

		case HTTP_MSG_NONE:
		default:
			http_error(HE_WARNING, HTTP_E_MESSAGE_TYPE, "HttpMessage is neither of type HTTP_MSG_REQUEST nor HTTP_MSG_RESPONSE");
		break;
	}

	return rs;
}

PHP_HTTP_API http_message *_http_message_dup(http_message *msg TSRMLS_DC)
{
	/*
	 * TODO: unroll
	 */
	http_message *new;
	char *serialized_data;
	size_t serialized_length;

	http_message_serialize(msg, &serialized_data, &serialized_length);
	new = http_message_parse(serialized_data, serialized_length);
	efree(serialized_data);
	return new;
}

PHP_HTTP_API void _http_message_dtor(http_message *message)
{
	if (message) {
		zend_hash_destroy(&message->hdrs);
		phpstr_dtor(PHPSTR(message));
		
		switch (message->type)
		{
			case HTTP_MSG_REQUEST:
				STR_SET(message->http.info.request.method, NULL);
				STR_SET(message->http.info.request.url, NULL);
			break;
			
			case HTTP_MSG_RESPONSE:
				STR_SET(message->http.info.response.status, NULL);
			break;
			
			default:
			break;
		}
	}
}

PHP_HTTP_API void _http_message_free(http_message **message)
{
	if (*message) {
		if ((*message)->parent) {
			http_message_free(&(*message)->parent);
		}
		http_message_dtor(*message);
		efree(*message);
		*message = NULL;
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

