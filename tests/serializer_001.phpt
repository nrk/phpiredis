--TEST--
[SERIALIZER] Command serialization

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
$response = phpiredis_format_command(array('a', 's', 'd'));
var_dump($response);

--EXPECT--
string(25) "*3
$1
a
$1
s
$1
d
"
