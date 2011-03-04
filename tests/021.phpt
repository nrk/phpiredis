--TEST--
Set reader error handler
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
$reader = phpiredis_reader_create();
$buffer = "*3\r\n:1\r\n+OK\r\n*2\r\n$3\r\nASD\r\n$-1\r\n";
phpiredis_reader_feed($reader, $buffer);
var_dump(phpiredis_reader_get_reply($reader));
?>
--EXPECTF--
array(3) {
  [0]=>
  int(1)
  [1]=>
  string(2) "OK"
  [2]=>
  array(2) {
    [0]=>
    string(3) "ASD"
    [1]=>
    NULL
  }
}
