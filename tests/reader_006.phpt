--TEST--
[READER] Set custom error handler

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php

function error_handler($err) {
    return ++$err;
}

$reader = phpiredis_reader_create();

phpiredis_reader_feed($reader, "-ERR\r\n");
var_dump(phpiredis_reader_get_reply($reader) === 'ERR');

phpiredis_reader_set_error_handler($reader, 'error_handler');
phpiredis_reader_feed($reader, "-ERR\r\n");
var_dump(phpiredis_reader_get_reply($reader) == 'ERS');
var_dump(FALSE === @phpiredis_reader_set_error_handler($reader, 'fake_error_handler'));

phpiredis_reader_feed($reader, "-ERR\r\n");
var_dump(phpiredis_reader_get_reply($reader) == 'ERS');
var_dump(TRUE === phpiredis_reader_set_error_handler($reader, NULL));

phpiredis_reader_feed($reader, "-ERR\r\n");
var_dump(phpiredis_reader_get_reply($reader) === 'ERR');

--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
