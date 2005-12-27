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


#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#define HTTP_WANT_ZLIB
#include "php_http.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_ZLIB)

#include "php_http_api.h"
#include "php_http_encoding_api.h"
#include "php_http_exception_object.h"
#include "php_http_inflatestream_object.h"

#define HTTP_BEGIN_ARGS(method, ret_ref, req_args) 	HTTP_BEGIN_ARGS_EX(HttpInflateStream, method, ret_ref, req_args)
#define HTTP_EMPTY_ARGS(method, ret_ref)			HTTP_EMPTY_ARGS_EX(HttpInflateStream, method, ret_ref)
#define HTTP_INFLATE_ME(method, visibility)			PHP_ME(HttpInflateStream, method, HTTP_ARGS(HttpInflateStream, method), visibility)

HTTP_BEGIN_ARGS(__construct, 0, 0)
	HTTP_ARG_VAL(flags, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(update, 0, 1)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(flush, 0, 0)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(finish, 0, 0)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

zend_class_entry *http_inflatestream_object_ce;
zend_function_entry http_inflatestream_object_fe[] = {
	HTTP_INFLATE_ME(update, ZEND_ACC_PUBLIC)
	HTTP_INFLATE_ME(flush, ZEND_ACC_PUBLIC)
	HTTP_INFLATE_ME(finish, ZEND_ACC_PUBLIC)
	
	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers http_inflatestream_object_handlers;

static inline void http_inflatestream_object_declare_default_properties() { return; }

PHP_MINIT_FUNCTION(http_inflatestream_object)
{
	HTTP_REGISTER_CLASS_EX(HttpInflateStream, http_inflatestream_object, NULL, 0);
	http_inflatestream_object_handlers.clone_obj = _http_inflatestream_object_clone_obj;
	return SUCCESS;
}

zend_object_value _http_inflatestream_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return http_inflatestream_object_new_ex(ce, NULL, NULL);
}

zend_object_value _http_inflatestream_object_new_ex(zend_class_entry *ce, http_encoding_stream *s, http_inflatestream_object **ptr TSRMLS_DC)
{
	zend_object_value ov;
	http_inflatestream_object *o;

	o = ecalloc(1, sizeof(http_inflatestream_object));
	o->zo.ce = ce;
	
	if (ptr) {
		*ptr = o;
	}

	if (s) {
		o->stream = s;
	}

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);

	ov.handle = putObject(http_inflatestream_object, o);
	ov.handlers = &http_inflatestream_object_handlers;

	return ov;
}

zend_object_value _http_inflatestream_object_clone_obj(zval *this_ptr TSRMLS_DC)
{
	http_encoding_stream *s;
	getObject(http_inflatestream_object, obj);
	
	s = ecalloc(1, sizeof(http_encoding_stream));
	s->flags = obj->stream->flags;
	inflateCopy(&s->stream, &obj->stream->stream);
	s->stream.opaque = phpstr_dup(s->stream.opaque);
	
	return http_inflatestream_object_new_ex(Z_OBJCE_P(this_ptr), s, NULL);
}

void _http_inflatestream_object_free(zend_object *object TSRMLS_DC)
{
	http_inflatestream_object *o = (http_inflatestream_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	if (o->stream) {
		http_encoding_inflate_stream_free(&o->stream);
	}
	efree(o);
}

/* {{{ proto string HttpInflateStream::update(string data)
 *
 * Passes more data through the inflate stream.
 * 
 * Expects a string parameter containing (a part of) the data to inflate.
 * 
 * Returns inflated data on success or FALSE on failure.
 */
PHP_METHOD(HttpInflateStream, update)
{
	int data_len;
	size_t decoded_len = 0;
	char *data, *decoded = NULL;
	getObject(http_inflatestream_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &data, &data_len)) {
		RETURN_FALSE;
	}
	
	if (!data_len) {
		RETURN_STRING("", 1);
	}
	
	if (!obj->stream && !(obj->stream = http_encoding_inflate_stream_init(NULL, 0))) {
		RETURN_FALSE;
	}
	
	if (SUCCESS == http_encoding_inflate_stream_update(obj->stream, data, data_len, &decoded, &decoded_len)) {
		RETURN_STRINGL(decoded, decoded_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string HttpInflateStream::flush([string data])
 *
 * Flush the inflate stream.
 * 
 * Returns some inflated data as string on success or FALSE on failure.
 */
PHP_METHOD(HttpInflateStream, flush)
{
	int data_len = 0;
	size_t decoded_len = 0;
	char *decoded = NULL, *data = NULL;
	getObject(http_inflatestream_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &data, &data_len));
	
	if (!obj->stream && !(obj->stream = http_encoding_inflate_stream_init(NULL, 0))) {
		RETURN_FALSE;
	}
	
	/* flushing the inflate stream is a no-op */
	if (!data_len) {
		RETURN_STRINGL("", 0, 1);
	} else if (SUCCESS == http_encoding_inflate_stream_update(obj->stream, data, data_len, &decoded, &decoded_len)) {
		RETURN_STRINGL(decoded, decoded_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string HttpInflateStream::finish([string data])
 *
 * Finalizes the inflate stream.  The inflate stream can be reused after finalizing.
 * 
 * Returns the final part of inflated data.
 */ 
PHP_METHOD(HttpInflateStream, finish)
{
	int data_len = 0;
	size_t updated_len = 0, decoded_len = 0;
	char *updated = NULL, *decoded = NULL, *data = NULL;
	getObject(http_inflatestream_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &data, &data_len)) {
		RETURN_FALSE;
	}
	
	if (!obj->stream && !(obj->stream = http_encoding_inflate_stream_init(NULL, 0))) {
		RETURN_FALSE;
	}
	
	if (data_len) {
		if (SUCCESS != http_encoding_inflate_stream_update(obj->stream, data, data_len, &updated, &updated_len)) {
			RETURN_FALSE;
		}
	}
	
	if (SUCCESS == http_encoding_inflate_stream_finish(obj->stream, &decoded, &decoded_len)) {
		if (updated_len) {
			updated = erealloc(updated, updated_len + decoded_len + 1);
			updated[updated_len + decoded_len] = '\0';
			memcpy(updated + updated_len, decoded, decoded_len);
			updated_len += decoded_len;
			RETVAL_STRINGL(updated, updated_len, 0);
			STR_FREE(decoded);
		} else {
			RETVAL_STRINGL(decoded, decoded_len, 0);
			STR_FREE(updated);
		}
	} else {
		RETVAL_FALSE;
	}
	
	http_encoding_inflate_stream_dtor(obj->stream);
	http_encoding_inflate_stream_init(obj->stream, obj->stream->flags);
}
/* }}} */


#endif /* ZEND_ENGINE_2 && HTTP_HAVE_ZLIB*/

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

