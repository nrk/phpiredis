--TEST--
[READER] Properly handle NULL responses

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

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

--EXPECT--
NULL
NULL
array(1) {
  [0]=>
  NULL
}
