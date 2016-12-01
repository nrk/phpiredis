--TEST--
[CLIENT] Execute commands (binary-safe)

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
require_once 'testsuite_utilities.inc';

$redis = create_phpiredis_connection(REDIS_HOST, REDIS_PORT);

phpiredis_command_bs($redis, array('DEL', TESTKEY));
phpiredis_command_bs($redis, array('SET', TESTKEY, '1'));
$response = phpiredis_command_bs($redis, array('GET', TESTKEY));

var_dump($response);

--EXPECT--
string(1) "1"
