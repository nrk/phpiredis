--TEST--
[READER] Set custom error handler

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php

$expectedErrorText = 'ERR after handler';


function error_handler($err) {
    return $err." after handler";
}

$reader = phpiredis_reader_create();

phpiredis_reader_feed($reader, "-ERR\r\n");
$reply = phpiredis_reader_get_reply($reader);
if ($reply !== 'ERR') {
    echo "Reply '$reply' != 'ERR' on line ".__LINE__." \n";
}

phpiredis_reader_set_error_handler($reader, 'error_handler');
phpiredis_reader_feed($reader, "-ERR\r\n");
$reply = phpiredis_reader_get_reply($reader);
if ($reply != $expectedErrorText) {
    echo "Reply '$reply' != '$expectedErrorText' on line ".__LINE__." \n";
}

$failResult = @phpiredis_reader_set_error_handler($reader, 'fake_error_handler');
if ($failResult !== FALSE) {
    echo "Did not get FALSE when creating a fake_error_handler \n";
}

phpiredis_reader_feed($reader, "-ERR\r\n");
$reply = phpiredis_reader_get_reply($reader);
if ($reply != $expectedErrorText) {
    echo "Reply '$reply' is not equal to '$expectedErrorText' on line ".__LINE__." \n";
}

$result = phpiredis_reader_set_error_handler($reader, NULL);
if ($result !== TRUE) {
    echo "Result '$result' is not true when setting NULL error handler'";
}

phpiredis_reader_feed($reader, "-ERR\r\n");
$reply = phpiredis_reader_get_reply($reader);
if ($reply !== 'ERR') {
    echo "Reply '$reply' is not equal to 'ERR' on line ".__LINE__." \n";
}

--EXPECT--
