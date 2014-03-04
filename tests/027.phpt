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

if (!function_exists('phpiredis_set_error_handler')) {
	printf("Function is not defined\n");
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

// do a faulty operation
phpiredis_command($link, 'DEL test');
phpiredis_command($link, 'SET test 1');
phpiredis_command($link, 'LLEN test');

$error_last = error_get_last();
if ($error_last != null) {
	printf("A php error was raised although a handler was set: %s\n", print_r($error_last, true));
}

if ($error_type_signaled != PHPIREDIS_ERROR_PROTOCOL) {
	printf("Wrong error type returned, was %d, should have been %d\n", $error_type_signaled, PHPIREDIS_ERROR_PROTOCOL);
}

// TODO: simulate a dead connection

// TODO: remove error handler and check error was properly raised

echo "OK" . PHP_EOL;
?>
--EXPECT--
OK
