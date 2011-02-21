--TEST--
phpiredis reconnect on disconnect
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
var_dump(phpiredis_format_command(array('a', 's', 'd')));
?>
--EXPECTF--
array(3) {
  [0]=>
  string(1) "1"
  [1]=>
  string(2) "OK"
  [2]=>
  bool(false)
}
