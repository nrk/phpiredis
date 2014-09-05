--TEST--
[CLIENT] Execute commands

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
require_once 'testsuite_utilities.inc';

$redis = create_phpiredis_connection(REDIS_HOST, REDIS_PORT);

phpiredis_command($redis, 'DEL test');
phpiredis_command($redis, 'SET test 1');
$response = phpiredis_command($redis, 'GET test');

var_dump($response);

--EXPECT--
string(1) "1"
