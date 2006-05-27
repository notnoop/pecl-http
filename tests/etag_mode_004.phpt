--TEST--
ext/hash etag
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmax(5.0);
skipif(!extension_loaded('hash'), 'need ext/hash support');
?>
--FILE--
<?php
ini_set('http.etag.mode', 'sha256');
http_cache_etag();
http_send_data("abc\n");
?>
--EXPECTF--
Content-type: %s
X-Powered-By: PHP/%s
Cache-Control: private, must-revalidate, max-age=0
Accept-Ranges: bytes
ETag: "edeaaff3f1774ad2888673770c6d64097e391bc362d7d6fb34982ddf0efd18cb"
Content-Length: 4

abc
