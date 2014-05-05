--TEST--
Test error handler for multi commands
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
	'DEL test',
	'SET test 2',
	'LLEN test',
	'GET test',
	'SMEMBERS test'
);

$expected_result = array(
	1,
	'OK',
	'WRONGTYPE Operation against a key holding the wrong kind of value',
	2,
	'WRONGTYPE Operation against a key holding the wrong kind of value'
);

$result = phpiredis_multi_command($link, $commands);

$error_last = error_get_last();
if ($error_last != null) {
	printf("A php error was raised although a handler was set: %s\n", print_r($error_last, true));
}

if (count($error_msg_signaled) != 2 || count($error_type_signaled) != 2) {
	printf("The wrong number of errors was raised: %d\n", count($error_msg_signaled));
}

if ($error_type_signaled[0] != PHPIREDIS_ERROR_PROTOCOL) {
	printf("Wrong error type returned, was %d, should have been %d\n", $error_type_signaled[0], PHPIREDIS_ERROR_PROTOCOL);
}

if (substr($error_msg_signaled[0], 0, 9) != 'WRONGTYPE') {
	printf("Wrong error message returned, was %s, should have started with %s\n", $error_msg_signaled[0], 'WRONGTYPE');
}

if (array_diff_assoc($result, $expected_result) !== array()) {
	printf("The actual result does not match the expected result. Actual: %s\n", print_r($result, true));
}

// reset vars
$error_msg_signaled = array();
$error_type_signaled = array();

// transform command array
array_walk($commands, function(&$value, $index) {
	$value = explode(' ', $value);
});

// test phpiredis_command_bs
phpiredis_multi_command_bs($link, $commands);

$error_last = error_get_last();
if ($error_last != null) {
	printf("A php error was raised although a handler was set: %s\n", print_r($error_last, true));
}

if (count($error_msg_signaled) != 2 || count($error_type_signaled) != 2) {
	printf("The wrong number of errors was raised: %d\n", count($error_msg_signaled));
}

if ($error_type_signaled[0] != PHPIREDIS_ERROR_PROTOCOL) {
	printf("Wrong error type returned, was %d, should have been %d\n", $error_type_signaled[0], PHPIREDIS_ERROR_PROTOCOL);
}

if (substr($error_msg_signaled[0], 0, 9) != 'WRONGTYPE') {
	printf("Wrong error message returned, was %s, should have started with %s\n", $error_msg_signaled[0], 'WRONGTYPE');
}

if (array_diff_assoc($result, $expected_result) !== array()) {
	printf("The actual result does not match the expected result. Actual: %s\n", print_r($result, true));
}

// remove error handler and check error was properly raised
phpiredis_set_error_handler($link, null);

@phpiredis_multi_command_bs($link, $commands);

$error_last = error_get_last();
if ($error_last == null) {
	printf("No error was raised although the error handler was removed.\n");
}

echo "OK" . PHP_EOL;
?>
--EXPECT--
Error handler called!
Error handler called!
Error handler called!
Error handler called!
OK
