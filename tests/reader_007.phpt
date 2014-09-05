--TEST--
[READER] Set custom status handler

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php

function status_handler($payload) {
    return "CUSTOM $payload";
}

$reader = phpiredis_reader_create();

phpiredis_reader_feed($reader, "+OK\r\n");
var_dump(phpiredis_reader_get_reply($reader) === 'OK');

phpiredis_reader_set_status_handler($reader, 'status_handler');
phpiredis_reader_feed($reader, "+OK\r\n");
var_dump(phpiredis_reader_get_reply($reader) === 'CUSTOM OK');
var_dump(FALSE === @phpiredis_reader_set_status_handler($reader, 'fake_status_handler'));

phpiredis_reader_feed($reader, "+PONG\r\n");
var_dump(phpiredis_reader_get_reply($reader) === 'CUSTOM PONG');

var_dump(TRUE === phpiredis_reader_set_status_handler($reader, NULL));
phpiredis_reader_feed($reader, "+QUEUED\r\n");
var_dump(phpiredis_reader_get_reply($reader) === 'QUEUED');

--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
