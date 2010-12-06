--TEST--
phpiredis array
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

$commands = phpiredis_multi_command($link, array('DEL test', 'LPUSH test 1', 'LPUSH test 2', 'LPUSH test 3', 'LRANGE test 0 -1'));
var_dump($commands[4]);
?>
--EXPECTF--
array(3) {
  [0]=>
  string(1) "3"
  [1]=>
  string(1) "2"
  [2]=>
  string(1) "1"
}
