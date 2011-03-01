--TEST--
Set reader error handler
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
$reader = phpiredis_reader_create();
phpiredis_reader_feed($reader, "$-1\r\n");
$reply = phpiredis_reader_get_reply($reader);
var_dump($reply);
phpiredis_reader_feed($reader, "*-1\r\n");
$reply = phpiredis_reader_get_reply($reader);
var_dump($reply);
phpiredis_reader_feed($reader, "*1\r\n$-1\r\n");
$reply = phpiredis_reader_get_reply($reader);
var_dump($reply);
?>
--EXPECTF--
NULL
NULL
array(1) {
  [0]=>
  NULL
}
