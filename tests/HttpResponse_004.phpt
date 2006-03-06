--TEST--
HttpResponse - send cached gzipped data
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmin(5.1);
skpif(!http_support(HTTP_SUPPORT_ENCODINGS), "need zlib support");
?>
--ENV--
HTTP_ACCEPT_ENCODING=gzip
HTTP_IF_NONE_MATCH="80b285463881575891e86ba7bfecb4d0"
--FILE--
<?php
HttpResponse::setGzip(true);
HttpResponse::setCache(true);
HttpResponse::setCacheControl('public', 3600);
HttpResponse::setData(file_get_contents(__FILE__));
HttpResponse::send();
?>
--EXPECTF--
Status: 304
X-Powered-By: PHP/%s
Cache-Control: public, must-revalidate, max-age=3600
ETag: "80b285463881575891e86ba7bfecb4d0"
Content-type: %s
