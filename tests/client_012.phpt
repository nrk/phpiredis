--TEST--
[CLIENT] Connect to Redis with timeout

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
require_once 'testsuite_utilities.inc';

$redis = create_phpiredis_connection('169.254.10.10', REDIS_PORT, false, 200);
usleep(500000);

--EXPECTF--
Cannot connect to host '169.254.10.10' on port [6379]
