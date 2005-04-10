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

#ifndef PHP_HTTP_API_H
#define PHP_HTTP_API_H

#include "php_http_std_defs.h"

char *pretty_key(char *key, size_t key_len, zend_bool uctitle, zend_bool xhyphen);

#define HTTP_GSC(var, name, ret)  HTTP_GSP(var, name, return ret)
#define HTTP_GSP(var, name, ret) \
		if (!(var = _http_get_server_var_ex(name, strlen(name)+1, 1 TSRMLS_CC))) { \
			ret; \
		}
#define http_got_server_var(v) (NULL != _http_get_server_var_ex((v), sizeof(v), 1 TSRMLS_CC))
#define http_get_server_var(v) http_get_server_var_ex((v), sizeof(v))
#define http_get_server_var_ex(v, s) _http_get_server_var_ex((v), (s), 0 TSRMLS_CC)
PHP_HTTP_API zval *_http_get_server_var_ex(const char *key, size_t key_size, zend_bool check TSRMLS_DC);

#define http_ob_etaghandler(o, l, ho, hl, m) _http_ob_etaghandler((o), (l), (ho), (hl), (m) TSRMLS_CC)
PHP_HTTP_API void _http_ob_etaghandler(char *output, uint output_len, char **handled_output, uint *handled_output_len, int mode TSRMLS_DC);

#define http_start_ob_handler(f, h, s, e) _http_start_ob_handler((f), (h), (s), (e) TSRMLS_CC)
PHP_HTTP_API STATUS _http_start_ob_handler(php_output_handler_func_t handler_func, char *handler_name, uint chunk_size, zend_bool erase TSRMLS_DC);

#define http_chunked_decode(e, el, d, dl) _http_chunked_decode((e), (el), (d), (dl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_chunked_decode(const char *encoded, size_t encoded_len, char **decoded, size_t *decoded_len TSRMLS_DC);

#define http_split_response(r, h, b) _http_split_response((r), (h), (b) TSRMLS_CC)
PHP_HTTP_API STATUS _http_split_response(zval *response, zval *headers, zval *body TSRMLS_DC);
#define http_split_response_ex(r, rl, h, b, bl) _http_split_response_ex((r), (rl), (h), (b), (bl) TSRMLS_CC)
PHP_HTTP_API STATUS _http_split_response_ex(char *response, size_t repsonse_len, HashTable *headers, char **body, size_t *body_len TSRMLS_DC);

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

