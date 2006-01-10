--TEST--
http_send_data() oversized range
--SKIPIF--
<?php 
include 'skip.inc';
checkcgi();
?>
--FILE--
<?php
$_SERVER['HTTP_RANGE'] = 'bytes=5990-6000';
http_send_content_type('text/plain');
http_send_data(str_repeat('123abc', 1000));
?>
--EXPECTF--
Status: 416
%s