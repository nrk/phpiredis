--TEST--
[CLIENT] Connect to Redis with timeout

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
require_once 'testsuite_utilities.inc';

$redis = create_phpiredis_connection(REDIS_HOST, REDIS_PORT, false, 1000);
var_dump($redis);

--EXPECTF--
resource(%d) of type (phpredis connection)
