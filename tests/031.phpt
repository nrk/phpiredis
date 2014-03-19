--TEST--
Test that no segfault occurs when attaching and removing an error handler
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
require_once('connect.inc');

$mem = 0;
$callback = function($type, $msg) {
	printf("Error handler called!\n");
};

$host = '127.0.0.1';
$link = my_phpiredis_connect($host);
if (!$link) {
	printf("Error connecting to the server using host=%s\n", $host);
}

if (!phpiredis_set_error_handler($link, $callback)) {
	printf("Error attaching error handler\n");
}

// here we should still be able to call it
$callback(1, 'test');

// now we remove it but have to make sure it still exists
phpiredis_set_error_handler($link, null);

$callback(1, 'test');

// check for leaking memory
$mem = memory_get_usage() - $mem;

phpiredis_set_error_handler($link, $callback);
phpiredis_set_error_handler($link, null);

$mem = memory_get_usage() - $mem;

if (abs($mem) != 0) {
	printf("Memory footprint was: %d\n", $mem);
}

echo "OK" . PHP_EOL;
?>
--EXPECT--
Error handler called!
Error handler called!
OK
