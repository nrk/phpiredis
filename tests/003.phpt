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

$commands = phpiredis_multi_command($link, array('DEL test', 'SET test 1', 'GET test'));
var_dump($commands[2]);
?>
--EXPECTF--
string(1) "1"

