--TEST--
phpiredis command formatting
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
var_dump(phpiredis_format_command(array('SET', 'key', 112)));
var_dump(phpiredis_format_command(array('SET', 'key', 1.2)));
?>
--EXPECTF--
string(31) "*3
$3
SET
$3
key
$3
112
"
string(31) "*3
$3
SET
$3
key
$3
1.2
"
