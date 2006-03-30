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

#ifdef ZEND_ENGINE_2

#include "zend_interfaces.h"
#include "ext/standard/url.h"

#include "php_http_api.h"
#include "php_http_send_api.h"
#include "php_http_url_api.h"
#include "php_http_message_api.h"
#include "php_http_message_object.h"
#include "php_http_exception_object.h"
#include "php_http_response_object.h"
#include "php_http_request_method_api.h"
#include "php_http_request_api.h"
#include "php_http_request_object.h"

#ifndef WONKY
#	ifdef HAVE_SPL
/* SPL doesn't install its headers */
extern PHPAPI zend_class_entry *spl_ce_Countable;
#	endif
#endif

#define HTTP_BEGIN_ARGS(method, req_args) 	HTTP_BEGIN_ARGS_EX(HttpMessage, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method)				HTTP_EMPTY_ARGS_EX(HttpMessage, method, 0)
#define HTTP_MESSAGE_ME(method, visibility)	PHP_ME(HttpMessage, method, HTTP_ARGS(HttpMessage, method), visibility)

HTTP_BEGIN_ARGS(__construct, 0)
	HTTP_ARG_VAL(message, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(fromString, 1)
	HTTP_ARG_VAL(message, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getBody);
HTTP_BEGIN_ARGS(setBody, 1)
	HTTP_ARG_VAL(body, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getHeaders);
HTTP_BEGIN_ARGS(setHeaders, 1)
	HTTP_ARG_VAL(headers, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addHeaders, 1)
	HTTP_ARG_VAL(headers, 0)
	HTTP_ARG_VAL(append, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getType);
HTTP_BEGIN_ARGS(setType, 1)
	HTTP_ARG_VAL(type, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getResponseCode);
HTTP_BEGIN_ARGS(setResponseCode, 1)
	HTTP_ARG_VAL(response_code, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getResponseStatus);
HTTP_BEGIN_ARGS(setResponseStatus, 1)
	HTTP_ARG_VAL(response_status, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getRequestMethod);
HTTP_BEGIN_ARGS(setRequestMethod, 1)
	HTTP_ARG_VAL(request_method, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getRequestUrl);
HTTP_BEGIN_ARGS(setRequestUrl, 1)
	HTTP_ARG_VAL(url, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getHttpVersion);
HTTP_BEGIN_ARGS(setHttpVersion, 1)
	HTTP_ARG_VAL(http_version, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getParentMessage);
HTTP_EMPTY_ARGS(send);
HTTP_BEGIN_ARGS(toString, 0)
	HTTP_ARG_VAL(include_parent, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(toMessageTypeObject);

HTTP_EMPTY_ARGS(count);

HTTP_EMPTY_ARGS(serialize);
HTTP_BEGIN_ARGS(unserialize, 1)
	HTTP_ARG_VAL(serialized, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(rewind);
HTTP_EMPTY_ARGS(valid);
HTTP_EMPTY_ARGS(key);
HTTP_EMPTY_ARGS(current);
HTTP_EMPTY_ARGS(next);

HTTP_EMPTY_ARGS(detach);
HTTP_BEGIN_ARGS(prepend, 1)
	HTTP_ARG_OBJ(HttpMessage, message, 0)
HTTP_END_ARGS;
HTTP_EMPTY_ARGS(reverse);

#define http_message_object_read_prop _http_message_object_read_prop
static zval *_http_message_object_read_prop(zval *object, zval *member, int type TSRMLS_DC);
#define http_message_object_write_prop _http_message_object_write_prop
static void _http_message_object_write_prop(zval *object, zval *member, zval *value TSRMLS_DC);
#define http_message_object_get_props _http_message_object_get_props
static HashTable *_http_message_object_get_props(zval *object TSRMLS_DC);

#define OBJ_PROP_CE http_message_object_ce
zend_class_entry *http_message_object_ce;
zend_function_entry http_message_object_fe[] = {
	HTTP_MESSAGE_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	HTTP_MESSAGE_ME(getBody, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setBody, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getHeaders, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setHeaders, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(addHeaders, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getType, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setType, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getResponseCode, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setResponseCode, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getResponseStatus, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setResponseStatus, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getRequestMethod, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setRequestMethod, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getRequestUrl, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setRequestUrl, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getHttpVersion, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(setHttpVersion, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(getParentMessage, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(send, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(toString, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(toMessageTypeObject, ZEND_ACC_PUBLIC)

	/* implements Countable */
	HTTP_MESSAGE_ME(count, ZEND_ACC_PUBLIC)
	
	/* implements Serializable */
	HTTP_MESSAGE_ME(serialize, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(unserialize, ZEND_ACC_PUBLIC)
	
	/* implements Iterator */
	HTTP_MESSAGE_ME(rewind, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(valid, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(current, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(key, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(next, ZEND_ACC_PUBLIC)

	ZEND_MALIAS(HttpMessage, __toString, toString, HTTP_ARGS(HttpMessage, toString), ZEND_ACC_PUBLIC)

	HTTP_MESSAGE_ME(fromString, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
	
	HTTP_MESSAGE_ME(detach, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(prepend, ZEND_ACC_PUBLIC)
	HTTP_MESSAGE_ME(reverse, ZEND_ACC_PUBLIC)

	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers http_message_object_handlers;

PHP_MINIT_FUNCTION(http_message_object)
{
	HTTP_REGISTER_CLASS_EX(HttpMessage, http_message_object, NULL, 0);
	
#ifndef WONKY
#	ifdef HAVE_SPL
	zend_class_implements(http_message_object_ce TSRMLS_CC, 3, spl_ce_Countable, zend_ce_serializable, zend_ce_iterator);
#	else
	zend_class_implements(http_message_object_ce TSRMLS_CC, 2, zend_ce_serializable, zend_ce_iterator);
#	endif
#else
	zend_class_implements(http_message_object_ce TSRMLS_CC, 1, zend_ce_iterator);
#endif
	
	http_message_object_handlers.clone_obj = _http_message_object_clone_obj;
	http_message_object_handlers.read_property = http_message_object_read_prop;
	http_message_object_handlers.write_property = http_message_object_write_prop;
	http_message_object_handlers.get_properties = http_message_object_get_props;
	http_message_object_handlers.get_property_ptr_ptr = NULL;
	
	DCL_PROP(PROTECTED, long, type, HTTP_MSG_NONE);
	DCL_PROP(PROTECTED, string, body, "");
	DCL_PROP(PROTECTED, string, requestMethod, "");
	DCL_PROP(PROTECTED, string, requestUrl, "");
	DCL_PROP(PROTECTED, string, responseStatus, "");
	DCL_PROP(PROTECTED, long, responseCode, 0);
	DCL_PROP_N(PROTECTED, httpVersion);
	DCL_PROP_N(PROTECTED, headers);
	DCL_PROP_N(PROTECTED, parentMessage);
	
#ifndef WONKY
	DCL_CONST(long, "TYPE_NONE", HTTP_MSG_NONE);
	DCL_CONST(long, "TYPE_REQUEST", HTTP_MSG_REQUEST);
	DCL_CONST(long, "TYPE_RESPONSE", HTTP_MSG_RESPONSE);
#endif
	
	HTTP_LONG_CONSTANT("HTTP_MSG_NONE", HTTP_MSG_NONE);
	HTTP_LONG_CONSTANT("HTTP_MSG_REQUEST", HTTP_MSG_REQUEST);
	HTTP_LONG_CONSTANT("HTTP_MSG_RESPONSE", HTTP_MSG_RESPONSE);
	
	return SUCCESS;
}

void _http_message_object_reverse(zval *this_ptr, zval *return_value TSRMLS_DC)
{
	int i;
	getObject(http_message_object, obj);
	
	/* count */
	http_message_count(i, obj->message);
	
	if (i > 1) {
		zval o;
		zend_object_value *ovalues = NULL;
		http_message_object **objects = NULL;
		int last = i - 1;
		
		objects = ecalloc(i, sizeof(http_message_object *));
		ovalues = ecalloc(i, sizeof(zend_object_value));
	
		/* we are the first message */
		objects[0] = obj;
		ovalues[0] = getThis()->value.obj;
	
		/* fetch parents */
		INIT_PZVAL(&o);
		o.type = IS_OBJECT;
		for (i = 1; obj->parent.handle; ++i) {
			o.value.obj = obj->parent;
			ovalues[i] = o.value.obj;
			objects[i] = obj = zend_object_store_get_object(&o TSRMLS_CC);
		}
		
		/* reorder parents */
		for (last = --i; i; --i) {
			objects[i]->message->parent = objects[i-1]->message;
			objects[i]->parent = ovalues[i-1];
		}
		objects[0]->message->parent = NULL;
		objects[0]->parent.handle = 0;
		objects[0]->parent.handlers = NULL;
		
		/* add ref (why?) */
		Z_OBJ_ADDREF_P(getThis());
		RETVAL_OBJVAL(ovalues[last], 1);
		
		efree(objects);
		efree(ovalues);
	} else {
		RETURN_ZVAL(getThis(), 1, 0);
	}
}

void _http_message_object_prepend_ex(zval *this_ptr, zval *prepend, zend_bool top TSRMLS_DC)
{
	zval m;
	http_message *save_parent_msg = NULL;
	zend_object_value save_parent_obj = {0, NULL};
	getObject(http_message_object, obj);
	getObjectEx(http_message_object, prepend_obj, prepend);
		
	INIT_PZVAL(&m);
	m.type = IS_OBJECT;
		
	if (!top) {
		save_parent_obj = obj->parent;
		save_parent_msg = obj->message->parent;
	} else {
		/* iterate to the most parent object */
		while (obj->parent.handle) {
			m.value.obj = obj->parent;
			obj = zend_object_store_get_object(&m TSRMLS_CC);
		}
	}
		
	/* prepend */
	obj->parent = prepend->value.obj;
	obj->message->parent = prepend_obj->message;
		
	/* add ref */
	zend_objects_store_add_ref(prepend TSRMLS_CC);
	while (prepend_obj->parent.handle) {
		m.value.obj = prepend_obj->parent;
		zend_objects_store_add_ref(&m TSRMLS_CC);
		prepend_obj = zend_object_store_get_object(&m TSRMLS_CC);
	}
		
	if (!top) {
		prepend_obj->parent = save_parent_obj;
		prepend_obj->message->parent = save_parent_msg;
	}
}

zend_object_value _http_message_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return http_message_object_new_ex(ce, NULL, NULL);
}

zend_object_value _http_message_object_new_ex(zend_class_entry *ce, http_message *msg, http_message_object **ptr TSRMLS_DC)
{
	zend_object_value ov;
	http_message_object *o;

	o = ecalloc(1, sizeof(http_message_object));
	o->zo.ce = ce;
	
	if (ptr) {
		*ptr = o;
	}

	if (msg) {
		o->message = msg;
		if (msg->parent) {
			o->parent = http_message_object_new_ex(ce, msg->parent, NULL);
		}
	}

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);

	ov.handle = putObject(http_message_object, o);
	ov.handlers = &http_message_object_handlers;

	return ov;
}

zend_object_value _http_message_object_clone_obj(zval *this_ptr TSRMLS_DC)
{
	getObject(http_message_object, obj);
	return http_message_object_new_ex(Z_OBJCE_P(this_ptr), http_message_dup(obj->message), NULL);
}

void _http_message_object_free(zend_object *object TSRMLS_DC)
{
	http_message_object *o = (http_message_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	if (o->message) {
		http_message_dtor(o->message);
		efree(o->message);
	}
	if (o->parent.handle) {
		zval p;
		
		INIT_PZVAL(&p);
		p.type = IS_OBJECT;
		p.value.obj = o->parent;
		zend_objects_store_del_ref(&p TSRMLS_CC);
	}
	efree(o);
}

static zval *_http_message_object_read_prop(zval *object, zval *member, int type TSRMLS_DC)
{
	getObjectEx(http_message_object, obj, object);
	http_message *msg = obj->message;
	zval *return_value;
#ifdef WONKY
	ulong h = zend_get_hash_value(Z_STRVAL_P(member), Z_STRLEN_P(member)+1);
#else
	zend_property_info *pinfo = zend_get_property_info(obj->zo.ce, member, 1 TSRMLS_CC);
	
	if (!pinfo || ACC_PROP_PUBLIC(pinfo->flags)) {
		return zend_get_std_object_handlers()->read_property(object, member, type TSRMLS_CC);
	}
#endif

	if (type == BP_VAR_W) {
		zend_error(E_ERROR, "Cannot access HttpMessage properties by reference or array key/index");
		return NULL;
	}
	
	ALLOC_ZVAL(return_value);
	return_value->refcount = 0;
	return_value->is_ref = 0;

#ifdef WONKY
	switch (h)
#else
	switch (pinfo->h)
#endif
	{
		case HTTP_MSG_PROPHASH_TYPE:
		case HTTP_MSG_CHILD_PROPHASH_TYPE:
			RETVAL_LONG(msg->type);
		break;

		case HTTP_MSG_PROPHASH_HTTP_VERSION:
		case HTTP_MSG_CHILD_PROPHASH_HTTP_VERSION:
			RETVAL_DOUBLE(msg->http.version);
		break;

		case HTTP_MSG_PROPHASH_BODY:
		case HTTP_MSG_CHILD_PROPHASH_BODY:
			phpstr_fix(PHPSTR(msg));
			RETVAL_PHPSTR(PHPSTR(msg), 0, 1);
		break;

		case HTTP_MSG_PROPHASH_HEADERS:
		case HTTP_MSG_CHILD_PROPHASH_HEADERS:
			array_init(return_value);
			zend_hash_copy(Z_ARRVAL_P(return_value), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		break;

		case HTTP_MSG_PROPHASH_PARENT_MESSAGE:
		case HTTP_MSG_CHILD_PROPHASH_PARENT_MESSAGE:
			if (msg->parent) {
				RETVAL_OBJVAL(obj->parent, 1);
			} else {
				RETVAL_NULL();
			}
		break;

		case HTTP_MSG_PROPHASH_REQUEST_METHOD:
		case HTTP_MSG_CHILD_PROPHASH_REQUEST_METHOD:
			if (HTTP_MSG_TYPE(REQUEST, msg) && msg->http.info.request.method) {
				RETVAL_STRING(msg->http.info.request.method, 1);
			} else {
				RETVAL_NULL();
			}
		break;

		case HTTP_MSG_PROPHASH_REQUEST_URL:
		case HTTP_MSG_CHILD_PROPHASH_REQUEST_URL:
			if (HTTP_MSG_TYPE(REQUEST, msg) && msg->http.info.request.url) {
				RETVAL_STRING(msg->http.info.request.url, 1);
			} else {
				RETVAL_NULL();
			}
		break;

		case HTTP_MSG_PROPHASH_RESPONSE_CODE:
		case HTTP_MSG_CHILD_PROPHASH_RESPONSE_CODE:
			if (HTTP_MSG_TYPE(RESPONSE, msg)) {
				RETVAL_LONG(msg->http.info.response.code);
			} else {
				RETVAL_NULL();
			}
		break;
		
		case HTTP_MSG_PROPHASH_RESPONSE_STATUS:
		case HTTP_MSG_CHILD_PROPHASH_RESPONSE_STATUS:
			if (HTTP_MSG_TYPE(RESPONSE, msg) && msg->http.info.response.status) {
				RETVAL_STRING(msg->http.info.response.status, 1);
			} else {
				RETVAL_NULL();
			}
		break;
		
		default:
#ifdef WONKY
			return zend_get_std_object_handlers()->read_property(object, member, type TSRMLS_CC);
#else
			RETVAL_NULL();
#endif
		break;
	}

	return return_value;
}

static void _http_message_object_write_prop(zval *object, zval *member, zval *value TSRMLS_DC)
{
	getObjectEx(http_message_object, obj, object);
	http_message *msg = obj->message;
	zval *cpy = NULL;
#ifdef WONKY
	ulong h = zend_get_hash_value(Z_STRVAL_P(member), Z_STRLEN_P(member) + 1);
#else
	zend_property_info *pinfo = zend_get_property_info(obj->zo.ce, member, 1 TSRMLS_CC);
	
	if (!pinfo || ACC_PROP_PUBLIC(pinfo->flags)) {
		zend_get_std_object_handlers()->write_property(object, member, value TSRMLS_CC);
		return;
	}
#endif
	
	cpy = zval_copy(Z_TYPE_P(value), value);
	
#ifdef WONKY
	switch (h)
#else
	switch (pinfo->h)
#endif
	{
		case HTTP_MSG_PROPHASH_TYPE:
		case HTTP_MSG_CHILD_PROPHASH_TYPE:
			convert_to_long(cpy);
			http_message_set_type(msg, Z_LVAL_P(cpy));
		break;

		case HTTP_MSG_PROPHASH_HTTP_VERSION:
		case HTTP_MSG_CHILD_PROPHASH_HTTP_VERSION:
			convert_to_double(cpy);
			msg->http.version = Z_DVAL_P(cpy);
		break;

		case HTTP_MSG_PROPHASH_BODY:
		case HTTP_MSG_CHILD_PROPHASH_BODY:
			convert_to_string(cpy);
			phpstr_dtor(PHPSTR(msg));
			phpstr_from_string_ex(PHPSTR(msg), Z_STRVAL_P(cpy), Z_STRLEN_P(cpy));
		break;

		case HTTP_MSG_PROPHASH_HEADERS:
		case HTTP_MSG_CHILD_PROPHASH_HEADERS:
			convert_to_array(cpy);
			zend_hash_clean(&msg->hdrs);
			zend_hash_copy(&msg->hdrs, Z_ARRVAL_P(cpy), (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		break;

		case HTTP_MSG_PROPHASH_PARENT_MESSAGE:
		case HTTP_MSG_CHILD_PROPHASH_PARENT_MESSAGE:
			if (Z_TYPE_P(value) == IS_OBJECT && instanceof_function(Z_OBJCE_P(value), http_message_object_ce TSRMLS_CC)) {
				if (msg->parent) {
					zval tmp;
					tmp.value.obj = obj->parent;
					Z_OBJ_DELREF(tmp);
				}
				Z_OBJ_ADDREF_P(value);
				obj->parent = value->value.obj;
			}
		break;

		case HTTP_MSG_PROPHASH_REQUEST_METHOD:
		case HTTP_MSG_CHILD_PROPHASH_REQUEST_METHOD:
			if (HTTP_MSG_TYPE(REQUEST, msg)) {
				convert_to_string(cpy);
				STR_SET(msg->http.info.request.method, estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy)));
			}
		break;

		case HTTP_MSG_PROPHASH_REQUEST_URL:
		case HTTP_MSG_CHILD_PROPHASH_REQUEST_URL:
			if (HTTP_MSG_TYPE(REQUEST, msg)) {
				convert_to_string(cpy);
				STR_SET(msg->http.info.request.url, estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy)));
			}
		break;

		case HTTP_MSG_PROPHASH_RESPONSE_CODE:
		case HTTP_MSG_CHILD_PROPHASH_RESPONSE_CODE:
			if (HTTP_MSG_TYPE(RESPONSE, msg)) {
				convert_to_long(cpy);
				msg->http.info.response.code = Z_LVAL_P(cpy);
			}
		break;
		
		case HTTP_MSG_PROPHASH_RESPONSE_STATUS:
		case HTTP_MSG_CHILD_PROPHASH_RESPONSE_STATUS:
			if (HTTP_MSG_TYPE(RESPONSE, msg)) {
				convert_to_string(cpy);
				STR_SET(msg->http.info.response.status, estrndup(Z_STRVAL_P(cpy), Z_STRLEN_P(cpy)));
			}
		break;
		
		default:
#ifdef WONKY
			zend_get_std_object_handlers()->write_property(object, member, value TSRMLS_CC);
#endif
		break;
	}
	zval_free(&cpy);
}

static HashTable *_http_message_object_get_props(zval *object TSRMLS_DC)
{
	zval *headers;
	getObjectEx(http_message_object, obj, object);
	http_message *msg = obj->message;
	HashTable *props = OBJ_PROP(obj);
	zval array, *parent;
	
	INIT_ZARR(array, props);

#define ASSOC_PROP(array, ptype, name, val) \
	{ \
		char *m_prop_name; \
		int m_prop_len; \
		zend_mangle_property_name(&m_prop_name, &m_prop_len, "*", 1, name, lenof(name), 0); \
		add_assoc_ ##ptype## _ex(&array, m_prop_name, sizeof(name)+3, val); \
		efree(m_prop_name); \
	}
#define ASSOC_STRING(array, name, val) ASSOC_STRINGL(array, name, val, strlen(val))
#define ASSOC_STRINGL(array, name, val, len) \
	{ \
		char *m_prop_name; \
		int m_prop_len; \
		zend_mangle_property_name(&m_prop_name, &m_prop_len, "*", 1, name, lenof(name), 0); \
		add_assoc_stringl_ex(&array, m_prop_name, sizeof(name)+3, val, len, 1); \
		efree(m_prop_name); \
	}

	ASSOC_PROP(array, long, "type", msg->type);
	ASSOC_PROP(array, double, "httpVersion", msg->http.version);

	switch (msg->type)
	{
		case HTTP_MSG_REQUEST:
			ASSOC_PROP(array, long, "responseCode", 0);
			ASSOC_STRINGL(array, "responseStatus", "", 0);
			ASSOC_STRING(array, "requestMethod", msg->http.info.request.method);
			ASSOC_STRING(array, "requestUrl", msg->http.info.request.url);
		break;

		case HTTP_MSG_RESPONSE:
			ASSOC_PROP(array, long, "responseCode", msg->http.info.response.code);
			ASSOC_STRING(array, "responseStatus", msg->http.info.response.status);
			ASSOC_STRINGL(array, "requestMethod", "", 0);
			ASSOC_STRINGL(array, "requestUrl", "", 0);
		break;

		case HTTP_MSG_NONE:
		default:
			ASSOC_PROP(array, long, "responseCode", 0);
			ASSOC_STRINGL(array, "responseStatus", "", 0);
			ASSOC_STRINGL(array, "requestMethod", "", 0);
			ASSOC_STRINGL(array, "requestUrl", "", 0);
		break;
	}

	MAKE_STD_ZVAL(headers);
	array_init(headers);
	zend_hash_copy(Z_ARRVAL_P(headers), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	ASSOC_PROP(array, zval, "headers", headers);
	ASSOC_STRINGL(array, "body", PHPSTR_VAL(msg), PHPSTR_LEN(msg));
	
	MAKE_STD_ZVAL(parent);
	if (msg->parent) {
		ZVAL_OBJVAL(parent, obj->parent, 1);
	} else {
		ZVAL_NULL(parent);
	}
	ASSOC_PROP(array, zval, "parentMessage", parent);

	return OBJ_PROP(obj);
}

/* ### USERLAND ### */

/* {{{ proto void HttpMessage::__construct([string message])
 *
 * Instantiate a new HttpMessage object.
 * 
 * Accepts an optional string parameter containing a single or several 
 * consecutive HTTP messages.  The constructed object will actually 
 * represent the *last* message of the passed string.  If there were
 * prior messages, those can be accessed by HttpMessage::getParentMessage().
 * 
 * Throws HttpMalformedHeaderException.
 */
PHP_METHOD(HttpMessage, __construct)
{
	int length = 0;
	char *message = NULL;
	
	getObject(http_message_object, obj);
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &message, &length) && message && length) {
		http_message *msg = obj->message;
		
		http_message_dtor(msg);
		if ((obj->message = http_message_parse_ex(msg, message, length))) {
			if (obj->message->parent) {
				obj->parent = http_message_object_new_ex(Z_OBJCE_P(getThis()), obj->message->parent, NULL);
			}
		} else {
			obj->message = http_message_init(msg);
		}
	}
	if (!obj->message) {
		obj->message = http_message_new();
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto static HttpMessage HttpMessage::fromString(string raw_message[, string class_name = "HttpMessage"])
 *
 * Create an HttpMessage object from a string. Kind of a static constructor.
 * 
 * Expects a string parameter containing a single or several consecutive
 * HTTP messages.  Accepts an optional string parameter specifying the class to use.
 * 
 * Returns an HttpMessage object on success or NULL on failure.
 * 
 * Throws HttpMalformedHeadersException.
 */
PHP_METHOD(HttpMessage, fromString)
{
	char *string = NULL, *class_name = NULL;
	int length = 0, class_length = 0;
	http_message *msg = NULL;

	RETVAL_NULL();
	
	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &string, &length, &class_name, &class_length)) {
		if ((msg = http_message_parse(string, length))) {
			zend_class_entry *ce = http_message_object_ce;
			
			if (class_name && *class_name) {
				ce = zend_fetch_class(class_name, class_length, ZEND_FETCH_CLASS_DEFAULT TSRMLS_CC);
				if (ce && !instanceof_function(ce, http_message_object_ce TSRMLS_CC)) {
					http_error_ex(HE_WARNING, HTTP_E_RUNTIME, "Class %s does not extend HttpMessage", class_name);
					ce = NULL;
				}
			}
			if (ce) {
				RETVAL_OBJVAL(http_message_object_new_ex(ce, msg, NULL), 0);
			}
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto string HttpMessage::getBody()
 *
 * Get the body of the parsed HttpMessage.
 * 
 * Returns the message body as string.
 */
PHP_METHOD(HttpMessage, getBody)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);
		RETURN_PHPSTR(&obj->message->body, PHPSTR_FREE_NOT, 1);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::setBody(string body)
 *
 * Set the body of the HttpMessage.
 * NOTE: Don't forget to update any headers accordingly.
 * 
 * Expects a string parameter containing the new body of the message.
 */
PHP_METHOD(HttpMessage, setBody)
{
	char *body;
	int len;
	getObject(http_message_object, obj);
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &body, &len)) {
		phpstr_dtor(PHPSTR(obj->message));
		phpstr_from_string_ex(PHPSTR(obj->message), body, len);		
	}
}
/* }}} */

/* {{{ proto array HttpMessage::getHeaders()
 *
 * Get Message Headers.
 * 
 * Returns an associative array containing the messages HTTP headers.
 */
PHP_METHOD(HttpMessage, getHeaders)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval headers;
		getObject(http_message_object, obj);

		INIT_ZARR(headers, &obj->message->hdrs);
		array_init(return_value);
		array_copy(&headers, return_value);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::setHeaders(array headers)
 *
 * Sets new headers.
 * 
 * Expects an associative array as parameter containing the new HTTP headers,
 * which will replace *all* previous HTTP headers of the message.
 */
PHP_METHOD(HttpMessage, setHeaders)
{
	zval *new_headers, old_headers;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &new_headers)) {
		return;
	}

	zend_hash_clean(&obj->message->hdrs);
	INIT_ZARR(old_headers, &obj->message->hdrs);
	array_copy(new_headers, &old_headers);
}
/* }}} */

/* {{{ proto void HttpMessage::addHeaders(array headers[, bool append = false])
 *
 * Add headers. If append is true, headers with the same name will be separated, else overwritten.
 * 
 * Expects an associative array as parameter containing the additional HTTP headers
 * to add to the messages existing headers.  If the optional bool parameter is true,
 * and a header with the same name of one to add exists already, this respective
 * header will be converted to an array containing both header values, otherwise
 * it will be overwritten with the new header value.
 */
PHP_METHOD(HttpMessage, addHeaders)
{
	zval old_headers, *new_headers;
	zend_bool append = 0;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|b", &new_headers, &append)) {
		return;
	}

	INIT_ZARR(old_headers, &obj->message->hdrs);
	if (append) {
		array_append(new_headers, &old_headers);
	} else {
		array_merge(new_headers, &old_headers);
	}
}
/* }}} */

/* {{{ proto int HttpMessage::getType()
 *
 * Get Message Type. (HTTP_MSG_NONE|HTTP_MSG_REQUEST|HTTP_MSG_RESPONSE)
 * 
 * Returns the HttpMessage::TYPE.
 */
PHP_METHOD(HttpMessage, getType)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);
		RETURN_LONG(obj->message->type);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::setType(int type)
 *
 * Set Message Type. (HTTP_MSG_NONE|HTTP_MSG_REQUEST|HTTP_MSG_RESPONSE)
 * 
 * Expects an int parameter, the HttpMessage::TYPE.
 */
PHP_METHOD(HttpMessage, setType)
{
	long type;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &type)) {
		return;
	}
	http_message_set_type(obj->message, type);
}
/* }}} */

/* {{{ proto int HttpMessage::getResponseCode()
 *
 * Get the Response Code of the Message.
 * 
 * Returns the HTTP response code if the message is of type 
 * HttpMessage::TYPE_RESPONSE, else FALSE.
 */
PHP_METHOD(HttpMessage, getResponseCode)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);
		HTTP_CHECK_MESSAGE_TYPE_RESPONSE(obj->message, RETURN_FALSE);
		RETURN_LONG(obj->message->http.info.response.code);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setResponseCode(int code)
 *
 * Set the response code of an HTTP Response Message.
 * 
 * Expects an int parameter with the HTTP response code.
 * 
 * Returns TRUE on success, or FALSE if the message is not of type
 * HttpMessage::TYPE_RESPONSE or the response code is out of range (100-510).
 */
PHP_METHOD(HttpMessage, setResponseCode)
{
	long code;
	getObject(http_message_object, obj);

	HTTP_CHECK_MESSAGE_TYPE_RESPONSE(obj->message, RETURN_FALSE);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &code)) {
		RETURN_FALSE;
	}
	if (code < 100 || code > 510) {
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "Invalid response code (100-510): %ld", code);
		RETURN_FALSE;
	}

	obj->message->http.info.response.code = code;
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getResponseStatus()
 *
 * Get the Response Status of the message (i.e. the string following the response code).
 *
 * Returns the HTTP response status string if the message is of type 
 * HttpMessage::TYPE_RESPONSE, else FALSE.
 */
PHP_METHOD(HttpMessage, getResponseStatus)
{
	NO_ARGS;
	
	IF_RETVAL_USED {
		getObject(http_message_object, obj);
		HTTP_CHECK_MESSAGE_TYPE_RESPONSE(obj->message, RETURN_FALSE);
		RETURN_STRING(obj->message->http.info.response.status, 1);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setResponseStatus(string status)
 *
 * Set the Response Status of the HTTP message (i.e. the string following the response code).
 *
 * Expects a string parameter containing the response status text.
 *
 * Returns TRUE on success or FALSE if the message is not of type
 * HttpMessage::TYPE_RESPONSE.
 */
PHP_METHOD(HttpMessage, setResponseStatus)
{
	char *status;
	int status_len;
	getObject(http_message_object, obj);
	
	HTTP_CHECK_MESSAGE_TYPE_RESPONSE(obj->message, RETURN_FALSE);
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &status, &status_len)) {
		RETURN_FALSE;
	}
	STR_SET(obj->message->http.info.response.status, estrdup(status));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getRequestMethod()
 *
 * Get the Request Method of the Message.
 * 
 * Returns the request method name on success, or FALSE if the message is
 * not of type HttpMessage::TYPE_REQUEST.
 */
PHP_METHOD(HttpMessage, getRequestMethod)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);
		HTTP_CHECK_MESSAGE_TYPE_REQUEST(obj->message, RETURN_FALSE);
		RETURN_STRING(obj->message->http.info.request.method, 1);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setRequestMethod(string method)
 *
 * Set the Request Method of the HTTP Message.
 * 
 * Expects a string parameter containing the request method name.
 * 
 * Returns TRUE on success, or FALSE if the message is not of type
 * HttpMessage::TYPE_REQUEST or an invalid request method was supplied. 
 */
PHP_METHOD(HttpMessage, setRequestMethod)
{
	char *method;
	int method_len;
	getObject(http_message_object, obj);

	HTTP_CHECK_MESSAGE_TYPE_REQUEST(obj->message, RETURN_FALSE);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &method, &method_len)) {
		RETURN_FALSE;
	}
	if (method_len < 1) {
		http_error(HE_WARNING, HTTP_E_INVALID_PARAM, "Cannot set HttpMessage::requestMethod to an empty string");
		RETURN_FALSE;
	}
	if (SUCCESS != http_check_method(method)) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST_METHOD, "Unkown request method: %s", method);
		RETURN_FALSE;
	}

	STR_SET(obj->message->http.info.request.method, estrndup(method, method_len));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getRequestUrl()
 *
 * Get the Request URL of the Message.
 * 
 * Returns the request url as string on success, or FALSE if the message
 * is not of type HttpMessage::TYPE_REQUEST. 
 */
PHP_METHOD(HttpMessage, getRequestUrl)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_message_object, obj);
		HTTP_CHECK_MESSAGE_TYPE_REQUEST(obj->message, RETURN_FALSE);
		RETURN_STRING(obj->message->http.info.request.url, 1);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setRequestUrl(string url)
 *
 * Set the Request URL of the HTTP Message.
 * 
 * Expects a string parameters containing the request url.
 * 
 * Returns TRUE on success, or FALSE if the message is not of type
 * HttpMessage::TYPE_REQUEST or supplied URL was empty.
 */
PHP_METHOD(HttpMessage, setRequestUrl)
{
	char *URI;
	int URIlen;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &URI, &URIlen)) {
		RETURN_FALSE;
	}
	HTTP_CHECK_MESSAGE_TYPE_REQUEST(obj->message, RETURN_FALSE);
	if (URIlen < 1) {
		http_error(HE_WARNING, HTTP_E_INVALID_PARAM, "Cannot set HttpMessage::requestUrl to an empty string");
		RETURN_FALSE;
	}

	STR_SET(obj->message->http.info.request.url, estrndup(URI, URIlen));
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpMessage::getHttpVersion()
 *
 * Get the HTTP Protocol Version of the Message.
 * 
 * Returns the HTTP protocol version as string.
 */
PHP_METHOD(HttpMessage, getHttpVersion)
{
	NO_ARGS;

	IF_RETVAL_USED {
		char ver[4] = {0};
		getObject(http_message_object, obj);

		sprintf(ver, "%1.1lf", obj->message->http.version);
		RETURN_STRINGL(ver, 3, 1);
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::setHttpVersion(string version)
 *
 * Set the HTTP Protocol version of the Message.
 * 
 * Expects a string parameter containing the HTTP protocol version.
 * 
 * Returns TRUE on success, or FALSE if supplied version is out of range (1.0/1.1).
 */
PHP_METHOD(HttpMessage, setHttpVersion)
{
	char v[4];
	zval *zv;
	getObject(http_message_object, obj);

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z/", &zv)) {
		return;
	}

	convert_to_double(zv);
	sprintf(v, "%1.1lf", Z_DVAL_P(zv));
	if (strcmp(v, "1.0") && strcmp(v, "1.1")) {
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "Invalid HTTP protocol version (1.0 or 1.1): %s", v);
		RETURN_FALSE;
	}

	obj->message->http.version = Z_DVAL_P(zv);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto HttpMessage HttpMessage::getParentMessage()
 *
 * Get parent Message.
 * 
 * Returns the parent HttpMessage on success, or NULL if there's none.
 *
 * Throws HttpMessageException.
 */
PHP_METHOD(HttpMessage, getParentMessage)
{
	SET_EH_THROW_HTTP();
	NO_ARGS {
		getObject(http_message_object, obj);

		if (obj->message->parent) {
			RETVAL_OBJVAL(obj->parent, 1);
		} else {
			http_error(HE_WARNING, HTTP_E_RUNTIME, "HttpMessage does not have a parent message");
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto bool HttpMessage::send()
 *
 * Send the Message according to its type as Response or Request.
 * This provides limited functionality compared to HttpRequest and HttpResponse.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpMessage, send)
{
	getObject(http_message_object, obj);

	NO_ARGS;

	RETURN_SUCCESS(http_message_send(obj->message));
}
/* }}} */

/* {{{ proto string HttpMessage::toString([bool include_parent = false])
 *
 * Get the string representation of the Message.
 * 
 * Accepts a bool parameter which specifies whether the returned string
 * should also contain any parent messages.
 * 
 * Returns the full message as string.
 */
PHP_METHOD(HttpMessage, toString)
{
	IF_RETVAL_USED {
		char *string;
		size_t length;
		zend_bool include_parent = 0;
		getObject(http_message_object, obj);

		if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &include_parent)) {
			RETURN_FALSE;
		}

		if (include_parent) {
			http_message_serialize(obj->message, &string, &length);
		} else {
			http_message_tostring(obj->message, &string, &length);
		}
		RETURN_STRINGL(string, length, 0);
	}
}
/* }}} */

/* {{{ proto HttpRequest|HttpResponse HttpMessage::toMessageTypeObject(void)
 *
 * Creates an object regarding to the type of the message.
 *
 * Returns either an HttpRequest or HttpResponse object on success, or NULL on failure.
 *
 * Throws HttpRuntimeException, HttpMessageTypeException, HttpHeaderException.
 */
PHP_METHOD(HttpMessage, toMessageTypeObject)
{
	SET_EH_THROW_HTTP();
	
	NO_ARGS;
	
	IF_RETVAL_USED {
		getObject(http_message_object, obj);
		
		switch (obj->message->type)
		{
			case HTTP_MSG_REQUEST:
			{
#ifdef HTTP_HAVE_CURL
				int method;
				char *url;
				zval tmp, body, *array, *headers, *host = http_message_header(obj->message, "Host");
				php_url hurl, *purl = php_url_parse(obj->message->http.info.request.url);
				
				MAKE_STD_ZVAL(array);
				array_init(array);
				
				memset(&hurl, 0, sizeof(php_url));
				hurl.host = host ? Z_STRVAL_P(host) : NULL;
				http_build_url(HTTP_URL_REPLACE, purl, &hurl, NULL, &url, NULL);
				php_url_free(purl);
				add_assoc_string(array, "url", url, 0);
				
				if (	(method = http_request_method_exists(1, 0, obj->message->http.info.request.method)) ||
						(method = http_request_method_register(obj->message->http.info.request.method, strlen(obj->message->http.info.request.method)))) {
					add_assoc_long(array, "method", method);
				}
				
				if (10 == (int) (obj->message->http.version * 10)) {
					add_assoc_long(array, "protocol", CURL_HTTP_VERSION_1_0);
				}
				
				MAKE_STD_ZVAL(headers);
				array_init(headers);
				INIT_ZARR(tmp, &obj->message->hdrs);
				array_copy(&tmp, headers);
				add_assoc_zval(array, "headers", headers);
				
				object_init_ex(return_value, http_request_object_ce);
				zend_call_method_with_1_params(&return_value, http_request_object_ce, NULL, "setoptions", NULL, array);
				zval_ptr_dtor(&array);
				
				INIT_PZVAL(&body);
				ZVAL_STRINGL(&body, PHPSTR_VAL(obj->message), PHPSTR_LEN(obj->message), 0);
				zend_call_method_with_1_params(&return_value, http_request_object_ce, NULL, "setrawpostdata", NULL, &body);
#else
				http_error(HE_WARNING, HTTP_E_RUNTIME, "Cannot transform HttpMessage to HttpRequest (missing curl support)");
#endif
			}
			break;
			
			case HTTP_MSG_RESPONSE:
			{
#ifndef WONKY
				HashPosition pos1, pos2;
				ulong idx;
				uint key_len;
				char *key = NULL;
				zval **header, **h, *body;
				
				if (obj->message->http.info.response.code) {
					http_send_status(obj->message->http.info.response.code);
				}
				
				object_init_ex(return_value, http_response_object_ce);
				
				FOREACH_HASH_KEYLENVAL(pos1, &obj->message->hdrs, key, key_len, idx, header) {
					if (key) {
						zval zkey;
						
						INIT_PZVAL(&zkey);
						ZVAL_STRINGL(&zkey, key, key_len, 0);
						
						switch (Z_TYPE_PP(header))
						{
							case IS_ARRAY:
							case IS_OBJECT:
								FOREACH_HASH_VAL(pos2, HASH_OF(*header), h) {
									ZVAL_ADDREF(*h);
									zend_call_method_with_2_params(&return_value, http_response_object_ce, NULL, "setheader", NULL, &zkey, *h);
									zval_ptr_dtor(h);
								}
							break;
							
							default:
								ZVAL_ADDREF(*header);
								zend_call_method_with_2_params(&return_value, http_response_object_ce, NULL, "setheader", NULL, &zkey, *header);
								zval_ptr_dtor(header);
							break;
						}
						key = NULL;
					}
				}
				
				MAKE_STD_ZVAL(body);
				ZVAL_STRINGL(body, PHPSTR_VAL(obj->message), PHPSTR_LEN(obj->message), 1);
				zend_call_method_with_1_params(&return_value, http_response_object_ce, NULL, "setdata", NULL, body);
				zval_ptr_dtor(&body);
#else
				http_error(HE_WARNING, HTTP_E_RUNTIME, "Cannot transform HttpMessage to HttpResponse (need PHP 5.1+)");
#endif
			}
			break;
			
			default:
				http_error(HE_WARNING, HTTP_E_MESSAGE_TYPE, "HttpMessage is neither of type HttpMessage::TYPE_REQUEST nor HttpMessage::TYPE_RESPONSE");
			break;
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto int HttpMessage::count()
 *
 * Implements Countable.
 * 
 * Returns the number of parent messages + 1.
 */
PHP_METHOD(HttpMessage, count)
{
	NO_ARGS {
		long i;
		getObject(http_message_object, obj);
		
		http_message_count(i, obj->message);
		RETURN_LONG(i);
	}
}
/* }}} */

/* {{{ proto string HttpMessage::serialize()
 *
 * Implements Serializable.
 * 
 * Returns the serialized representation of the HttpMessage.
 */
PHP_METHOD(HttpMessage, serialize)
{
	NO_ARGS {
		char *string;
		size_t length;
		getObject(http_message_object, obj);
		
		http_message_serialize(obj->message, &string, &length);
		RETURN_STRINGL(string, length, 0);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::unserialize(string serialized)
 *
 * Implements Serializable.
 * 
 * Re-constructs the HttpMessage based upon the serialized string.
 */
PHP_METHOD(HttpMessage, unserialize)
{
	int length;
	char *serialized;
	getObject(http_message_object, obj);
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &serialized, &length)) {
		http_message_dtor(obj->message);
		if (!http_message_parse_ex(obj->message, serialized, (size_t) length)) {
			http_error(HE_ERROR, HTTP_E_RUNTIME, "Could not unserialize HttpMessage");
			http_message_init(obj->message);
		}
	}
}
/* }}} */

/* {{{ proto HttpMessage HttpMessage::detach(void)
 *
 * Returns a clone of an HttpMessage object detached from any parent messages.
 */
PHP_METHOD(HttpMessage, detach)
{
	http_info info;
	http_message *msg;
	getObject(http_message_object, obj);
	
	NO_ARGS;
	
	info.type = obj->message->type;
	memcpy(&HTTP_INFO(&info), &HTTP_INFO(obj->message), sizeof(struct http_info));
	
	msg = http_message_new();
	http_message_set_info(msg, &info);
	
	zend_hash_copy(&msg->hdrs, &obj->message->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
	phpstr_append(&msg->body, PHPSTR_VAL(obj->message), PHPSTR_LEN(obj->message));
	
	RETVAL_OBJVAL(http_message_object_new_ex(Z_OBJCE_P(getThis()), msg, NULL), 0);
}
/* }}} */

/* {{{ proto void HttpMessage::prepend(HttpMessage message)
 *
 * Prepends message(s) to the HTTP message.
 *
 * Expects an HttpMessage object as parameter.
 */
PHP_METHOD(HttpMessage, prepend)
{
	zval *prepend;
	zend_bool top = 1;
	
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "O|b", &prepend, http_message_object_ce, &top)) {
		http_message_object_prepend_ex(getThis(), prepend, top);
	}
}
/* }}} */

/* {{{ proto HttpMessage HttpMessage::reverse()
 *
 * Reorders the message chain in reverse order.
 *
 * Returns the most parent HttpMessage object.
 */
PHP_METHOD(HttpMessage, reverse)
{
	NO_ARGS {
		http_message_object_reverse(getThis(), return_value);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::rewind(void)
 *
 * Implements Iterator.
 */
PHP_METHOD(HttpMessage, rewind)
{
	NO_ARGS {
		getObject(http_message_object, obj);
		
		if (obj->iterator) {
			zval_ptr_dtor(&obj->iterator);
		}
		ZVAL_ADDREF(getThis());
		obj->iterator = getThis();
	}
}
/* }}} */

/* {{{ proto bool HttpMessage::valid(void)
 *
 * Implements Iterator.
 */
PHP_METHOD(HttpMessage, valid)
{
	NO_ARGS {
		getObject(http_message_object, obj);
		
		RETURN_BOOL(obj->iterator != NULL);
	}
}
/* }}} */

/* {{{ proto void HttpMessage::next(void)
 *
 * Implements Iterator.
 */
PHP_METHOD(HttpMessage, next)
{
	NO_ARGS {
		getObject(http_message_object, obj);
		getObjectEx(http_message_object, itr, obj->iterator);
		
		if (itr && itr->parent.handle) {
			zval *old = obj->iterator;
			MAKE_STD_ZVAL(obj->iterator);
			ZVAL_OBJVAL(obj->iterator, itr->parent, 1);
			zval_ptr_dtor(&old);
		} else {
			zval_ptr_dtor(&obj->iterator);
			obj->iterator = NULL;
		}
	}
}
/* }}} */

/* {{{ proto int HttpMessage::key(void)
 *
 * Implements Iterator.
 */
PHP_METHOD(HttpMessage, key)
{
	NO_ARGS {
		getObject(http_message_object, obj);
		
		RETURN_LONG(obj->iterator ? obj->iterator->value.obj.handle:0);
	}
}
/* }}} */

/* {{{ proto HttpMessage HttpMessage::current(void)
 *
 * Implements Iterator.
 */
PHP_METHOD(HttpMessage, current)
{
	NO_ARGS {
		getObject(http_message_object, obj);
		
		if (obj->iterator) {
			RETURN_ZVAL(obj->iterator, 1, 0);
		}
	}
}
/* }}} */

#endif /* ZEND_ENGINE_2 */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

