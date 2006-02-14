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

#define HTTP_WANT_CURL
#include "php_http.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)

#include "zend_interfaces.h"

#include "php_http_api.h"
#include "php_http_cookie_api.h"
#include "php_http_exception_object.h"
#include "php_http_message_api.h"
#include "php_http_message_object.h"
#include "php_http_request_api.h"
#include "php_http_request_object.h"
#include "php_http_request_pool_api.h"
#include "php_http_url_api.h"

#define HTTP_BEGIN_ARGS(method, req_args) 	HTTP_BEGIN_ARGS_EX(HttpRequest, method, 0, req_args)
#define HTTP_EMPTY_ARGS(method)				HTTP_EMPTY_ARGS_EX(HttpRequest, method, 0)
#define HTTP_REQUEST_ME(method, visibility)	PHP_ME(HttpRequest, method, HTTP_ARGS(HttpRequest, method), visibility)
#define HTTP_REQUEST_ALIAS(method, func)	HTTP_STATIC_ME_ALIAS(method, func, HTTP_ARGS(HttpRequest, method))

HTTP_BEGIN_ARGS(__construct, 0)
	HTTP_ARG_VAL(url, 0)
	HTTP_ARG_VAL(method, 0)
	HTTP_ARG_VAL(options, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getOptions);
HTTP_BEGIN_ARGS(setOptions, 0)
	HTTP_ARG_VAL(options, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getSslOptions);
HTTP_BEGIN_ARGS(setSslOptions, 0)
	HTTP_ARG_VAL(ssl_options, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addSslOptions, 0)
	HTTP_ARG_VAL(ssl_optins, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getHeaders);
HTTP_BEGIN_ARGS(setHeaders, 0)
	HTTP_ARG_VAL(headers, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addHeaders, 1)
	HTTP_ARG_VAL(headers, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getCookies);
HTTP_BEGIN_ARGS(setCookies, 0)
	HTTP_ARG_VAL(cookies, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addCookies, 1)
	HTTP_ARG_VAL(cookies, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getUrl);
HTTP_BEGIN_ARGS(setUrl, 1)
	HTTP_ARG_VAL(url, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getMethod);
HTTP_BEGIN_ARGS(setMethod, 1)
	HTTP_ARG_VAL(request_method, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getContentType);
HTTP_BEGIN_ARGS(setContentType, 1)
	HTTP_ARG_VAL(content_type, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getQueryData);
HTTP_BEGIN_ARGS(setQueryData, 0)
	HTTP_ARG_VAL(query_data, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addQueryData, 1)
	HTTP_ARG_VAL(query_data, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getPostFields);
HTTP_BEGIN_ARGS(setPostFields, 0)
	HTTP_ARG_VAL(post_fields, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addPostFields, 1)
	HTTP_ARG_VAL(post_fields, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getPostFiles);
HTTP_BEGIN_ARGS(setPostFiles, 0)
	HTTP_ARG_VAL(post_files, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addPostFile, 2)
	HTTP_ARG_VAL(formname, 0)
	HTTP_ARG_VAL(filename, 0)
	HTTP_ARG_VAL(content_type, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getRawPostData);
HTTP_BEGIN_ARGS(setRawPostData, 0)
	HTTP_ARG_VAL(raw_post_data, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(addRawPostData, 1)
	HTTP_ARG_VAL(raw_post_data, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getPutFile);
HTTP_BEGIN_ARGS(setPutFile, 0)
	HTTP_ARG_VAL(filename, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getResponseData);
HTTP_BEGIN_ARGS(getResponseHeader, 0)
	HTTP_ARG_VAL(name, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(getResponseCookies, 0)
	HTTP_ARG_VAL(flags, 0)
	HTTP_ARG_VAL(allowed_extras, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getResponseBody);
HTTP_EMPTY_ARGS(getResponseCode);
HTTP_EMPTY_ARGS(getResponseStatus);
HTTP_BEGIN_ARGS(getResponseInfo, 0)
	HTTP_ARG_VAL(name, 0)
HTTP_END_ARGS;

HTTP_EMPTY_ARGS(getResponseMessage);
HTTP_EMPTY_ARGS(getRawResponseMessage);
HTTP_EMPTY_ARGS(getRequestMessage);
HTTP_EMPTY_ARGS(getRawRequestMessage);
HTTP_EMPTY_ARGS(getHistory);
HTTP_EMPTY_ARGS(clearHistory);
HTTP_EMPTY_ARGS(send);

HTTP_BEGIN_ARGS(get, 1)
	HTTP_ARG_VAL(url, 0)
	HTTP_ARG_VAL(options, 0)
	HTTP_ARG_VAL(info, 1)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(head, 1)
	HTTP_ARG_VAL(url, 0)
	HTTP_ARG_VAL(options, 0)
	HTTP_ARG_VAL(info, 1)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(postData, 2)
	HTTP_ARG_VAL(url, 0)
	HTTP_ARG_VAL(data, 0)
	HTTP_ARG_VAL(options, 0)
	HTTP_ARG_VAL(info, 1)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(postFields, 2)
	HTTP_ARG_VAL(url, 0)
	HTTP_ARG_VAL(data, 0)
	HTTP_ARG_VAL(options, 0)
	HTTP_ARG_VAL(info, 1)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(putFile, 2)
	HTTP_ARG_VAL(url, 0)
	HTTP_ARG_VAL(file, 0)
	HTTP_ARG_VAL(options, 0)
	HTTP_ARG_VAL(info, 1)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(putStream, 2)
	HTTP_ARG_VAL(url, 0)
	HTTP_ARG_VAL(stream, 0)
	HTTP_ARG_VAL(options, 0)
	HTTP_ARG_VAL(info, 1)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(methodRegister, 1)
	HTTP_ARG_VAL(method_name, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(methodUnregister, 1)
	HTTP_ARG_VAL(method, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(methodName, 1)
	HTTP_ARG_VAL(method_id, 0)
HTTP_END_ARGS;

HTTP_BEGIN_ARGS(methodExists, 1)
	HTTP_ARG_VAL(method, 0)
HTTP_END_ARGS;

#define http_request_object_declare_default_properties() _http_request_object_declare_default_properties(TSRMLS_C)
static inline void _http_request_object_declare_default_properties(TSRMLS_D);

#define OBJ_PROP_CE http_request_object_ce
zend_class_entry *http_request_object_ce;
zend_function_entry http_request_object_fe[] = {
	HTTP_REQUEST_ME(__construct, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)

	HTTP_REQUEST_ME(setOptions, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getOptions, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(setSslOptions, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getSslOptions, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(addSslOptions, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(addHeaders, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getHeaders, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(setHeaders, ZEND_ACC_PUBLIC)
	
	HTTP_REQUEST_ME(addCookies, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getCookies, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(setCookies, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setMethod, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getMethod, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setUrl, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getUrl, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setContentType, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getContentType, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setQueryData, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getQueryData, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(addQueryData, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setPostFields, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getPostFields, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(addPostFields, ZEND_ACC_PUBLIC)
	
	HTTP_REQUEST_ME(setRawPostData, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getRawPostData, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(addRawPostData, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setPostFiles, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(addPostFile, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getPostFiles, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(setPutFile, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getPutFile, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(send, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ME(getResponseData, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseHeader, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseCookies, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseCode, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseStatus, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseBody, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseInfo, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getResponseMessage, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getRawResponseMessage, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getRequestMessage, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getRawRequestMessage, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(getHistory, ZEND_ACC_PUBLIC)
	HTTP_REQUEST_ME(clearHistory, ZEND_ACC_PUBLIC)

	HTTP_REQUEST_ALIAS(get, http_get)
	HTTP_REQUEST_ALIAS(head, http_head)
	HTTP_REQUEST_ALIAS(postData, http_post_data)
	HTTP_REQUEST_ALIAS(postFields, http_post_fields)
	HTTP_REQUEST_ALIAS(putFile, http_put_file)
	HTTP_REQUEST_ALIAS(putStream, http_put_stream)

	HTTP_REQUEST_ALIAS(methodRegister, http_request_method_register)
	HTTP_REQUEST_ALIAS(methodUnregister, http_request_method_unregister)
	HTTP_REQUEST_ALIAS(methodName, http_request_method_name)
	HTTP_REQUEST_ALIAS(methodExists, http_request_method_exists)

	EMPTY_FUNCTION_ENTRY
};
static zend_object_handlers http_request_object_handlers;

PHP_MINIT_FUNCTION(http_request_object)
{
	HTTP_REGISTER_CLASS_EX(HttpRequest, http_request_object, NULL, 0);
	http_request_object_handlers.clone_obj = _http_request_object_clone_obj;
	return SUCCESS;
}

zend_object_value _http_request_object_new(zend_class_entry *ce TSRMLS_DC)
{
	return http_request_object_new_ex(ce, NULL, NULL);
}

zend_object_value _http_request_object_new_ex(zend_class_entry *ce, CURL *ch, http_request_object **ptr TSRMLS_DC)
{
	zend_object_value ov;
	http_request_object *o;

	o = ecalloc(1, sizeof(http_request_object));
	o->zo.ce = ce;
	o->request = http_request_init_ex(NULL, ch, 0, NULL);
	
	if (ptr) {
		*ptr = o;
	}

	ALLOC_HASHTABLE(OBJ_PROP(o));
	zend_hash_init(OBJ_PROP(o), 0, NULL, ZVAL_PTR_DTOR, 0);
	zend_hash_copy(OBJ_PROP(o), &ce->default_properties, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));

	ov.handle = putObject(http_request_object, o);
	ov.handlers = &http_request_object_handlers;

	return ov;
}

zend_object_value _http_request_object_clone_obj(zval *this_ptr TSRMLS_DC)
{
	zend_object *old_zo;
	zend_object_value new_ov;
	http_request_object *new_obj;
	getObject(http_request_object, old_obj);
	
	old_zo = zend_objects_get_address(this_ptr TSRMLS_CC);
	new_ov = http_request_object_new_ex(old_zo->ce, NULL, &new_obj);
	if (old_obj->request->ch) {
		http_curl_init_ex(curl_easy_duphandle(old_obj->request->ch), new_obj->request);
	}
	
	zend_objects_clone_members(&new_obj->zo, new_ov, old_zo, Z_OBJ_HANDLE_P(this_ptr) TSRMLS_CC);
	phpstr_append(&new_obj->request->conv.request, old_obj->request->conv.request.data, old_obj->request->conv.request.used);
	phpstr_append(&new_obj->request->conv.response, old_obj->request->conv.response.data, old_obj->request->conv.response.used);
	
	return new_ov;
}

static inline void _http_request_object_declare_default_properties(TSRMLS_D)
{
	zend_class_entry *ce = http_request_object_ce;

	DCL_PROP_N(PRIVATE, options);
	DCL_PROP_N(PRIVATE, postFields);
	DCL_PROP_N(PRIVATE, postFiles);
	DCL_PROP_N(PRIVATE, responseInfo);
	DCL_PROP_N(PRIVATE, responseData);
	DCL_PROP_N(PRIVATE, responseMessage);
	DCL_PROP(PRIVATE, long, responseCode, 0);
	DCL_PROP(PRIVATE, string, responseStatus, "");
	DCL_PROP(PRIVATE, long, method, HTTP_GET);
	DCL_PROP(PRIVATE, string, url, "");
	DCL_PROP(PRIVATE, string, contentType, "");
	DCL_PROP(PRIVATE, string, rawPostData, "");
	DCL_PROP(PRIVATE, string, queryData, "");
	DCL_PROP(PRIVATE, string, putFile, "");
	DCL_PROP_N(PRIVATE, history);
	DCL_PROP(PUBLIC, bool, recordHistory, 0);

#ifndef WONKY
	/*
	 * Request Method Constants
	 */
	/* HTTP/1.1 */
	DCL_CONST(long, "METH_GET", HTTP_GET);
	DCL_CONST(long, "METH_HEAD", HTTP_HEAD);
	DCL_CONST(long, "METH_POST", HTTP_POST);
	DCL_CONST(long, "METH_PUT", HTTP_PUT);
	DCL_CONST(long, "METH_DELETE", HTTP_DELETE);
	DCL_CONST(long, "METH_OPTIONS", HTTP_OPTIONS);
	DCL_CONST(long, "METH_TRACE", HTTP_TRACE);
	DCL_CONST(long, "METH_CONNECT", HTTP_CONNECT);
	/* WebDAV - RFC 2518 */
	DCL_CONST(long, "METH_PROPFIND", HTTP_PROPFIND);
	DCL_CONST(long, "METH_PROPPATCH", HTTP_PROPPATCH);
	DCL_CONST(long, "METH_MKCOL", HTTP_MKCOL);
	DCL_CONST(long, "METH_COPY", HTTP_COPY);
	DCL_CONST(long, "METH_MOVE", HTTP_MOVE);
	DCL_CONST(long, "METH_LOCK", HTTP_LOCK);
	DCL_CONST(long, "METH_UNLOCK", HTTP_UNLOCK);
	/* WebDAV Versioning - RFC 3253 */
	DCL_CONST(long, "METH_VERSION_CONTROL", HTTP_VERSION_CONTROL);
	DCL_CONST(long, "METH_REPORT", HTTP_REPORT);
	DCL_CONST(long, "METH_CHECKOUT", HTTP_CHECKOUT);
	DCL_CONST(long, "METH_CHECKIN", HTTP_CHECKIN);
	DCL_CONST(long, "METH_UNCHECKOUT", HTTP_UNCHECKOUT);
	DCL_CONST(long, "METH_MKWORKSPACE", HTTP_MKWORKSPACE);
	DCL_CONST(long, "METH_UPDATE", HTTP_UPDATE);
	DCL_CONST(long, "METH_LABEL", HTTP_LABEL);
	DCL_CONST(long, "METH_MERGE", HTTP_MERGE);
	DCL_CONST(long, "METH_BASELINE_CONTROL", HTTP_BASELINE_CONTROL);
	DCL_CONST(long, "METH_MKACTIVITY", HTTP_MKACTIVITY);
	/* WebDAV Access Control - RFC 3744 */
	DCL_CONST(long, "METH_ACL", HTTP_ACL);

	/* cURL HTTP protocol versions */
	DCL_CONST(long, "VERSION_1_0", CURL_HTTP_VERSION_1_0);
	DCL_CONST(long, "VERSION_1_1", CURL_HTTP_VERSION_1_1);
	DCL_CONST(long, "VERSION_NONE", CURL_HTTP_VERSION_NONE);

	/*
	 * Auth Constants
	 */
	DCL_CONST(long, "AUTH_BASIC", CURLAUTH_BASIC);
	DCL_CONST(long, "AUTH_DIGEST", CURLAUTH_DIGEST);
	DCL_CONST(long, "AUTH_NTLM", CURLAUTH_NTLM);
	DCL_CONST(long, "AUTH_ANY", CURLAUTH_ANY);
#endif /* WONKY */
}

void _http_request_object_free(zend_object *object TSRMLS_DC)
{
	http_request_object *o = (http_request_object *) object;

	if (OBJ_PROP(o)) {
		zend_hash_destroy(OBJ_PROP(o));
		FREE_HASHTABLE(OBJ_PROP(o));
	}
	http_request_free(&o->request);
	efree(o);
}

STATUS _http_request_object_requesthandler(http_request_object *obj, zval *this_ptr TSRMLS_DC)
{
	STATUS status = SUCCESS;

	http_request_reset(obj->request);
	HTTP_CHECK_CURL_INIT(obj->request->ch, http_curl_init(obj->request), return FAILURE);
	
	obj->request->url = http_absolute_url(Z_STRVAL_P(GET_PROP(url)));
	
	switch (obj->request->meth = Z_LVAL_P(GET_PROP(method)))
	{
		case HTTP_GET:
		case HTTP_HEAD:
		break;

		case HTTP_PUT:
		{
			php_stream_statbuf ssb;
			php_stream *stream = php_stream_open_wrapper(Z_STRVAL_P(GET_PROP(putFile)), "rb", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL);
			
			if (stream && !php_stream_stat(stream, &ssb)) {
				obj->request->body = http_request_body_init_ex(obj->request->body, HTTP_REQUEST_BODY_UPLOADFILE, stream, ssb.sb.st_size, 1);
			} else {
				status = FAILURE;
			}
		}
		break;

		case HTTP_POST:
		default:
		{
			/* check for raw post data */
			zval *raw_data = GET_PROP(rawPostData);
			
			if (Z_STRLEN_P(raw_data)) {
				zval *ctype = GET_PROP(contentType);
				
				if (Z_STRLEN_P(ctype)) {
					zval **headers, *opts = GET_PROP(options);
					
					if (	(Z_TYPE_P(opts) == IS_ARRAY) &&
							(SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), "headers", sizeof("headers"), (void **) &headers)) && 
							(Z_TYPE_PP(headers) == IS_ARRAY)) {
						zval **ct_header;
						
						/* only override if not already set */
						if ((SUCCESS != zend_hash_find(Z_ARRVAL_PP(headers), "Content-Type", sizeof("Content-Type"), (void **) &ct_header)) && (Z_TYPE_PP(ct_header) == IS_STRING)) {
							add_assoc_stringl(*headers, "Content-Type", Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
						}
					} else {
						zval *headers;
						
						MAKE_STD_ZVAL(headers);
						array_init(headers);
						add_assoc_stringl(headers, "Content-Type", Z_STRVAL_P(ctype), Z_STRLEN_P(ctype), 1);
						zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addheaders", NULL, headers);
						zval_ptr_dtor(&headers);
					}
				}

				obj->request->body = http_request_body_init_ex(obj->request->body, HTTP_REQUEST_BODY_CSTRING,
					estrndup(Z_STRVAL_P(raw_data), Z_STRLEN_P(raw_data)), Z_STRLEN_P(raw_data), 1);
				
			} else {
				zval *zfields = GET_PROP(postFields), *zfiles = GET_PROP(postFiles);
				HashTable *fields;
				HashTable *files;
				
				fields = (Z_TYPE_P(zfields) == IS_ARRAY) ? Z_ARRVAL_P(zfields) : NULL;
				files = (Z_TYPE_P(zfiles) == IS_ARRAY) ? Z_ARRVAL_P(zfiles) : NULL;
				
				if ((fields && zend_hash_num_elements(fields)) || (files && zend_hash_num_elements(files))) {
					if (!(obj->request->body = http_request_body_fill(obj->request->body, fields, files))) {
						status = FAILURE;
					}
				}
			}
		}
		break;
	}

	if (status == SUCCESS) {
		zval *qdata = GET_PROP(queryData);
		zval *options = GET_PROP(options);
		
		if (Z_STRLEN_P(qdata)) {
			if (!strchr(obj->request->url, '?')) {
				strlcat(obj->request->url, "?", HTTP_URL_MAXLEN);
			} else {
				strlcat(obj->request->url, "&", HTTP_URL_MAXLEN);
			}
			strlcat(obj->request->url, Z_STRVAL_P(qdata), HTTP_URL_MAXLEN);
		}
		
		http_request_prepare(obj->request, Z_ARRVAL_P(options));
		
		/* check if there's a onProgress method and add it as progress callback if one isn't already set */
		if (zend_hash_exists(&Z_OBJCE_P(getThis())->function_table, "onprogress", sizeof("onprogress"))) {
			zval **entry, *pcb;
			
			if (	(Z_TYPE_P(options) != IS_ARRAY)
				||	(SUCCESS != zend_hash_find(Z_ARRVAL_P(options), "onprogress", sizeof("onprogress"), (void **) &entry)
				||	(!zval_is_true(*entry)))) {
				MAKE_STD_ZVAL(pcb);
				array_init(pcb);
				ZVAL_ADDREF(getThis());
				add_next_index_zval(pcb, getThis());
				add_next_index_stringl(pcb, "onprogress", lenof("onprogress"), 1);
				http_request_set_progress_callback(obj->request, pcb);
				zval_ptr_dtor(&pcb);
			}
		}
	}

	return status;
}

STATUS _http_request_object_responsehandler(http_request_object *obj, zval *this_ptr TSRMLS_DC)
{
	http_message *msg;
	
	phpstr_fix(&obj->request->conv.request);
	phpstr_fix(&obj->request->conv.response);
	
	msg = http_message_parse(PHPSTR_VAL(&obj->request->conv.response), PHPSTR_LEN(&obj->request->conv.response));
	
	if (!msg) {
		return FAILURE;
	} else {
		char *body;
		size_t body_len;
		zval *headers, *message, *resp, *info;

		if (zval_is_true(GET_PROP(recordHistory))) {
			/* we need to act like a zipper, as we'll receive
			 * the requests and the responses in separate chains
			 * for redirects
			 */
			http_message *response = http_message_dup(msg);
			http_message *request = http_message_parse(PHPSTR_VAL(&obj->request->conv.request), PHPSTR_LEN(&obj->request->conv.request));
			
			if (request && response) {
				int num_req, num_resp;
				
				http_message_count(num_req, request);
				http_message_count(num_resp, response);
				
				/*
					stuck request messages in between response messages
				
					response   request
					   v          v
					response   request
					   v          v
					response   request
					==================
					response > request
				           ,---'
					response > request
				           ,---'
					response > request
				*/
				if (num_req == num_resp) {
					int i;
					zval *hist, *history = GET_PROP(history);
					http_message *res_tmp = response, *req_tmp = request, *req_par, *res_par;
					
					for (i = 0; i < num_req; ++i) {
						res_par = res_tmp->parent;
						req_par = req_tmp->parent;
						res_tmp->parent = req_tmp;
						req_tmp->parent = res_par;
						res_tmp = res_par;
						req_tmp = req_par;
					}
					
					MAKE_STD_ZVAL(hist);
					ZVAL_OBJVAL(hist, http_message_object_new_ex(http_message_object_ce, response, NULL), 0);
					if (Z_TYPE_P(history) == IS_OBJECT) {
						http_message_object_prepend(hist, history);
					}
					SET_PROP(history, hist);
					zval_ptr_dtor(&hist);
				}
				/* TODO: error? */
			} else {
				http_message_free(&response);
				http_message_free(&request);
			}
		}

		UPD_PROP(long, responseCode, msg->http.info.response.code);
		UPD_PROP(string, responseStatus, msg->http.info.response.status);

		MAKE_STD_ZVAL(resp);
		array_init(resp);
		MAKE_STD_ZVAL(headers);
		array_init(headers);
		zend_hash_copy(Z_ARRVAL_P(headers), &msg->hdrs, (copy_ctor_func_t) zval_add_ref, NULL, sizeof(zval *));
		add_assoc_zval(resp, "headers", headers);
		phpstr_data(PHPSTR(msg), &body, &body_len);
		add_assoc_stringl(resp, "body", body, body_len, 0);
		SET_PROP(responseData, resp);
		zval_ptr_dtor(&resp);

		MAKE_STD_ZVAL(message);
		ZVAL_OBJVAL(message, http_message_object_new_ex(http_message_object_ce, msg, NULL), 0);
		SET_PROP(responseMessage, message);
		zval_ptr_dtor(&message);

		MAKE_STD_ZVAL(info);
		array_init(info);
		http_request_info(obj->request, Z_ARRVAL_P(info));
		SET_PROP(responseInfo, info);
		zval_ptr_dtor(&info);
		
		if (zend_hash_exists(&Z_OBJCE_P(getThis())->function_table, "onfinish", sizeof("onfinish"))) {
			zend_call_method_with_0_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "onfinish", NULL);
		}
		
		return SUCCESS;
	}
}

#define http_request_object_set_options_subr(key, ow) \
	_http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, (key), sizeof(key), (ow))
static inline void _http_request_object_set_options_subr(INTERNAL_FUNCTION_PARAMETERS, char *key, size_t len, int overwrite)
{
	zval *old_opts, *new_opts, *opts = NULL, **entry;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a/!", &opts)) {
		RETURN_FALSE;
	}

	MAKE_STD_ZVAL(new_opts);
	array_init(new_opts);
	old_opts = GET_PROP(options);
	if (Z_TYPE_P(old_opts) == IS_ARRAY) {
		array_copy(old_opts, new_opts);
	}

	if (SUCCESS == zend_hash_find(Z_ARRVAL_P(new_opts), key, len, (void **) &entry)) {
		if (overwrite) {
			zend_hash_clean(Z_ARRVAL_PP(entry));
		}
		if (opts && zend_hash_num_elements(Z_ARRVAL_P(opts))) {
			if (overwrite) {
				array_copy(opts, *entry);
			} else {
				array_merge(opts, *entry);
			}
		}
	} else if (opts) {
		ZVAL_ADDREF(opts);
		add_assoc_zval(new_opts, key, opts);
	}
	SET_PROP(options, new_opts);
	zval_ptr_dtor(&new_opts);

	RETURN_TRUE;
}

#define http_request_object_get_options_subr(key) \
	_http_request_get_options_subr(INTERNAL_FUNCTION_PARAM_PASSTHRU, (key), sizeof(key))
static inline void _http_request_get_options_subr(INTERNAL_FUNCTION_PARAMETERS, char *key, size_t len)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *opts, **options;

		opts = GET_PROP(options);
		array_init(return_value);

		if (	(Z_TYPE_P(opts) == IS_ARRAY) && 
				(SUCCESS == zend_hash_find(Z_ARRVAL_P(opts), key, len, (void **) &options))) {
			convert_to_array(*options);
			array_copy(*options, return_value);
		}
	}
}


/* ### USERLAND ### */

/* {{{ proto void HttpRequest::__construct([string url[, int request_method = HTTP_METH_GET[, array options]]])
 *
 * Instantiate a new HttpRequest object.
 * 
 * Accepts a string as optional parameter containing the target request url.
 * Additionally accepts an optional int parameter specifying the request method
 * to use and an associative array as optional third parameter which will be
 * passed to HttpRequest::setOptions(). 
 * 
 * Throws HttpException.
 */
PHP_METHOD(HttpRequest, __construct)
{
	char *URL = NULL;
	int URL_len;
	long meth = -1;
	zval *options = NULL;

	SET_EH_THROW_HTTP();
	if (SUCCESS == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|sla", &URL, &URL_len, &meth, &options)) {
		if (URL) {
			UPD_STRL(url, URL, URL_len);
		}
		if (meth > -1) {
			UPD_PROP(long, method, meth);
		}
		if (options) {
			zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "setoptions", NULL, options);
		}
	}
	SET_EH_NORMAL();
}
/* }}} */

/* {{{ proto bool HttpRequest::setOptions([array options])
 *
 * Set the request options to use.  See http_get() for a full list of available options.
 * 
 * Accepts an array as optional parameters, which values will overwrite the 
 * currently set request options.  If the parameter is empty or omitted,
 * the options of the HttpRequest object will be reset.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, setOptions)
{
	char *key = NULL;
	ulong idx = 0;
	HashPosition pos;
	zval *opts = NULL, *old_opts, *new_opts, *add_opts, **opt;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a!/", &opts)) {
		RETURN_FALSE;
	}
	
	MAKE_STD_ZVAL(new_opts);
	array_init(new_opts);
		
	if (!opts || !zend_hash_num_elements(Z_ARRVAL_P(opts))) {
		SET_PROP(options, new_opts);
		zval_ptr_dtor(&new_opts);
		RETURN_TRUE;
	}
	
	MAKE_STD_ZVAL(add_opts);
	array_init(add_opts);
	/* some options need extra attention -- thus cannot use array_merge() directly */
	FOREACH_KEYVAL(pos, opts, key, idx, opt) {
		if (key) {
			if (!strcmp(key, "headers")) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addheaders", NULL, *opt);
			} else if (!strcmp(key, "cookies")) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addcookies", NULL, *opt);
			} else if (!strcmp(key, "ssl")) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "addssloptions", NULL, *opt);
			} else if ((!strcasecmp(key, "url")) || (!strcasecmp(key, "uri"))) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "seturl", NULL, *opt);
			} else if (!strcmp(key, "method")) {
				zend_call_method_with_1_params(&getThis(), Z_OBJCE_P(getThis()), NULL, "setmethod", NULL, *opt);
			} else {
				ZVAL_ADDREF(*opt);
				add_assoc_zval(add_opts, key, *opt);
			}
			/* reset */
			key = NULL;
		}
	}
	
	old_opts = GET_PROP(options);
	if (Z_TYPE_P(old_opts) == IS_ARRAY) {
		array_copy(old_opts, new_opts);
	}
	array_merge(add_opts, new_opts);
	SET_PROP(options, new_opts);
	zval_ptr_dtor(&new_opts);
	zval_ptr_dtor(&add_opts);
	
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HttpRequest::getOptions()
 *
 * Get currently set options.
 * 
 * Returns an associative array containing currently set options.
 */
PHP_METHOD(HttpRequest, getOptions)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_PROP(options);
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::setSslOptions([array options])
 *
 * Set SSL options.
 * 
 * Accepts an associative array as parameter containing any SSL specific options.
 * If the parameter is empty or omitted, the SSL options will be reset.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, setSslOptions)
{
	http_request_object_set_options_subr("ssl", 1);
}
/* }}} */

/* {{{ proto bool HttpRequest::addSslOptions(array options)
 *
 * Set additional SSL options.
 * 
 * Expects an associative array as parameter containing additional SSL specific options.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, addSslOptions)
{
	http_request_object_set_options_subr("ssl", 0);
}
/* }}} */

/* {{{ proto array HttpRequest::getSslOtpions()
 *
 * Get previously set SSL options.
 * 
 * Returns an associative array containing any previously set SSL options.
 */
PHP_METHOD(HttpRequest, getSslOptions)
{
	http_request_object_get_options_subr("ssl");
}
/* }}} */

/* {{{ proto bool HttpRequest::addHeaders(array headers)
 *
 * Add request header name/value pairs.
 * 
 * Expects an associative array as parameter containing additional header
 * name/value pairs.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, addHeaders)
{
	http_request_object_set_options_subr("headers", 0);
}

/* {{{ proto bool HttpRequest::setHeaders([array headers])
 *
 * Set request header name/value pairs.
 * 
 * Accepts an associative array as parameter containing header name/value pairs.
 * If the parameter is empty or omitted, all previously set headers will be unset.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, setHeaders)
{
	http_request_object_set_options_subr("headers", 1);
}
/* }}} */

/* {{{ proto array HttpRequest::getHeaders()
 *
 * Get previously set request headers.
 * 
 * Returns an associative array containing all currently set headers.
 */
PHP_METHOD(HttpRequest, getHeaders)
{
	http_request_object_get_options_subr("headers");
}
/* }}} */

/* {{{ proto bool HttpRequest::setCookies([array cookies])
 *
 * Set cookies.
 * 
 * Accepts an associative array as parameter containing cookie name/value pairs.
 * If the parameter is empty or omitted, all previously set cookies will be unset.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, setCookies)
{
	http_request_object_set_options_subr("cookies", 1);
}
/* }}} */

/* {{{ proto bool HttpRequest::addCookies(array cookies)
 *
 * Add cookies.
 * 
 * Expects an associative array as parameter containing any cookie name/value
 * pairs to add.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, addCookies)
{
	http_request_object_set_options_subr("cookies", 0);
}
/* }}} */

/* {{{ proto array HttpRequest::getCookies()
 *
 * Get previously set cookies.
 * 
 * Returns an associative array containing any previously set cookies.
 */
PHP_METHOD(HttpRequest, getCookies)
{
	http_request_object_get_options_subr("cookies");
}
/* }}} */

/* {{{ proto bool HttpRequest::setUrl(string url)
 *
 * Set the request URL.
 * 
 * Expects a string as parameter specifying the request url.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, setUrl)
{
	char *URL = NULL;
	int URL_len;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &URL, &URL_len)) {
		RETURN_FALSE;
	}

	UPD_STRL(url, URL, URL_len);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpRequest::getUrl()
 *
 * Get the previously set request URL.
 * 
 * Returns the currently set request url as string.
 */
PHP_METHOD(HttpRequest, getUrl)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_PROP(url);
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::setMethod(int request_method)
 *
 * Set the request method.
 * 
 * Expects an int as parameter specifying the request method to use.
 * In PHP 5.1+ HttpRequest::METH_*, otherwise the HTTP_METH_* constants can be used.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, setMethod)
{
	long meth;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &meth)) {
		RETURN_FALSE;
	}

	UPD_PROP(long, method, meth);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int HttpRequest::getMethod()
 *
 * Get the previously set request method.
 * 
 * Returns the currently set request method.
 */
PHP_METHOD(HttpRequest, getMethod)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_PROP(method);
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::setContentType(string content_type)
 *
 * Set the content type the post request should have.
 * 
 * Expects a string as parameters containing the content type of the request
 * (primary/secondary).
 * 
 * Returns TRUE on success, or FALSE if the content type does not seem to
 * contain a primary and a secondary part.
 */
PHP_METHOD(HttpRequest, setContentType)
{
	char *ctype;
	int ct_len;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &ctype, &ct_len)) {
		RETURN_FALSE;
	}

	HTTP_CHECK_CONTENT_TYPE(ctype, RETURN_FALSE);
	UPD_STRL(contentType, ctype, ct_len);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpRequest::getContentType()
 *
 * Get the previously content type.
 * 
 * Returns the previously set content type as string.
 */
PHP_METHOD(HttpRequest, getContentType)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_PROP(contentType);
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::setQueryData([mixed query_data])
 *
 * Set the URL query parameters to use, overwriting previously set query parameters.
 * Affects any request types.
 * 
 * Accepts a string or associative array parameter containing the pre-encoded 
 * query string or to be encoded query fields.  If the parameter is empty or
 * omitted, the query data will be unset. 
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, setQueryData)
{
	zval *qdata = NULL;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z!", &qdata)) {
		RETURN_FALSE;
	}

	if ((!qdata) || Z_TYPE_P(qdata) == IS_NULL) {
		UPD_STRL(queryData, "", 0);
	} else if ((Z_TYPE_P(qdata) == IS_ARRAY) || (Z_TYPE_P(qdata) == IS_OBJECT)) {
		char *query_data = NULL;
		
		if (SUCCESS != http_urlencode_hash(HASH_OF(qdata), &query_data)) {
			RETURN_FALSE;
		}
		
		UPD_PROP(string, queryData, query_data);
		efree(query_data);
	} else {
		zval *orig = qdata;
		
		convert_to_string_ex(&qdata);
		UPD_STRL(queryData, Z_STRVAL_P(qdata), Z_STRLEN_P(qdata));
		if (orig != qdata) {
			zval_ptr_dtor(&qdata);
		}
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpRequest::getQueryData()
 *
 * Get the current query data in form of an urlencoded query string.
 * 
 * Returns a string containing the urlencoded query.
 */
PHP_METHOD(HttpRequest, getQueryData)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_PROP(queryData);
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::addQueryData(array query_params)
 *
 * Add parameters to the query parameter list, leaving previously set unchanged.
 * Affects any request type.
 * 
 * Expects an associative array as parameter containing the query fields to add.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, addQueryData)
{
	zval *qdata, *old_qdata;
	char *query_data = NULL;
	size_t query_data_len = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &qdata)) {
		RETURN_FALSE;
	}

	old_qdata = GET_PROP(queryData);

	if (SUCCESS != http_urlencode_hash_ex(HASH_OF(qdata), 1, Z_STRVAL_P(old_qdata), Z_STRLEN_P(old_qdata), &query_data, &query_data_len)) {
		RETURN_FALSE;
	}

	UPD_STRL(queryData, query_data, query_data_len);
	efree(query_data);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HttpRequest::addPostFields(array post_data)
 *
 * Adds POST data entries, leaving previously set unchanged, unless a
 * post entry with the same name already exists. 
 * Affects only POST and custom requests.
 * 
 * Expects an associative array as parameter containing the post fields.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, addPostFields)
{
	zval *post_data, *old_post, *new_post;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/", &post_data)) {
		RETURN_FALSE;
	}

	if (zend_hash_num_elements(Z_ARRVAL_P(post_data))) {
		MAKE_STD_ZVAL(new_post);
		array_init(new_post);
		old_post = GET_PROP(postFields);
		if (Z_TYPE_P(old_post) == IS_ARRAY) {
			array_copy(old_post, new_post);
		}
		array_merge(post_data, new_post);
		SET_PROP(postFields, new_post);
		zval_ptr_dtor(&new_post);
	}
	
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HttpRequest::setPostFields([array post_data])
 *
 * Set the POST data entries, overwriting previously set POST data.
 * Affects only POST and custom requests.
 * 
 * Accepts an associative array as parameter containing the post fields.
 * If the parameter is empty or omitted, the post data will be unset.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, setPostFields)
{
	zval *post, *post_data = NULL;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a/!", &post_data)) {
		RETURN_FALSE;
	}

	MAKE_STD_ZVAL(post);
	array_init(post);
	if (post_data && zend_hash_num_elements(Z_ARRVAL_P(post_data))) {
		array_copy(post_data, post);
	}
	SET_PROP(postFields, post);
	zval_ptr_dtor(&post);

	RETURN_TRUE;
}
/* }}}*/

/* {{{ proto array HttpRequest::getPostFields()
 *
 * Get previously set POST data.
 * 
 * Returns the currently set post fields as associative array.
 */
PHP_METHOD(HttpRequest, getPostFields)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_PROP(postFields);
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::setRawPostData([string raw_post_data])
 *
 * Set raw post data to send, overwriting previously set raw post data.  Don't 
 * forget to specify a content type. Affects only POST and custom requests.
 * Only either post fields or raw post data can be used for each request.
 * Raw post data has higher precedence and will be used even if post fields
 * are set.  
 * 
 * Accepts a string as parameter containing the *raw* post data.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, setRawPostData)
{
	char *raw_data = NULL;
	int data_len = 0;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &raw_data, &data_len)) {
		RETURN_FALSE;
	}
	
	if (!raw_data) {
		raw_data = "";
	}
	
	UPD_STRL(rawPostData, raw_data, data_len);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HttpRequest::addRawPostData(string raw_post_data)
 *
 * Add raw post data, leaving previously set raw post data unchanged.
 * Affects only POST and custom requests.
 * 
 * Expects a string as parameter containing the raw post data to concatenate.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, addRawPostData)
{
	char *raw_data;
	int data_len;
	
	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &raw_data, &data_len)) {
		RETURN_FALSE;
	}
	
	if (data_len) {
		zval *data = zval_copy(IS_STRING, GET_PROP(rawPostData));
		
		Z_STRVAL_P(data) = erealloc(Z_STRVAL_P(data), (Z_STRLEN_P(data) += data_len) + 1);
		Z_STRVAL_P(data)[Z_STRLEN_P(data)] = '\0';
		memcpy(Z_STRVAL_P(data) + Z_STRLEN_P(data) - data_len, raw_data, data_len);
		SET_PROP(rawPostData, data);
		zval_free(&data);
	}
	
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpRequest::getRawPostData()
 *
 * Get previously set raw post data.
 * 
 * Returns a string containing the currently set raw post data.
 */
PHP_METHOD(HttpRequest, getRawPostData)
{
	NO_ARGS;
	
	IF_RETVAL_USED {
		RETURN_PROP(rawPostData);
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::addPostFile(string name, string file[, string content_type = "application/x-octetstream"])
 *
 * Add a file to the POST request, leaving previously set files unchanged.
 * Affects only POST and custom requests. Cannot be used with raw post data.
 * 
 * Expects a string parameter containing the form element name, and a string
 * paremeter containing the path to the file which should be uploaded.
 * Additionally accepts an optional string parameter which should contain
 * the content type of the file.
 * 
 * Returns TRUE on success, or FALSE if the content type seems not to contain a 
 * primary and a secondary content type part.
 */
PHP_METHOD(HttpRequest, addPostFile)
{
	zval *entry, *old_post, *new_post;
	char *name, *file, *type = NULL;
	int name_len, file_len, type_len = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|s", &name, &name_len, &file, &file_len, &type, &type_len)) {
		RETURN_FALSE;
	}

	if (type_len) {
		HTTP_CHECK_CONTENT_TYPE(type, RETURN_FALSE);
	} else {
		type = "application/x-octetstream";
		type_len = sizeof("application/x-octetstream") - 1;
	}

	MAKE_STD_ZVAL(entry);
	array_init(entry);

	add_assoc_stringl(entry, "name", name, name_len, 1);
	add_assoc_stringl(entry, "type", type, type_len, 1);
	add_assoc_stringl(entry, "file", file, file_len, 1);

	MAKE_STD_ZVAL(new_post);
	array_init(new_post);
	old_post = GET_PROP(postFiles);
	if (Z_TYPE_P(old_post) == IS_ARRAY) {
		array_copy(old_post, new_post);
	}
	add_next_index_zval(new_post, entry);
	SET_PROP(postFiles, new_post);
	zval_ptr_dtor(&new_post);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool HttpRequest::setPostFiles([array post_files])
 *
 * Set files to post, overwriting previously set post files.
 * Affects only POST and requests. Cannot be used with raw post data.
 * 
 * Accepts an array containing the files to post.  Each entry should be an
 * associative array with "name", "file" and "type" keys.  If the parameter
 * is empty or omitted the post files will be unset.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, setPostFiles)
{
	zval *files = NULL, *post;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a!/", &files)) {
		RETURN_FALSE;
	}

	MAKE_STD_ZVAL(post);
	array_init(post);
	if (files && (Z_TYPE_P(files) == IS_ARRAY)) {
		array_copy(files, post);
	}
	SET_PROP(postFiles, post);
	zval_ptr_dtor(&post);

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array HttpRequest::getPostFiles()
 *
 * Get all previously added POST files.
 * 
 * Returns an array containing currently set post files.
 */
PHP_METHOD(HttpRequest, getPostFiles)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_PROP(postFiles);
	}
}
/* }}} */

/* {{{ proto bool HttpRequest::setPutFile([string file])
 *
 * Set file to put. Affects only PUT requests.
 * 
 * Accepts a string as parameter referencing the path to file.
 * If the parameter is empty or omitted the put file will be unset.
 * 
 * Returns TRUE on success, or FALSE on failure.
 */
PHP_METHOD(HttpRequest, setPutFile)
{
	char *file = "";
	int file_len = 0;

	if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &file, &file_len)) {
		RETURN_FALSE;
	}

	UPD_STRL(putFile, file, file_len);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto string HttpRequest::getPutFile()
 *
 * Get previously set put file.
 * 
 * Returns a string containing the path to the currently set put file.
 */
PHP_METHOD(HttpRequest, getPutFile)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_PROP(putFile);
	}
}
/* }}} */

/* {{{ proto array HttpRequest::getResponseData()
 *
 * Get all response data after the request has been sent.
 * 
 * Returns an associative array with the key "headers" containing an associative
 * array holding all response headers, as well as the key "body" containing a
 * string with the response body.  
 * 
 * If redirects were allowed and several responses were received, the data 
 * references the last received response.
 */
PHP_METHOD(HttpRequest, getResponseData)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_PROP(responseData);
	}
}
/* }}} */

/* {{{ proto mixed HttpRequest::getResponseHeader([string name])
 *
 * Get response header(s) after the request has been sent.
 * 
 * Accepts an string as optional parameter specifying a certain header to read.
 * If the parameter is empty or omitted all response headers will be returned.
 * 
 * Returns either a string with the value of the header matching name if requested, 
 * FALSE on failure, or an associative array containing all response headers.
 * 
 * If redirects were allowed and several responses were received, the data 
 * references the last received response.
 */
PHP_METHOD(HttpRequest, getResponseHeader)
{
	IF_RETVAL_USED {
		zval *data, **headers, **header;
		char *header_name = NULL;
		int header_len = 0;

		if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &header_name, &header_len)) {
			RETURN_FALSE;
		}

		data = GET_PROP(responseData);
		if (	(Z_TYPE_P(data) == IS_ARRAY) && 
				(SUCCESS == zend_hash_find(Z_ARRVAL_P(data), "headers", sizeof("headers"), (void **) &headers)) &&
				(Z_TYPE_PP(headers) == IS_ARRAY)) {
			if (!header_len || !header_name) {
				RETVAL_ZVAL(*headers, 1, 0);
			} else if (SUCCESS == zend_hash_find(Z_ARRVAL_PP(headers), pretty_key(header_name, header_len, 1, 1), header_len + 1, (void **) &header)) {
				RETVAL_ZVAL(*header, 1, 0);
			} else {
				RETVAL_FALSE;
			}
		} else {
			RETVAL_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto array HttpRequest::getResponseCookies([int flags[, array allowed_extras]])
 *
 * Get response cookie(s) after the request has been sent.
 * 
 * Returns an array of stdClass objects like http_parse_cookie would return.
 * 
 * If redirects were allowed and several responses were received, the data 
 * references the last received response.
 */
PHP_METHOD(HttpRequest, getResponseCookies)
{
	IF_RETVAL_USED {
		long flags = 0;
		zval *allowed_extras_array = NULL, *data, **headers;

		if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|la", &flags, &allowed_extras_array)) {
			RETURN_FALSE;
		}

		data = GET_PROP(responseData);
		if (	(Z_TYPE_P(data) == IS_ARRAY) &&
				(SUCCESS == zend_hash_find(Z_ARRVAL_P(data), "headers", sizeof("headers"), (void **) &headers)) &&
				(Z_TYPE_PP(headers) == IS_ARRAY)) {
			int i = 0;
			ulong idx = 0;
			char *key = NULL, **allowed_extras = NULL;
			zval **header = NULL, **entry = NULL;
			HashPosition pos, pos1, pos2;
			
			array_init(return_value);

			if (allowed_extras_array) {
				allowed_extras = ecalloc(zend_hash_num_elements(Z_ARRVAL_P(allowed_extras_array)) + 1, sizeof(char *));
				FOREACH_VAL(pos, allowed_extras_array, entry) {
					ZVAL_ADDREF(*entry);
					convert_to_string_ex(entry);
					allowed_extras[i++] = estrndup(Z_STRVAL_PP(entry), Z_STRLEN_PP(entry));
					zval_ptr_dtor(entry);
				}
			}
			
			FOREACH_HASH_KEYVAL(pos1, Z_ARRVAL_PP(headers), key, idx, header) {
				if (key && !strcasecmp(key, "Set-Cookie")) {
					http_cookie_list list;
					
					if (Z_TYPE_PP(header) == IS_ARRAY) {
						zval **single_header;
						HashPosition pos;
						
						FOREACH_VAL(pos2, *header, single_header) {
							ZVAL_ADDREF(*single_header);
							convert_to_string_ex(single_header);
							if (http_parse_cookie_ex(&list, Z_STRVAL_PP(single_header), flags, allowed_extras)) {
								zval *cookie;
								
								MAKE_STD_ZVAL(cookie);
								object_init(cookie);
								http_cookie_list_tostruct(&list, cookie);
								add_next_index_zval(return_value, cookie);
								http_cookie_list_dtor(&list);
							}
							zval_ptr_dtor(single_header);
						}
					} else {
						ZVAL_ADDREF(*header);
						convert_to_string_ex(header);
						if (http_parse_cookie_ex(&list, Z_STRVAL_PP(header), flags, allowed_extras)) {
							zval *cookie;
								
							MAKE_STD_ZVAL(cookie);
							object_init(cookie);
							http_cookie_list_tostruct(&list, cookie);
							add_next_index_zval(return_value, cookie);
							http_cookie_list_dtor(&list);
						}
						zval_ptr_dtor(header);
					}
				}
				/* reset key */
				key = NULL;
			}
	
			if (allowed_extras) {
				for (i = 0; allowed_extras[i]; ++i) {
					efree(allowed_extras[i]);
				}
				efree(allowed_extras);
			}
		} else {
			RETURN_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto string HttpRequest::getResponseBody()
 *
 * Get the response body after the request has been sent.
 * 
 * Returns a string containing the response body.
 * 
 * If redirects were allowed and several responses were received, the data 
 * references the last received response.
 */
PHP_METHOD(HttpRequest, getResponseBody)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval **body;
		zval *data = GET_PROP(responseData);
		
		if (	(Z_TYPE_P(data) == IS_ARRAY) && 
				(SUCCESS == zend_hash_find(Z_ARRVAL_P(data), "body", sizeof("body"), (void **) &body))) {
			RETURN_ZVAL(*body, 1, 0);
		} else {
			RETURN_FALSE;
		}
	}
}
/* }}} */

/* {{{ proto int HttpRequest::getResponseCode()
 *
 * Get the response code after the request has been sent.
 * 
 * Returns an int representing the response code.
 * 
 * If redirects were allowed and several responses were received, the data 
 * references the last received response.
 */
PHP_METHOD(HttpRequest, getResponseCode)
{
	NO_ARGS;

	IF_RETVAL_USED {
		RETURN_PROP(responseCode);
	}
}
/* }}} */

/* {{{ proto string HttpRequest::getResponseStatus()
 *
 * Get the response status (i.e. the string after the response code) after the message has been sent.
 *
 * Returns a string containing the response status text.
 */
PHP_METHOD(HttpRequest, getResponseStatus)
{
	NO_ARGS;
	
	IF_RETVAL_USED {
		RETURN_PROP(responseStatus);
	}
}
/* }}} */

/* {{{ proto mixed HttpRequest::getResponseInfo([string name])
 *
 * Get response info after the request has been sent.
 * See http_get() for a full list of returned info.
 * 
 * Accepts a string as optional parameter specifying the info to read.
 * If the parameter is empty or omitted, an associative array containing
 * all available info will be returned.
 * 
 * Returns either a scalar containing the value of the info matching name if
 * requested, FALSE on failure, or an associative array containing all
 * available info.
 * 
 * If redirects were allowed and several responses were received, the data 
 * references the last received response.
 */
PHP_METHOD(HttpRequest, getResponseInfo)
{
	IF_RETVAL_USED {
		zval *info, **infop;
		char *info_name = NULL;
		int info_len = 0;

		if (SUCCESS != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &info_name, &info_len)) {
			RETURN_FALSE;
		}

		info = GET_PROP(responseInfo);
		
		if (Z_TYPE_P(info) != IS_ARRAY) {
			RETURN_FALSE;
		}

		if (info_len && info_name) {
			if (SUCCESS == zend_hash_find(Z_ARRVAL_P(info), pretty_key(info_name, info_len, 0, 0), info_len + 1, (void **) &infop)) {
				RETURN_ZVAL(*infop, 1, 0);
			} else {
				http_error_ex(HE_NOTICE, HTTP_E_INVALID_PARAM, "Could not find response info named %s", info_name);
				RETURN_FALSE;
			}
		} else {
			RETURN_ZVAL(info, 1, 0);
		}
	}
}
/* }}}*/

/* {{{ proto HttpMessage HttpRequest::getResponseMessage()
 *
 * Get the full response as HttpMessage object after the request has been sent.
 * 
 * Returns an HttpMessage object of the response.
 * 
 * If redirects were allowed and several responses were received, the data 
 * references the last received response.  Use HttpMessage::getParentMessage()
 * to access the data of previously received responses within this request
 * cycle.
 * 
 * Throws HttpException.
 */
PHP_METHOD(HttpRequest, getResponseMessage)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *message;

		SET_EH_THROW_HTTP();
		message = GET_PROP(responseMessage);
		if (Z_TYPE_P(message) == IS_OBJECT) {
			RETVAL_OBJECT(message, 1);
		} else {
			RETVAL_NULL();
		}
		SET_EH_NORMAL();
	}
}
/* }}} */

/* {{{ proto HttpMessage HttpRequest::getRequestMessage()
 *
 * Get sent HTTP message.
 * 
 * Returns an HttpMessage object representing the sent request.
 * 
 * If redirects were allowed and several responses were received, the data 
 * references the last received response.  Use HttpMessage::getParentMessage()
 * to access the data of previously sent requests within this request
 * cycle.
 * 
 * Note that the internal request message is immutable, that means that the
 * request message received through HttpRequest::getRequestMessage() will
 * always look the same for the same request, regardless of any changes you
 * may have made to the returned object.
 * 
 * Throws HttpMalformedHeadersException, HttpEncodingException.
 */
PHP_METHOD(HttpRequest, getRequestMessage)
{
	NO_ARGS;

	IF_RETVAL_USED {
		http_message *msg;
		getObject(http_request_object, obj);

		SET_EH_THROW_HTTP();
		if ((msg = http_message_parse(PHPSTR_VAL(&obj->request->conv.request), PHPSTR_LEN(&obj->request->conv.request)))) {
			RETVAL_OBJVAL(http_message_object_new_ex(http_message_object_ce, msg, NULL), 0);
		}
		SET_EH_NORMAL();
	}
}
/* }}} */

/* {{{ proto string HttpRequest::getRawRequestMessage()
 *
 * Get sent HTTP message.
 * 
 * Returns an HttpMessage in a form of a string 
 * 
 */
PHP_METHOD(HttpRequest, getRawRequestMessage)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_request_object, obj);

		RETURN_PHPSTR_DUP(&obj->request->conv.request);
	}
}
/* }}} */

/* {{{ proto string HttpRequest::getRawResponseMessage()
 *
 * Get the entire HTTP response.
 * 
 * Returns the complete web server response, including the headers in a form of a string.
 * 
 */
PHP_METHOD(HttpRequest, getRawResponseMessage)
{
	NO_ARGS;

	IF_RETVAL_USED {
		getObject(http_request_object, obj);

		RETURN_PHPSTR_DUP(&obj->request->conv.response);
	}
}
/* }}} */

/* {{{ proto HttpMessage HttpRequest::getHistory()
 *
 * Get all sent requests and received responses as an HttpMessage object.
 * 
 * If you want to record history, set the instance variable
 * HttpRequest::$recordHistory to TRUE.
 * 
 * Returns an HttpMessage object representing the complete request/response
 * history.
 * 
 * The object references the last received response, use HttpMessage::getParentMessage() 
 * to access the data of previously sent requests and received responses.
 * 
 * Throws HttpRuntimeException.
 */
PHP_METHOD(HttpRequest, getHistory)
{
	NO_ARGS;

	IF_RETVAL_USED {
		zval *hist;
		
		SET_EH_THROW_HTTP();
		hist = GET_PROP(history);
		if (Z_TYPE_P(hist) == IS_OBJECT) {
			RETVAL_OBJECT(hist, 1);
		} else {
			http_error(HE_WARNING, HTTP_E_RUNTIME, "The history is empty");
		}
		SET_EH_NORMAL();
	}
}
/* }}} */

/* {{{ proto void HttpRequest::clearHistory()
 *
 * Clear the history.
 */
PHP_METHOD(HttpRequest, clearHistory)
{
	NO_ARGS {
		zval *hist;
		
		MAKE_STD_ZVAL(hist);
		ZVAL_NULL(hist);
		SET_PROP(history, hist);
		zval_ptr_dtor(hist);
	}
}
/* }}} */

/* {{{ proto HttpMessage HttpRequest::send()
 *
 * Send the HTTP request.
 * 
 * Returns the received response as HttpMessage object.
 * 
 * NOTE: While an exception may be thrown, the transfer could have succeeded 
 * at least partially, so you might want to check the return values of various
 * HttpRequest::getResponse*() methods.
 * 
 * Throws HttpRuntimeException, HttpRequestException, 
 * HttpMalformedHeaderException, HttpEncodingException.
 *
 * GET example:
 * <pre>
 * <?php
 * $r = new HttpRequest('http://example.com/feed.rss', HttpRequest::METH_GET);
 * $r->setOptions(array('lastmodified' => filemtime('local.rss')));
 * $r->addQueryData(array('category' => 3));
 * try {
 *     $r->send();
 *     if ($r->getResponseCode() == 200) {
 *         file_put_contents('local.rss', $r->getResponseBody());
 *    }
 * } catch (HttpException $ex) {
 *     echo $ex;
 * }
 * ?>
 * </pre>
 *
 * POST example:
 * <pre>
 * <?php
 * $r = new HttpRequest('http://example.com/form.php', HttpRequest::METH_POST);
 * $r->setOptions(array('cookies' => array('lang' => 'de')));
 * $r->addPostFields(array('user' => 'mike', 'pass' => 's3c|r3t'));
 * $r->addPostFile('image', 'profile.jpg', 'image/jpeg');
 * try {
 *     echo $r->send()->getBody();
 * } catch (HttpException $ex) {
 *     echo $ex;
 * }
 * ?>
 * </pre>
 */
PHP_METHOD(HttpRequest, send)
{
	getObject(http_request_object, obj);

	NO_ARGS;

	SET_EH_THROW_HTTP();

	RETVAL_FALSE;
	
	if (obj->pool) {
		http_error(HE_WARNING, HTTP_E_RUNTIME, "Cannot perform HttpRequest::send() while attached to an HttpRequestPool");
	} else if (SUCCESS == http_request_object_requesthandler(obj, getThis())) {
		http_request_exec(obj->request);
		if (SUCCESS == http_request_object_responsehandler(obj, getThis())) {
			RETVAL_OBJECT(GET_PROP(responseMessage), 1);
		}
	}

	SET_EH_NORMAL();
}
/* }}} */

#endif /* ZEND_ENGINE_2 && HTTP_HAVE_CURL */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

