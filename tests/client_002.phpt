--TEST--
[CLIENT] Persistent and non-persistent connections

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
require_once 'testsuite_utilities.inc';

create_phpiredis_connection(REDIS_HOST, REDIS_PORT);
create_phpiredis_connection(REDIS_HOST, REDIS_PORT, TRUE);

echo "OK" . PHP_EOL;

--EXPECT--
OK
