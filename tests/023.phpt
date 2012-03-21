--TEST--
phpiredis connect
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
phpiredis_format_command('SET a 1');
?>
--EXPECTF--
Warning: phpiredis_format_command() expects parameter 1 to be array, string given in %s
