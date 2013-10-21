--TEST--
Test persistent and non-persistent connections
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
require_once('connect.inc');
$test = '';

$host = '127.0.0.1';
if (!$link = my_phpiredis_connect($host))
        printf("[001] Cannot connect to the server using host=%s\n",
                $host);

if (!$link = my_phpiredis_connect($host, 6379, true))
        printf("[001] Cannot pconnect to the server using host=%s\n",
                $host);

echo "OK" . PHP_EOL;
?>
--EXPECT--
OK
