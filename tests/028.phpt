--TEST--
Test error handler on connection failure
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
require_once('connect.inc');

$error_msg_signaled = '';
$error_type_signaled = 0;

// this error handler just sets the global variables
$callback = function($type, $msg) {
	printf("Error handler called!\n");

	global $error_type_signaled, $error_msg_signaled;
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

// close the connection right away
phpiredis_command($link, 'QUIT');
phpiredis_command($link, 'GET test');

$error_last = error_get_last();
if ($error_last != null) {
	printf("A php error was raised although a handler was set: %s\n", print_r($error_last, true));
}

if ($error_type_signaled != PHPIREDIS_ERROR_CONNECTION) {
	printf("Wrong error type returned, was %d, should have been %d\n", $error_type_signaled, PHPIREDIS_ERROR_CONNECTION);
}

// reset vars
$error_msg_signaled = '';
$error_type_signaled = 0;

// test phpiredis_command_bs
phpiredis_command_bs($link, array('GET', 'test'));

$error_last = error_get_last();
if ($error_last != null) {
	printf("A php error was raised although a handler was set: %s\n", print_r($error_last, true));
}

if ($error_type_signaled != PHPIREDIS_ERROR_CONNECTION) {
	printf("Wrong error type returned, was %d, should have been %d\n", $error_type_signaled, PHPIREDIS_ERROR_CONNECTION);
}

// reset vars
$error_msg_signaled = '';
$error_type_signaled = 0;

// remove error handler and check error was properly raised
phpiredis_set_error_handler($link, null);

@phpiredis_command($link, 'GET test');

$error_last = error_get_last();
if ($error_last == null) {
	printf("No error was raised although the error handler was removed.\n");
}

echo "OK" . PHP_EOL;
?>
--EXPECT--
Error handler called!
Error handler called!
OK
