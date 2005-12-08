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

#define HTTP_WANT_CURL
#include "php_http.h"

#if defined(ZEND_ENGINE_2) && defined(HTTP_HAVE_CURL)

#include "php_http_api.h"
#include "php_http_exception_object.h"
#include "php_http_request_api.h"
#include "php_http_request_object.h"
#include "php_http_request_pool_api.h"
#include "php_http_requestpool_object.h"

#ifndef HTTP_DEBUG_REQPOOLS
#	define HTTP_DEBUG_REQPOOLS 0
#endif

ZEND_EXTERN_MODULE_GLOBALS(http);

#ifndef HAVE_CURL_MULTI_STRERROR
#	define curl_multi_strerror(dummy) "unknown error"
#endif

static void http_request_pool_freebody(http_request_callback_ctx **body);
static int http_request_pool_compare_handles(void *h1, void *h2);

/* {{{ http_request_pool *http_request_pool_init(http_request_pool *) */
PHP_HTTP_API http_request_pool *_http_request_pool_init(http_request_pool *pool TSRMLS_DC)
{
	zend_bool free_pool;
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Initializing request pool %p\n", pool);
#endif
	
	if ((free_pool = (!pool))) {
		pool = emalloc(sizeof(http_request_pool));
		pool->ch = NULL;
	}

	HTTP_CHECK_CURL_INIT(pool->ch, curl_multi_init(), ;);
	if (!pool->ch) {
		if (free_pool) {
			efree(pool);
		}
		return NULL;
	}

	pool->unfinished = 0;
	zend_llist_init(&pool->finished, sizeof(zval *), (llist_dtor_func_t) ZVAL_PTR_DTOR, 0);
	zend_llist_init(&pool->handles, sizeof(zval *), (llist_dtor_func_t) ZVAL_PTR_DTOR, 0);
	zend_llist_init(&pool->bodies, sizeof(http_request_callback_ctx *), (llist_dtor_func_t) http_request_pool_freebody, 0);
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Initialized request pool %p\n", pool);
#endif
	
	return pool;
}
/* }}} */

/* {{{ STATUS http_request_pool_attach(http_request_pool *, zval *) */
PHP_HTTP_API STATUS _http_request_pool_attach(http_request_pool *pool, zval *request TSRMLS_DC)
{
	getObjectEx(http_request_object, req, request);
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Attaching HttpRequest(#%d) %p to pool %p\n", Z_OBJ_HANDLE_P(request), req, pool);
#endif
	
	if (req->pool) {
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "HttpRequest object(#%d) is already member of %s HttpRequestPool", Z_OBJ_HANDLE_P(request), req->pool == pool ? "this" : "another");
	} else {
		http_request_callback_ctx *body = http_request_callback_data_ex(http_request_body_new(), 0);

		if (SUCCESS != http_request_pool_requesthandler(request, body->data)) {
			http_error_ex(HE_WARNING, HTTP_E_REQUEST, "Could not initialize HttpRequest object for attaching to the HttpRequestPool");
		} else {
			CURLMcode code = curl_multi_add_handle(pool->ch, req->ch);

			if ((CURLM_OK != code) && (CURLM_CALL_MULTI_PERFORM != code)) {
				http_error_ex(HE_WARNING, HTTP_E_REQUEST_POOL, "Could not attach HttpRequest object to the HttpRequestPool: %s", curl_multi_strerror(code));
			} else {
				req->pool = pool;

				zend_llist_add_element(&pool->handles, &request);
				zend_llist_add_element(&pool->bodies, &body);

				ZVAL_ADDREF(request);

#if HTTP_DEBUG_REQPOOLS
				fprintf(stderr, "> %d HttpRequests attached to pool %p\n", zend_llist_count(&pool->handles), pool);
#endif
				return SUCCESS;
			}
		}
		efree(body->data);
		efree(body);
	}
	return FAILURE;
}
/* }}} */

/* {{{ STATUS http_request_pool_detach(http_request_pool *, zval *) */
PHP_HTTP_API STATUS _http_request_pool_detach(http_request_pool *pool, zval *request TSRMLS_DC)
{
	CURLMcode code;
	getObjectEx(http_request_object, req, request);
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Detaching HttpRequest(#%d) %p from pool %p\n", Z_OBJ_HANDLE_P(request), req, pool);
#endif
	
	if (!req->pool) {
		/* not attached to any pool */
#if HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "HttpRequest object(#%d) %p is not attached to any HttpRequestPool\n", Z_OBJ_HANDLE_P(request), req);
#endif
	} else if (req->pool != pool) {
		http_error_ex(HE_WARNING, HTTP_E_INVALID_PARAM, "HttpRequest object(#%d) is not attached to this HttpRequestPool", Z_OBJ_HANDLE_P(request));
	} else if (CURLM_OK != (code = curl_multi_remove_handle(pool->ch, req->ch))) {
		http_error_ex(HE_WARNING, HTTP_E_REQUEST_POOL, "Could not detach HttpRequest object from the HttpRequestPool: %s", curl_multi_strerror(code));
	} else {
		req->pool = NULL;
		zend_llist_del_element(&pool->finished, request, http_request_pool_compare_handles);
		zend_llist_del_element(&pool->handles, request, http_request_pool_compare_handles);
		
#if HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "> %d HttpRequests remaining in pool %p\n", zend_llist_count(&pool->handles), pool);
#endif
	
		return SUCCESS;
	}
	return FAILURE;
}
/* }}} */

/* {{{ void http_request_pool_detach_all(http_request_pool *) */
PHP_HTTP_API void _http_request_pool_detach_all(http_request_pool *pool TSRMLS_DC)
{
	int count = zend_llist_count(&pool->handles);
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Detaching %d requests from pool %p\n", count, pool);
#endif
	
	/*
	 * we cannot apply a function to the llist which actually detaches
	 * the curl handle *and* removes the llist element --
	 * so let's get our hands dirty
	 */
	if (count) {
		int i = 0;
		zend_llist_position pos;
		zval **handle, **handles = emalloc(count * sizeof(zval *));

		for (handle = zend_llist_get_first_ex(&pool->handles, &pos); handle; handle = zend_llist_get_next_ex(&pool->handles, &pos)) {
			handles[i++] = *handle;
		}
		
		/* should never happen */
		if (i != count) {
			zend_error(E_ERROR, "number of fetched request handles do not match overall count");
			count = i;
		}
		
		for (i = 0; i < count; ++i) {
			http_request_pool_detach(pool, handles[i]);
		}
		efree(handles);
	}
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Destroying %d request bodies of pool %p\n", zend_llist_count(&pool->bodies), pool);
#endif
	
	/* free created bodies too */
	zend_llist_clean(&pool->bodies);
}

/* {{{ STATUS http_request_pool_send(http_request_pool *) */
PHP_HTTP_API STATUS _http_request_pool_send(http_request_pool *pool TSRMLS_DC)
{
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Attempt to send %d requests of pool %p\n", zend_llist_count(&pool->handles), pool);
#endif
	
	while (http_request_pool_perform(pool)) {
		if (SUCCESS != http_request_pool_select(pool)) {
#ifdef PHP_WIN32
			http_error(HE_WARNING, HTTP_E_SOCKET, WSAGetLastError());
#else
			http_error(HE_WARNING, HTTP_E_SOCKET, strerror(errno));
#endif
			return FAILURE;
		}
	}
	
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Finished sending %d HttpRequests of pool %p (still unfinished: %d)\n", zend_llist_count(&pool->handles), pool, pool->unfinished);
#endif
	
	return SUCCESS;
}
/* }}} */

/* {{{ void http_request_pool_dtor(http_request_pool *) */
PHP_HTTP_API void _http_request_pool_dtor(http_request_pool *pool TSRMLS_DC)
{
#if HTTP_DEBUG_REQPOOLS
	fprintf(stderr, "Destructing request pool %p\n", pool);
#endif
	
	pool->unfinished = 0;
	zend_llist_clean(&pool->finished);
	zend_llist_clean(&pool->handles);
	zend_llist_clean(&pool->bodies);
	curl_multi_cleanup(pool->ch);
}
/* }}} */

#ifdef PHP_WIN32
#	define SELECT_ERROR SOCKET_ERROR
#else
#	define SELECT_ERROR -1
#endif

/* {{{ STATUS http_request_pool_select(http_request_pool *) */
PHP_HTTP_API STATUS _http_request_pool_select(http_request_pool *pool)
{
	int MAX;
	fd_set R, W, E;
	struct timeval timeout = {1, 0};

	FD_ZERO(&R);
	FD_ZERO(&W);
	FD_ZERO(&E);

	if (CURLM_OK == curl_multi_fdset(pool->ch, &R, &W, &E, &MAX)) {
		if (MAX == -1 || SELECT_ERROR != select(MAX + 1, &R, &W, &E, &timeout)) {
			return SUCCESS;
		}
	}
	return FAILURE;
}
/* }}} */

/* {{{ int http_request_pool_perform(http_request_pool *) */
PHP_HTTP_API int _http_request_pool_perform(http_request_pool *pool TSRMLS_DC)
{
	CURLMsg *msg;
	int remaining = 0;
	
	while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(pool->ch, &pool->unfinished));
	
	while ((msg = curl_multi_info_read(pool->ch, &remaining))) {
		if (CURLMSG_DONE == msg->msg) {
			if (CURLE_OK != msg->data.result) {
				http_request_pool_try {
					http_error(HE_WARNING, HTTP_E_REQUEST, curl_easy_strerror(msg->data.result));
				} http_request_pool_catch();
			}
			http_request_pool_try {
				zend_llist_apply_with_argument(&pool->handles, (llist_apply_with_arg_func_t) http_request_pool_responsehandler, msg->easy_handle TSRMLS_CC);
			} http_request_pool_catch();
		}
	}
	http_request_pool_final();
	
	return pool->unfinished;
}
/* }}} */

/* {{{ STATUS http_request_pool_requesthandler(zval *, http_request_body *) */
STATUS _http_request_pool_requesthandler(zval *request, http_request_body *body TSRMLS_DC)
{
	getObjectEx(http_request_object, req, request);
	if (SUCCESS == http_request_object_requesthandler(req, request, body)) {
		http_request_conv(req->ch, &req->response, &req->request);
		return SUCCESS;
	}
	return FAILURE;
}
/* }}} */

/* {{{ void http_request_pool_responsehandler(zval **) */
void _http_request_pool_responsehandler(zval **req, CURL *ch TSRMLS_DC)
{
	getObjectEx(http_request_object, obj, *req);
	
	if (obj->ch == ch) {
		
#if HTTP_DEBUG_REQPOOLS
		fprintf(stderr, "Fetching data from HttpRequest(#%d) %p of pool %p\n", Z_OBJ_HANDLE_PP(req), obj, obj->pool);
#endif
		
		ZVAL_ADDREF(*req);
		zend_llist_add_element(&obj->pool->finished, req);
		http_request_object_responsehandler(obj, *req);
	}
}
/* }}} */

static void move_backtrace_args(zval *from, zval *to TSRMLS_DC)
{
	zval **args, **trace_0, *old_trace_0, *trace = NULL;
	
	if ((trace = zend_read_property(zend_exception_get_default(), from, "trace", lenof("trace"), 0 TSRMLS_CC))) {
		if (SUCCESS == zend_hash_index_find(Z_ARRVAL_P(trace), 0, (void **) &trace_0)) {
			old_trace_0 = *trace_0;
			if (SUCCESS == zend_hash_find(Z_ARRVAL_PP(trace_0), "args", sizeof("args"), (void **) &args)) {
				if ((trace = zend_read_property(zend_exception_get_default(), to, "trace", lenof("trace"), 0 TSRMLS_CC))) {
					if (SUCCESS == zend_hash_index_find(Z_ARRVAL_P(trace), 0, (void **) &trace_0)) {
						ZVAL_ADDREF(*args);
						add_assoc_zval(*trace_0, "args", *args);
						zend_hash_del(Z_ARRVAL_P(old_trace_0), "args", sizeof("args"));
					}
				}
			}
		}
	}
}
/* {{{ void http_request_pool_wrap_exception(zval *, zval *) */
void _http_request_pool_wrap_exception(zval *old_exception, zval *new_exception TSRMLS_DC)
{
	zend_class_entry *ce = HTTP_EX_CE(request_pool);
	
	/*	if old_exception is already an HttpRequestPoolException append the new one, 
		else create a new HttpRequestPoolException and append the old and new exceptions */
	if (old_exception && Z_OBJCE_P(old_exception) == ce) {
		zval *exprop;
		
		exprop = zend_read_property(ce, old_exception, "exceptionStack", lenof("exceptionStack"), 0 TSRMLS_CC);
		SEP_PROP(&exprop);
		convert_to_array(exprop);
		
		add_next_index_zval(exprop, new_exception);
		zend_update_property(ce, old_exception, "exceptionStack", lenof("exceptionStack"), exprop TSRMLS_CC);
		
		EG(exception) = old_exception;
	} else if (new_exception && Z_OBJCE_P(new_exception) != ce){
		zval *exval, *exprop;
		
		MAKE_STD_ZVAL(exval);
		object_init_ex(exval, ce);
		MAKE_STD_ZVAL(exprop);
		array_init(exprop);
		
		if (old_exception) {
			add_next_index_zval(exprop, old_exception);
		}
		move_backtrace_args(new_exception, exval TSRMLS_CC);
		zend_update_property_long(ce, exval, "code", lenof("code"), HTTP_E_REQUEST_POOL TSRMLS_CC);
		zend_update_property_string(ce, exval, "message", lenof("message"), "See exceptionStack property" TSRMLS_CC);
		add_next_index_zval(exprop, new_exception);
		zend_update_property(ce, exval, "exceptionStack", lenof("exceptionStack"), exprop TSRMLS_CC);
		zval_ptr_dtor(&exprop);
		
		EG(exception) = exval;
	}
}
/* }}} */

/*#*/

/* {{{ static void http_request_pool_freebody(http_request_ctx **) */
static void http_request_pool_freebody(http_request_callback_ctx **body)
{
	HTTP_REQUEST_CALLBACK_DATA(*body, http_request_body *, b);
	http_request_body_free(&b);
	efree(*body);
	*body = NULL;
}
/* }}} */

/* {{{ static int http_request_pool_compare_handles(void *, void *) */
static int http_request_pool_compare_handles(void *h1, void *h2)
{
	return (Z_OBJ_HANDLE_PP((zval **) h1) == Z_OBJ_HANDLE_P((zval *) h2));
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

