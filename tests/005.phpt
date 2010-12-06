--TEST--
phpiredis multicommand
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

phpiredis_command($link, 'DEL test');
phpiredis_command($link, 'SET test 1');
$command = phpiredis_command($link, 'DEL test');
var_dump($command);
?>
--EXPECTF--
int(1)
