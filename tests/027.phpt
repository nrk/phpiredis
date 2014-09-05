--TEST--
Test persistent and non-persistent connections (UNIX socket)
--SKIPIF--
<?php
include 'skipif.inc';

if (!file_exists("/tmp/redis.sock")) {
    die('SKIP could not find required /tmp/redis.sock');
}
?>
--FILE--
<?php
require_once('connect.inc');

$host = '/tmp/redis.sock';

if (!$link = my_phpiredis_connect($host)) {
    printf("[001] Cannot connect to the server using host=%s\n", $host);
}

if (!$link = my_phpiredis_connect($host, 9999)) {
    printf("[001] Cannot connect to the server using host=%s\n", $host);
}

if (!$link = my_phpiredis_connect($host, NULL, true)) {
    printf("[001] Cannot pconnect to the server using host=%s\n", $host);
}

echo "OK" . PHP_EOL;
?>
--EXPECT--
OK
