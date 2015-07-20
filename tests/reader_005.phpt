--TEST--
[READER] Response types

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php

$type = -1;
$reader = phpiredis_reader_create();

phpiredis_reader_feed($reader, "+OK\r\n");
$reply = phpiredis_reader_get_reply($reader, $type);
if ($reply != 'OK') {
    echo "Value of reply '$reply' is not 'OK'\n";
}
if ($type != PHPIREDIS_REPLY_STATUS) {
    echo "Value of type '$type' != PHPIREDIS_REPLY_STATUS(".PHPIREDIS_REPLY_STATUS.")\n";
}

$reader = phpiredis_reader_create();
phpiredis_reader_feed($reader, "-ERR\r\n");
$reply = phpiredis_reader_get_reply($reader, $type);
if ($reply != 'ERR') {
    echo "Value of reply '$reply' is not 'ERR'\n";
}
if ($type != PHPIREDIS_REPLY_ERROR) {
    echo "Value of type '$type' != PHPIREDIS_REPLY_ERROR(".PHPIREDIS_REPLY_ERROR.")\n";
}
echo "OK\n";

--EXPECT--
OK

