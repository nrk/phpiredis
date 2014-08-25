--TEST--
Test pipelined commands, through append_command and read_reply change
--SKIPIF--
<?php include 'skipif.inc'; ?>
--FILE--
<?php
require_once('connect.inc');
$test = '';

$host = '127.0.0.1';
$port = '6379';

$stream = @stream_socket_client('tcp://'.$host.':'.$port);
if (!$stream) {
        echo 'Failed to connect to host';
}

$link = phpiredis_create_from_stream($stream);

phpiredis_append_command($link, array('del', 'a'));
phpiredis_append_command($link, array('set', 'a', 'b'));
phpiredis_append_command($link, array('get', 'a'));

echo serialize(phpiredis_read_reply($link)).PHP_EOL;
echo serialize(phpiredis_read_reply($link)).PHP_EOL;
echo serialize(phpiredis_read_reply($link)).PHP_EOL;

?>
--EXPECT--
i:1;
s:2:"OK";
s:1:"b";

