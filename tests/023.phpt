--TEST--
format parameters are not modified
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
$array = array('SET', 'key', 1);
var_dump(phpiredis_format_command($array));
var_dump($array);
?>
--EXPECTF--
string(29) "*3
$3
SET
$3
key
$1
1
"
array(3) {
  [0]=>
  string(3) "SET"
  [1]=>
  string(3) "key"
  [2]=>
  int(1)
}

