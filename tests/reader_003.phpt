--TEST--
[READER] Check reader state

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php

$reader = phpiredis_reader_create();

phpiredis_reader_feed($reader, "+OK\r\n");
var_dump(phpiredis_reader_get_reply($reader));
var_dump(phpiredis_reader_get_state($reader) == PHPIREDIS_READER_STATE_INCOMPLETE);

phpiredis_reader_feed($reader, "+OK\r\n");
var_dump(phpiredis_reader_get_state($reader) == PHPIREDIS_READER_STATE_COMPLETE);
var_dump(phpiredis_reader_get_reply($reader));

phpiredis_reader_feed($reader, "$3\r\nSE");
var_dump(phpiredis_reader_get_state($reader) == PHPIREDIS_READER_STATE_INCOMPLETE);

phpiredis_reader_feed($reader, "T\r\n");
var_dump(phpiredis_reader_get_state($reader) == PHPIREDIS_READER_STATE_COMPLETE);
var_dump(phpiredis_reader_get_reply($reader));

phpiredis_reader_feed($reader, "nSET\r\n");
var_dump(phpiredis_reader_get_reply($reader) == FALSE);
var_dump(phpiredis_reader_get_state($reader) == PHPIREDIS_READER_STATE_ERROR);
var_dump(phpiredis_reader_get_error($reader));

--EXPECT--
string(2) "OK"
bool(true)
bool(true)
string(2) "OK"
bool(true)
bool(true)
string(3) "SET"
bool(true)
bool(true)
string(42) "Protocol error, got "n" as reply type byte"
