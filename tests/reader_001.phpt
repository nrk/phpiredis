--TEST--
[READER] Create reader resource

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
$reader = phpiredis_reader_create();
var_dump($reader);

phpiredis_reader_destroy($reader);


--EXPECTF--
resource(%d) of type (phpredis reader)
