--TEST--
[CLIENT] Execute pipelined commands (binary safe)

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
require_once 'testsuite_utilities.inc';

$redis = create_phpiredis_connection(REDIS_HOST, REDIS_PORT);

$commands = phpiredis_multi_command_bs($redis, array(
    array('DEL', 'test'),
    array('SET', 'test', '1'),
    array('GET', 'test')
));

var_dump($commands[2]);

--EXPECT--
string(1) "1"
