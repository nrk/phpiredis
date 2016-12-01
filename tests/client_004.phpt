--TEST--
[CLIENT] Execute commands

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
require_once 'testsuite_utilities.inc';

$redis = create_phpiredis_connection(REDIS_HOST, REDIS_PORT);

phpiredis_command($redis, 'DEL ' . TESTKEY);
phpiredis_command($redis, 'SET ' . TESTKEY . ' 1');
$response = phpiredis_command($redis, 'GET ' . TESTKEY);

var_dump($response);

--EXPECT--
string(1) "1"
