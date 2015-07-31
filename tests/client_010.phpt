--TEST--
[CLIENT] Do not attempt to reconnect upon disconnection

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
require_once 'testsuite_utilities.inc';

$redis = create_phpiredis_connection(REDIS_HOST, REDIS_PORT);

phpiredis_command($redis, 'SET ' . TESTKEY . ' 1');

var_dump(phpiredis_command($redis, 'GET ' . TESTKEY));
var_dump(phpiredis_command($redis, 'QUIT'));
var_dump(phpiredis_command($redis, 'GET ' . TESTKEY));

--EXPECT--
string(1) "1"
string(2) "OK"
bool(false)
