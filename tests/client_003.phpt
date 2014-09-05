--TEST--
[CLIENT] Persistent and non-persistent connections (UNIX socket)

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

if (!file_exists(REDIS_UNIX_SOCKET)) {
    die('skip Cannot find UNIX domain socket file at ' . REDIS_UNIX_SOCKET);
}

--FILE--
<?php
require_once 'testsuite_utilities.inc';

create_phpiredis_connection(REDIS_UNIX_SOCKET, 9999);
create_phpiredis_connection(REDIS_UNIX_SOCKET, NULL);
create_phpiredis_connection(REDIS_UNIX_SOCKET, NULL, TRUE);

echo "OK" . PHP_EOL;

--EXPECT--
OK
