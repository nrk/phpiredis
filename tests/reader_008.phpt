--TEST--
[READER] Custom error handler throwing exceptions or returning objects

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php

function error_handler_exception($err) {
    throw new Exception($err);
}

function error_handler_object($err) {
    $r = new stdClass();
    $r->err = $err;
    return $r;
}

$reader = phpiredis_reader_create();

phpiredis_reader_set_error_handler($reader, 'error_handler_exception');
phpiredis_reader_feed($reader, "-ERR\r\n");

try {
    phpiredis_reader_get_reply($reader);
    var_dump(FALSE);
} catch (Exception $e) {
    var_dump($e->getMessage() == 'ERR');
}

phpiredis_reader_set_error_handler($reader, 'error_handler_object');
phpiredis_reader_feed($reader, "-ERR\r\n");
var_dump(phpiredis_reader_get_reply($reader));

--EXPECT--
bool(true)
object(stdClass)#2 (1) {
  ["err"]=>
  string(3) "ERR"
}
