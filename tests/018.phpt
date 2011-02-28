--TEST--
Set status handler
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
function status_handler($err) { return ++$err; }
$reader = phpiredis_reader_create();
phpiredis_reader_feed($reader, "+OK\r\n");
var_dump(phpiredis_reader_get_reply($reader) === TRUE);
phpiredis_reader_set_status_handler($reader, 'status_handler');
phpiredis_reader_feed($reader, "+OK\r\n");
var_dump(phpiredis_reader_get_reply($reader) == 'OL');
var_dump(FALSE === @phpiredis_reader_set_status_handler($reader, 'fake_status_handler'));
phpiredis_reader_feed($reader, "+OK\r\n");
var_dump(phpiredis_reader_get_reply($reader) == 'OL');
var_dump(TRUE === phpiredis_reader_set_status_handler($reader, NULL));
phpiredis_reader_feed($reader, "+OK\r\n");
var_dump(phpiredis_reader_get_reply($reader) === TRUE);
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
