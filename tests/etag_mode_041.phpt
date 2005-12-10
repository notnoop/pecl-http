--TEST--
ob crc32 etag (may fail because PHPs crc32 is actually crc32b)
--SKIPIF--
<?php
include 'skip.inc';
checkcgi();
checkmin(5.1);
?>
--FILE--
<?php
ini_set('http.etag_mode', 'crc32');
http_cache_etag();
print("abc\n");
?>
--EXPECTF--
X-Powered-By: PHP/%s
Cache-Control: private, must-revalidate, max-age=0
ETag: "4e818847"
Content-type: %s

abc
