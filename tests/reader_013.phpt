--TEST--
[READER] Parameters formatting should not break on many arguments

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
$array = array('MSET', 'key1', '1', 'key2', '2', 'key3', '3', 'key4', '4', 'key5', '5');

var_dump(phpiredis_format_command($array));
var_dump($array);

--EXPECT--
string(100) "*11
$4
MSET
$4
key1
$1
1
$4
key2
$1
2
$4
key3
$1
3
$4
key4
$1
4
$4
key5
$1
5
"
array(11) {
  [0]=>
  string(4) "MSET"
  [1]=>
  string(4) "key1"
  [2]=>
  string(1) "1"
  [3]=>
  string(4) "key2"
  [4]=>
  string(1) "2"
  [5]=>
  string(4) "key3"
  [6]=>
  string(1) "3"
  [7]=>
  string(4) "key4"
  [8]=>
  string(1) "4"
  [9]=>
  string(4) "key5"
  [10]=>
  string(1) "5"
}
