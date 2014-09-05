--TEST--
[READER] Test regression for segfaults

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
$reader = phpiredis_reader_create();

$buffer = "+OK\r\n";
phpiredis_reader_feed($reader, $buffer);
var_dump(phpiredis_reader_get_reply($reader));

$buffer = "+QUEUED\r\n";
phpiredis_reader_feed($reader, $buffer);
var_dump(phpiredis_reader_get_reply($reader));

$buffer = "*1\r\n:1\r\n";
phpiredis_reader_feed($reader, $buffer);
var_dump(phpiredis_reader_get_reply($reader));

--EXPECT--
string(2) "OK"
string(6) "QUEUED"
array(1) {
  [0]=>
  int(1)
}
