--TEST--
[CLIENT] Execute commands with binary data (binary-safe)

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

if (!@is_readable('/dev/urandom')) {
    die('skip Cannot read from /dev/urandom"');
}

--FILE--
<?php
require_once 'testsuite_utilities.inc';

$redis = create_phpiredis_connection(REDIS_HOST, REDIS_PORT);

$resource = fopen('/dev/urandom', 'r');
$data = fread($resource, 1024);
fclose($resource);

phpiredis_command_bs($redis, array('DEL', 'test'));
phpiredis_command_bs($redis, array('SET', 'test', $data));
$response = phpiredis_command_bs($redis, array('GET', 'test'));

var_dump($response === $data);

--EXPECT--
bool(true)
