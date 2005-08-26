dnl config.m4 for pecl/http
dnl $Id$

PHP_ARG_ENABLE([http], [whether to enable extended HTTP support],
[  --enable-http           Enable extended HTTP support])
PHP_ARG_WITH([curl], [for CURL support],
[  --with-curl[=DIR]       Include CURL support])
PHP_ARG_WITH([mhash], [for mhash support],
[  --with-mhash[=DIR]      Include mhash support])

if test "$PHP_HTTP" != "no"; then

dnl -------
dnl NETDB.H
dnl -------

	AC_MSG_CHECKING(for netdb.h)
	if test -r /usr/include/netdb.h -o -r /usr/local/include/netdb.h; then
		AC_DEFINE(HAVE_NETDB_H, 1, [Have netdb.h])
		AC_MSG_RESULT(found in default path)
	else
		AC_MSG_RESULT(not found in default path)
	fi

dnl ----
dnl CURL
dnl ----
	if test "$PHP_CURL" != "no"; then
		if test -r $PHP_CURL/include/curl/easy.h; then
			CURL_DIR=$PHP_CURL
		else
			AC_MSG_CHECKING(for CURL in default path)
			for i in /usr/local /usr; do
			if test -r $i/include/curl/easy.h; then
				CURL_DIR=$i
				AC_MSG_RESULT(found in $i)
				break
			fi
			done
		fi

		if test -z "$CURL_DIR"; then
			AC_MSG_RESULT(not found)
			AC_MSG_ERROR(Please reinstall the libcurl distribution -
			easy.h should be in <curl-dir>/include/curl/)
		fi

		CURL_CONFIG="curl-config"

		if ${CURL_DIR}/bin/curl-config --libs > /dev/null 2>&1; then
			CURL_CONFIG=${CURL_DIR}/bin/curl-config
		else
			if ${CURL_DIR}/curl-config --libs > /dev/null 2>&1; then
			CURL_CONFIG=${CURL_DIR}/curl-config
			fi
		fi

		PHP_ADD_INCLUDE($CURL_DIR/include)
		PHP_EVAL_LIBLINE($CURL_LIBS, CURL_SHARED_LIBADD)
		PHP_ADD_LIBRARY_WITH_PATH(curl, $CURL_DIR/lib, HTTP_SHARED_LIBADD)

		PHP_CHECK_LIBRARY(curl,curl_easy_init,
		[
			AC_DEFINE(HTTP_HAVE_CURL,1,[Have CURL easy support])
		],[
			AC_MSG_ERROR(There is something wrong with libcurl. Please check config.log for more information.)
		],[
			$CURL_LIBS -L$CURL_DIR/lib
		])

	fi

dnl ----
dnl MHASH
dnl ----

	if test "$PHP_MHASH" != "no"; then
		for i in $PHP_MHASH /usr/local /usr /opt/mhash; do
			test -f $i/include/mhash.h && MHASH_DIR=$i && break
		done
	
		if test -z "$MHASH_DIR"; then
			AC_MSG_ERROR(Please reinstall libmhash - cannot find mhash.h)
		fi
	
		PHP_ADD_INCLUDE($MHASH_DIR/include)
		PHP_ADD_LIBRARY_WITH_PATH(mhash, $MHASH_DIR/lib, MHASH_SHARED_LIBADD)
		AC_DEFINE(HAVE_LIBMHASH,1,[HAve mhash support])
	fi

dnl ----
dnl DONE
dnl ----
	PHP_HTTP_SOURCES="missing.c http.c http_functions.c phpstr/phpstr.c \
		http_util_object.c http_message_object.c http_request_object.c http_request_pool_api.c \
		http_response_object.c http_exception_object.c http_requestpool_object.c \
		http_api.c http_cache_api.c http_request_api.c http_date_api.c \
		http_headers_api.c http_message_api.c http_send_api.c http_url_api.c \
		http_info_api.c"
	PHP_NEW_EXTENSION([http], $PHP_HTTP_SOURCES, [$ext_shared])
	PHP_ADD_BUILD_DIR($ext_builddir/phpstr, 1)
	PHP_SUBST([HTTP_SHARED_LIBADD])
	PHP_ADD_MAKEFILE_FRAGMENT
	AC_DEFINE([HAVE_HTTP], [1], [Have extended HTTP support])

dnl ---
dnl odd warnings
dnl ---
dnl		CFLAGS=" -g -O2 -W -Wchar-subscripts -Wformat=2 -Wno-format-y2k -Wimplicit -Wmissing-braces -Wunused-variable -Wbad-function-cast -Wpointer-arith -Wsign-compare -Winline"
dnl		PHP_SUBST([CFLAGS])

fi
