--TEST--
phpiredis reader resetting
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
$reader = phpiredis_reader_create();
phpiredis_reader_feed($reader, "+OK\r\n");
var_dump(phpiredis_reader_get_reply($reader));
$reader = phpiredis_reader_create();
phpiredis_reader_feed($reader, "-ERR\r\n");
var_dump(phpiredis_reader_get_reply($reader) == FALSE);
?>
--EXPECTF--
bool(true)
bool(true)
