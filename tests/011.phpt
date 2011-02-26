--TEST--
phpiredis create and destroy reader
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
$reader = phpiredis_reader_create();
var_dump($reader);
phpiredis_reader_destroy($reader);
?>
--EXPECTF--
resource(%d) of type (phpredis reader)
