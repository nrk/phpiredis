--TEST--
[CLIENT] Execute pipelined commands
--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';
?>
--FILE--
<?php
require_once 'testsuite_utilities.inc';

$redis = create_phpiredis_connection(REDIS_HOST, REDIS_PORT);

$commands = phpiredis_multi_command($redis, array(
    'DEL ' . TESTKEY,
    'SET ' . TESTKEY . ' 1',
    'GET ' . TESTKEY
));

var_dump($commands[2]);
?>
--EXPECT--
string(1) "1"
