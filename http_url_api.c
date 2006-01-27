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
#define HTTP_WANT_NETDB
#include "php_http.h"

#include "zend_ini.h"
#include "php_output.h"
#include "ext/standard/php_string.h"

#include "php_http_api.h"
#include "php_http_url_api.h"

PHP_MINIT_FUNCTION(http_url)
{
	HTTP_LONG_CONSTANT("HTTP_URL_REPLACE", HTTP_URL_REPLACE);
	HTTP_LONG_CONSTANT("HTTP_URL_JOIN_PATH", HTTP_URL_JOIN_PATH);
	HTTP_LONG_CONSTANT("HTTP_URL_JOIN_QUERY", HTTP_URL_JOIN_QUERY);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_USER", HTTP_URL_STRIP_USER);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_PASS", HTTP_URL_STRIP_PASS);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_AUTH", HTTP_URL_STRIP_AUTH);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_PORT", HTTP_URL_STRIP_PORT);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_PATH", HTTP_URL_STRIP_PATH);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_QUERY", HTTP_URL_STRIP_QUERY);
	HTTP_LONG_CONSTANT("HTTP_URL_STRIP_FRAGMENT", HTTP_URL_STRIP_FRAGMENT);
	return SUCCESS;
}

PHP_HTTP_API char *_http_absolute_url(const char *url TSRMLS_DC)
{
	char *abs = estrdup(url);
	php_url *purl = php_url_parse(abs);
	
	STR_SET(abs, NULL);
	
	if (purl) {
		http_build_url(0, purl, NULL, NULL, &abs, NULL);
		php_url_free(purl);
	} else {
		http_error_ex(HE_WARNING, HTTP_E_URL, "Could not parse URL (%s)", url);
	}
	
	return abs;
}

/* {{{ void http_build_url(int flags, const php_url *, const php_url *, php_url **, char **, size_t *) */
PHP_HTTP_API void _http_build_url(int flags, const php_url *old_url, const php_url *new_url, php_url **url_ptr, char **url_str, size_t *url_len TSRMLS_DC)
{
#ifdef HTTP_HAVE_NETDB
	struct servent *se;
#endif
	php_url *url = ecalloc(1, sizeof(php_url));

#define __URLSET(u,n) \
	((u)&&(u)->n)
#define __URLCPY(n) \
	url->n = __URLSET(new_url,n) ? estrdup(new_url->n) : (__URLSET(old_url,n) ? estrdup(old_url->n) : NULL)
	
	if (!(flags & HTTP_URL_STRIP_PORT)) {
		url->port = (new_url&&new_url->port) ? new_url->port : ((old_url) ? old_url->port : 0);
	}
	if ((!(flags & HTTP_URL_STRIP_AUTH)) && (!(flags & HTTP_URL_STRIP_USER))) {
		__URLCPY(user);
	}
	if ((!(flags & HTTP_URL_STRIP_AUTH)) && (!(flags & HTTP_URL_STRIP_PASS))) {
		__URLCPY(pass);
	}
	
	__URLCPY(scheme);
	__URLCPY(host);
	
	if (!(flags & HTTP_URL_STRIP_PATH)) {
		if ((flags & HTTP_URL_JOIN_PATH) && __URLSET(old_url, path) && __URLSET(new_url, path) && *new_url->path != '/') {
			size_t old_path_len = strlen(old_url->path), new_path_len = strlen(new_url->path);
			
			url->path = ecalloc(1, old_path_len + new_path_len + 1 + 1);
			
			strcat(url->path, old_url->path);
			if (url->path[old_path_len - 1] != '/') {
				php_dirname(url->path, old_path_len);
				strcat(url->path, "/");
			}
			strcat(url->path, new_url->path);
		} else {
			__URLCPY(path);
		}
	}
	if (!(flags & HTTP_URL_STRIP_QUERY)) {
		if ((flags & HTTP_URL_JOIN_QUERY) && __URLSET(new_url, query) && __URLSET(old_url, query)) {
			url->query = ecalloc(1, strlen(new_url->query) + strlen(old_url->query) + 1 + 1);
			strcat(url->query, old_url->query);
			strcat(url->query, "&");
			strcat(url->query, new_url->query);
		} else {
			__URLCPY(query);
		}
	}
	if (!(flags & HTTP_URL_STRIP_FRAGMENT)) {
		__URLCPY(fragment);
	}
	
	if (!url->scheme) {
		switch (url->port)
		{
			case 443:
				url->scheme = estrndup("https", lenof("https"));
			break;

#ifndef HTTP_HAVE_NETDB
			default:
#endif
			case 80:
				url->scheme = estrndup("http", lenof("http"));
			break;
			
#ifdef HTTP_HAVE_NETDB
			default:
				if ((se = getservbyport(htons(url->port), "tcp")) && se->s_name) {
					url->scheme = estrdup(se->s_name);
				} else {
					url->scheme = estrndup("http", lenof("http"));
				}
			break;
#endif
		}
	}

	if (!url->host) {
		zval *zhost;
		
		if ((((zhost = http_get_server_var("HTTP_HOST")) || 
				(zhost = http_get_server_var("SERVER_NAME")))) && Z_STRLEN_P(zhost)) {
			url->host = estrndup(Z_STRVAL_P(zhost), Z_STRLEN_P(zhost));
		} else {
			url->host = estrndup("localhost", lenof("localhost"));
		}
	}
	
	if (!url->path) {
		if (SG(request_info).request_uri && *SG(request_info).request_uri) {
			const char *q = strchr(SG(request_info).request_uri, '?');
			
			if (q) {
				url->path = estrndup(SG(request_info).request_uri, q - SG(request_info).request_uri);
			} else {
				url->path = estrdup(SG(request_info).request_uri);
			}
		} else {
			url->path = estrndup("/", 1);
		}
	} else if (*url->path != '/') {
		if (SG(request_info).request_uri && *SG(request_info).request_uri) {
			const char *q = strchr(SG(request_info).request_uri, '?');
			char *uri, *path;
			size_t len;
			
			if (q) {
				uri = estrndup(SG(request_info).request_uri, len = q - SG(request_info).request_uri);
			} else {
				uri = estrndup(SG(request_info).request_uri, len = strlen(SG(request_info).request_uri));
			}
			
			php_dirname(uri, len);
			spprintf(&path, 0, "%s/%s", uri, url->path);
			efree(uri);
			STR_SET(url->path, path);
		} else {
			char *uri;
			
			spprintf(&uri, 0, "/%s", url->path);
			STR_SET(url->path, uri);
		}
	}
	if (url->path) {
		char *ptr, *end = url->path + strlen(url->path) + 1;
			
		for (ptr = strstr(url->path, "/."); ptr; ptr = strstr(ptr, "/.")) {
			switch (ptr[2])
			{
				case '\0':
					ptr[1] = '\0';
				break;
				
				case '/':
					memmove(&ptr[1], &ptr[3], end - &ptr[3]);
				break;
					
				case '.':
					if (ptr[3] == '/') {
						char *pos = &ptr[4];
						while (ptr != url->path) {
							if (*--ptr == '/') {
								break;
							}
						}
						memmove(&ptr[1], pos, end - pos);
					}
				break;
			}
		}
	}
	
	if (url->port) {
		if (	((url->port == 80) && !strcmp(url->scheme, "http"))
			||	((url->port ==443) && !strcmp(url->scheme, "https"))
#ifdef HTTP_HAVE_NETDB
			||	((se = getservbyname(url->scheme, "tcp")) && se->s_port && 
					(url->port == ntohs(se->s_port)))
#endif
		) {
			url->port = 0;
		}
	}
	
	if (url_str) {
		size_t len;
		
		*url_str = emalloc(HTTP_URL_MAXLEN + 1);
		
		**url_str = '\0';
		strlcat(*url_str, url->scheme, HTTP_URL_MAXLEN);
		strlcat(*url_str, "://", HTTP_URL_MAXLEN);
		
		if (url->user && *url->user) {
			strlcat(*url_str, url->user, HTTP_URL_MAXLEN);
			if (url->pass && *url->pass) {
				strlcat(*url_str, ":", HTTP_URL_MAXLEN);
				strlcat(*url_str, url->pass, HTTP_URL_MAXLEN);
			}
			strlcat(*url_str, "@", HTTP_URL_MAXLEN);
		}
		
		strlcat(*url_str, url->host, HTTP_URL_MAXLEN);
		
		if (url->port) {
			char port_str[6] = {0};
			
			snprintf(port_str, 5, "%d", (int) url->port);
			strlcat(*url_str, ":", HTTP_URL_MAXLEN);
			strlcat(*url_str, port_str, HTTP_URL_MAXLEN);
		}
		
		if (*url->path != '/') {
			strlcat(*url_str, "/", HTTP_URL_MAXLEN);
		}
		strlcat(*url_str, url->path, HTTP_URL_MAXLEN);
		
		if (url->query && *url->query) {
			strlcat(*url_str, "?", HTTP_URL_MAXLEN);
			strlcat(*url_str, url->query, HTTP_URL_MAXLEN);
		}
		
		if (url->fragment && *url->fragment) {
			strlcat(*url_str, "#", HTTP_URL_MAXLEN);
			strlcat(*url_str, url->fragment, HTTP_URL_MAXLEN);
		}
		
		if (HTTP_URL_MAXLEN == (len = strlen(*url_str))) {
			http_error(HE_NOTICE, HTTP_E_URL, "Length of URL exceeds HTTP_URL_MAXLEN");
		}
		if (url_len) {
			*url_len = len;
		}
	}
	
	if (url_ptr) {
		*url_ptr = url;
	} else {
		php_url_free(url);
	}
}
/* }}} */

/* {{{ STATUS http_urlencode_hash_ex(HashTable *, zend_bool, char *, size_t, char **, size_t *) */
PHP_HTTP_API STATUS _http_urlencode_hash_ex(HashTable *hash, zend_bool override_argsep,
	char *pre_encoded_data, size_t pre_encoded_len,
	char **encoded_data, size_t *encoded_len TSRMLS_DC)
{
	char *arg_sep;
	size_t arg_sep_len;
	phpstr *qstr = phpstr_new();

	if (override_argsep || !(arg_sep_len = strlen(arg_sep = INI_STR("arg_separator.output")))) {
		arg_sep = HTTP_URL_ARGSEP;
		arg_sep_len = lenof(HTTP_URL_ARGSEP);
	}

	if (pre_encoded_len && pre_encoded_data) {
		phpstr_append(qstr, pre_encoded_data, pre_encoded_len);
	}

	if (SUCCESS != http_urlencode_hash_recursive(hash, qstr, arg_sep, arg_sep_len, NULL, 0)) {
		phpstr_free(&qstr);
		return FAILURE;
	}

	phpstr_data(qstr, encoded_data, encoded_len);
	phpstr_free(&qstr);

	return SUCCESS;
}
/* }}} */

/* {{{ http_urlencode_hash_recursive */
PHP_HTTP_API STATUS _http_urlencode_hash_recursive(HashTable *ht, phpstr *str, const char *arg_sep, size_t arg_sep_len, const char *prefix, size_t prefix_len TSRMLS_DC)
{
	char *key = NULL;
	uint len = 0;
	ulong idx = 0;
	zval **data = NULL;
	HashPosition pos;

	if (!ht || !str) {
		http_error(HE_WARNING, HTTP_E_INVALID_PARAM, "Invalid parameters");
		return FAILURE;
	}
	if (ht->nApplyCount > 0) {
		return SUCCESS;
	}
	
	FOREACH_HASH_KEYLENVAL(pos, ht, key, len, idx, data) {
		char *encoded_key;
		int encoded_len;
		phpstr new_prefix;
		
		if (!data || !*data) {
			return FAILURE;
		}
		
		if (key) {
			if (len && key[len - 1] == '\0') {
				--len;
			}
			encoded_key = php_url_encode(key, len, &encoded_len);
			key = NULL;
		} else {
			encoded_len = spprintf(&encoded_key, 0, "%ld", idx);
		}
		
		{
			phpstr_init(&new_prefix);
			if (prefix && prefix_len) {
				phpstr_append(&new_prefix, prefix, prefix_len);
				phpstr_appends(&new_prefix, "[");
			}
			
			phpstr_append(&new_prefix, encoded_key, encoded_len);
			efree(encoded_key);
			
			if (prefix && prefix_len) {
				phpstr_appends(&new_prefix, "]");
			}
			phpstr_fix(&new_prefix);
		}
		
		if (Z_TYPE_PP(data) == IS_ARRAY) {
			STATUS status;
			++ht->nApplyCount;
			status = http_urlencode_hash_recursive(Z_ARRVAL_PP(data), str, arg_sep, arg_sep_len, PHPSTR_VAL(&new_prefix), PHPSTR_LEN(&new_prefix));
			--ht->nApplyCount;
			if (SUCCESS != status) {
				phpstr_dtor(&new_prefix);
				return FAILURE;
			}
		} else {
			zval *val = zval_copy(IS_STRING, *data);
			
			if (PHPSTR_LEN(str)) {
				phpstr_append(str, arg_sep, arg_sep_len);
			}
			phpstr_append(str, PHPSTR_VAL(&new_prefix), PHPSTR_LEN(&new_prefix));
			phpstr_appends(str, "=");
			
			if (Z_STRLEN_P(val) && Z_STRVAL_P(val)) {
				char *encoded_val;
				int encoded_len;
				
				encoded_val = php_url_encode(Z_STRVAL_P(val), Z_STRLEN_P(val), &encoded_len);
				phpstr_append(str, encoded_val, encoded_len);
				efree(encoded_val);
			}
			
			zval_free(&val);
		}
		phpstr_dtor(&new_prefix);
	}
	return SUCCESS;
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

