--TEST--
phpiredis command formatting
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
var_dump(phpiredis_format_command(array('a', 's', 'd')));
?>
--EXPECTF--
string(25) "*3
$1
a
$1
s
$1
d
"
