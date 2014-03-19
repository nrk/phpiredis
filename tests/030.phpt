--TEST--
Test error handler on connection failure with multi method
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
require_once('connect.inc');

$error_msg_signaled = array();
$error_type_signaled = array();

// this error handler just sets the global variables
$callback = function($type, $msg) {
	printf("Error handler called!\n");

	global $error_type_signaled, $error_msg_signaled;
	$error_msg_signaled[] = $msg;
	$error_type_signaled[] = $type;
};

$host = '127.0.0.1';
$link = my_phpiredis_connect($host);
if (!$link) {
	printf("Error connecting to the server using host=%s\n", $host);
}

if (!phpiredis_set_error_handler($link, $callback)) {
	printf("Error attaching error handler\n");
}

$commands = array(
	'QUIT',
	'GET test'
);

$expected_result = array(
	'OK',
	false
);

$result = phpiredis_multi_command($link, $commands);

$error_last = error_get_last();
if ($error_last != null) {
	printf("A php error was raised although a handler was set: %s\n", print_r($error_last, true));
}

if (count($error_msg_signaled) != 1 || count($error_type_signaled) != 1) {
	printf("The wrong number of errors was raised: %d\n", count($error_msg_signaled));
}

if ($error_type_signaled[0] != PHPIREDIS_ERROR_CONNECTION) {
	printf("Wrong error type returned, was %d, should have been %d\n", $error_type_signaled[0], PHPIREDIS_ERROR_CONNECTION);
}

if (array_diff_assoc($result, $expected_result) !== array()) {
	printf("The actual result does not match the expected result. Actual: %s\n", print_r($result, true));
}

echo "OK" . PHP_EOL;
?>
--EXPECT--
Error handler called!
OK
