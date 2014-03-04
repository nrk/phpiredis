--TEST--
Test per-connection error handlers
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
require_once('connect.inc');

if (!defined('PHPIREDIS_ERROR_CONNECTION') || !defined('PHPIREDIS_ERROR_PROTOCOL')) {
	printf("Constants are not defined\n");
}

$error_msg_signaled = '';
$error_type_signaled = 0;

// this error handler just sets the global variables
$callback = function($msg, $type) use ($error_msg_signaled, $error_type_signaled) {
	$error_msg_signaled = $msg;
	$error_type_signaled = $type;
};

$host = '127.0.0.1';
$link = my_phpiredis_connect($host);
if (!$link) {
	printf("Error connecting to the server using host=%s\n", $host);
}

if (!phpiredis_set_error_handler($link, $callback)) {
	printf("Error attaching error handler\n");
}

// TODO: do a faulty operation

if ($error_type_signaled != PHPIREDIS_ERROR_PROTOCOL) {
	printf("Wrong error type returned, was %d, should have been %d\n", $error_type_signaled, PHPIREDIS_ERROR_PROTOCOL);
}

// TODO: simulate a dead connection

echo "OK" . PHP_EOL;
?>
--EXPECT--
OK
