--TEST--
[CLIENT] Do not attempt to reconnect upon disconnection (pipeline)

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
require_once 'testsuite_utilities.inc';

$redis = create_phpiredis_connection(REDIS_HOST, REDIS_PORT);

phpiredis_command($redis, 'SET ' . TESTKEY . ' 1');
var_dump(phpiredis_multi_command($redis, array(
	'GET ' . TESTKEY,
	'QUIT',
	'GET ' . TESTKEY)
));

--EXPECT--
array(3) {
  [0]=>
  string(1) "1"
  [1]=>
  string(2) "OK"
  [2]=>
  bool(false)
}
