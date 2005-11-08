--TEST--
HttpRequestPool::__destruct() invalid curl handle
--SKIPIF--
<?php
include 'skip.inc';
checkmin(5);
checkcls('HttpRequest');
checkcls('HttpRequestPool');
?>
--FILE--
<?php
echo "-TEST\n";
$p = new HttpRequestPool(new HttpRequest('http://example.com'));
$p = null;
echo "Done\n";
?>
--EXPECTF--
%sTEST
Done
