--TEST--
[CLIENT] Execute pipelined commands returning nested multibulk responses

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
require_once 'testsuite_utilities.inc';

$redis = create_phpiredis_connection(REDIS_HOST, REDIS_PORT);

$commands = phpiredis_multi_command($redis, array(
    'DEL test',
    'LPUSH test 1',
    'LPUSH test 2',
    'LPUSH test 3',
    'LRANGE test 0 -1',
));

var_dump($commands[4]);

--EXPECT--
array(3) {
  [0]=>
  string(1) "3"
  [1]=>
  string(1) "2"
  [2]=>
  string(1) "1"
}
