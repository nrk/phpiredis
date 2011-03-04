--TEST--
Test possible segfault
--SKIPIF--
<?php include 'skipif.inc'; ?>
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
?>
--EXPECTF--
string(2) "OK"
string(6) "QUEUED"
array(1) {
  [0]=>
  int(1)
}
