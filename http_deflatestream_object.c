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

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_ZLIB)

#include "php_http_api.h"
#include "php_http_encoding_api.h"
#include "php_http_exception_object.h"
#include "php_http_deflatestream_object.h"

#define HTTP_BEGIN_ARGS(method, req_args) 	HTTP_BEGIN_ARGS_EX(HttpDeflateStream, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method)				HTTP_EMPTY_ARGS_EX(HttpDeflateStream, method, 0)
#define HTTP_DEFLATE_ME(method, visibility)	PHP_ME(HttpDeflateStream, method, HTTP_ARGS(HttpDeflateStream, method), visibility)

HTTP_BEGIN_ARGS(__construct, 0)
	HTTP_ARG_VAL(flags, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(update, 1)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(flush, 0)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(finish, 0)
	HTTP_ARG_VAL(data, 0)
HTTP_END_ARGS;

#define OBJ_PROP_CE http_deflatestream_object_ce
zend_class_entry *http_deflatestream_object_ce;
zend_function_entry http_deflatestream_object_fe[] = {
	HTTP_DEFLATE_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	HTTP_DEFLATE_ME(update, ZEND_ACC_PUBLIC)
	HTTP_DEFLATE_ME(flush, ZEND_ACC_PUBLIC)
	HTTP_DEFLATE_ME(finish, ZEND_ACC_PUBLIC)
	
	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers http_deflatestream_object_handlers;

PHP_MINIT_FUNCTION(http_deflatestream_object)
{
	HTTP_REGISTER_CLASS_EX(HttpDeflateStream, http_deflatestream_object, NULL, 0);
	http_deflatestream_object_handlers.clone_obj = _http_deflatestream_object_clone_obj;
	
#ifndef WONKY
	DCL_CONST(long, "TYPE_GZIP", HTTP_DEFLATE_TYPE_GZIP);
	DCL_CONST(long, "TYPE_ZLIB", HTTP_DEFLATE_TYPE_ZLIB);
	DCL_CONST(long, "TYPE_RAW", HTTP_DEFLATE_TYPE_RAW);
	DCL_CONST(long, "LEVEL_DEF", HTTP_DEFLATE_LEVEL_DEF);
	DCL_CONST(long, "LEVEL_MIN", HTTP_DEFLATE_LEVEL_MIN);
	DCL_CONST(long, "LEVEL_MAX", HTTP_DEFLATE_LEVEL_MAX);
	DCL_CONST(long, "STRATEGY_DEF", HTTP_DEFLATE_STRATEGY_DEF);
	DCL_CONST(long, "STRATEGY_FILT", HTTP_DEFLATE_STRATEGY_FILT);
	DCL_CONST(long, "STRATEGY_HUFF", HTTP_DEFLATE_STRATEGY_HUFF);
	DCL_CONST(long, "STRATEGY_RLE", HTTP_DEFLATE_STRATEGY_RLE);
	DCL_CONST(long, "STRATEGY_FIXED", HTTP_DEFLATE_STRATEGY_FIXED);
#endif
	
	return SUCCESS;
}

zend_object_value _http_deflatestream_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return http_deflatestream_object_new_ex(ce, NULL, NULL);
}

zend_object_value _http_deflatestream_object_new_ex(zend_class_entry *ce, http_encoding_stream *s, http_deflatestream_object **ptr TSRMLS_DC)
{
	zend_object_value ov;
	http_deflatestream_object *o;

	o = ecalloc(1, sizeof(http_deflatestream_object));
	o->zo.ce = ce;
	
	if (ptr) {
		*ptr = o;
	}

	if (s) {
		o->stream = s;
	}

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);

	ov.handle = putObject(http_deflatestream_object, o);
	ov.handlers = &http_deflatestream_object_handlers;

	return ov;
}

zend_object_value _http_deflatestream_object_clone_obj(zval *this_ptr TSRMLS_DC)
{
	http_encoding_stream *s;
	getObject(http_deflatestream_object, obj);
	
	s = ecalloc(1, sizeof(http_encoding_stream));
	s->flags = obj->stream->flags;
	deflateCopy(&s->stream, &obj->stream->stream);
	s->stream.opaque = phpstr_dup(s->stream.opaque);
	
	return http_deflatestream_object_new_ex(Z_OBJCE_P(this_ptr), s, NULL);
}

void _http_deflatestream_object_free(zend_object *object TSRMLS_DC)
{
	http_deflatestream_object *o = (http_deflatestream_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	if (o->stream) {
		http_encoding_deflate_stream_free(&o->stream);
	}
	efree(o);
}

/* {{{ proto void HttpDeflateStream::__construct([int flags = 0])
 *
 * Creates a new HttpDeflateStream object instance.
 * 
 * Accepts an optional int parameter specifying how to initialize the deflate stream.
 */ 
PHP_METHOD(HttpDeflateStream, __construct)
{
	long flags = 0;
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &flags)) {
		getObject(http_deflatestream_object, obj);
		
		if (!obj->stream) {
			obj->stream = http_encoding_deflate_stream_init(NULL, flags);
		} else {
			http_error_ex(HE_WARNING, HTTP_E_ENCODING, "HttpDeflateStream cannot be initialized twice");
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto string HttpDeflateStream::update(string data)
 *
 * Passes more data through the deflate stream.
 * 
 * Expects a string parameter containing (a part of) the data to deflate.
 * 
 * Returns deflated data on success or FALSE on failure.
 */
PHP_METHOD(HttpDeflateStream, update)
{
	int data_len;
	size_t encoded_len = 0;
	char *data, *encoded = NULL;
	getObject(http_deflatestream_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &data, &data_len)) {
		RETURN_FALSE;
	}
	
	if (!obj->stream && !(obj->stream = http_encoding_deflate_stream_init(NULL, 0))) {
		RETURN_FALSE;
	}
	
	if (SUCCESS == http_encoding_deflate_stream_update(obj->stream, data, data_len, &encoded, &encoded_len)) {
		RETURN_STRINGL(encoded, encoded_len, 0);
	} else {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string HttpDeflateStream::flush([string data])
 *
 * Flushes the deflate stream.
 * 
 * Returns some deflated data as string on success or FALSE on failure.
 */ 
PHP_METHOD(HttpDeflateStream, flush)
{
	int data_len = 0;
	size_t updated_len = 0, encoded_len = 0;
	char *updated = NULL, *encoded = NULL, *data = NULL;
	getObject(http_deflatestream_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &data, &data_len)) {
		RETURN_FALSE;
	}
	
	if (!obj->stream && !(obj->stream = http_encoding_deflate_stream_init(NULL, 0))) {
		RETURN_FALSE;
	}
	
	if (data_len) {
		if (SUCCESS != http_encoding_deflate_stream_update(obj->stream, data, data_len, &updated, &updated_len)) {
			RETURN_FALSE;
		}
	}
	
	if (SUCCESS == http_encoding_deflate_stream_flush(obj->stream, &encoded, &encoded_len)) {
		if (updated_len) {
			updated = erealloc(updated, updated_len + encoded_len + 1);
			updated[updated_len + encoded_len] = '\0';
			memcpy(updated + updated_len, encoded, encoded_len);
			STR_FREE(encoded);
			updated_len += encoded_len;
			RETURN_STRINGL(updated, updated_len, 0);
		} else {
			RETVAL_STRINGL(encoded, encoded_len, 0);
		}
	} else {
		RETVAL_FALSE;
	}
	STR_FREE(updated);
}
/* }}} */

/* {{{ proto string HttpDeflateStream::finish([string data])
 *
 * Finalizes the deflate stream.  The deflate stream can be reused after finalizing.
 * 
 * Returns the final part of deflated data.
 */ 
PHP_METHOD(HttpDeflateStream, finish)
{
	int data_len = 0;
	size_t updated_len = 0, encoded_len = 0;
	char *updated = NULL, *encoded = NULL, *data = NULL;
	getObject(http_deflatestream_object, obj);
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &data, &data_len)) {
		RETURN_FALSE;
	}
	
	if (!obj->stream && !(obj->stream = http_encoding_deflate_stream_init(NULL, 0))) {
		RETURN_FALSE;
	}
	
	if (data_len) {
		if (SUCCESS != http_encoding_deflate_stream_update(obj->stream, data, data_len, &updated, &updated_len)) {
			RETURN_FALSE;
		}
	}
	
	if (SUCCESS == http_encoding_deflate_stream_finish(obj->stream, &encoded, &encoded_len)) {
		if (updated_len) {
			updated = erealloc(updated, updated_len + encoded_len + 1);
			updated[updated_len + encoded_len] = '\0';
			memcpy(updated + updated_len, encoded, encoded_len);
			STR_FREE(encoded);
			updated_len += encoded_len;
			RETVAL_STRINGL(updated, updated_len, 0);
		} else {
			STR_FREE(updated);
			RETVAL_STRINGL(encoded, encoded_len, 0);
		}
	} else {
		STR_FREE(updated);
		RETVAL_FALSE;
	}
	
	http_encoding_deflate_stream_dtor(obj->stream);
	http_encoding_deflate_stream_init(obj->stream, obj->stream->flags);
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

