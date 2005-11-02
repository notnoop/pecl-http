--TEST--
http_send_file() NIL-NUM range
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
checkmax(5.0);
?>
--ENV--
HTTP_RANGE=bytes=-9
--FILE--
<?php
http_send_file('data.txt');
?>
--EXPECTF--
Status: 206
Content-type: %s
X-Powered-By: PHP/%s
Accept-Ranges: bytes
Content-Range: bytes 1001-1009/1010
Content-Length: 9

23456789
