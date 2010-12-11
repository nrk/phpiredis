--TEST--
phpiredis reconnect on disconnect
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

phpiredis_command($link, 'SET a 1');
var_dump(phpiredis_command($link, 'GET a'));
var_dump(phpiredis_command($link, 'QUIT'));
var_dump(phpiredis_command($link, 'GET a'));
?>
--EXPECTF--
string(1) "1"
string(2) "OK"
string(1) "1"
