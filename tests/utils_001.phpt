--TEST--
[UTILS] Calculate the CRC16 of strings

--SKIPIF--
<?php
require_once 'testsuite_skipif.inc';

--FILE--
<?php
var_dump(phpiredis_utils_crc16('key:000'));
var_dump(phpiredis_utils_crc16('key:001'));
var_dump(phpiredis_utils_crc16('key:002'));
var_dump(phpiredis_utils_crc16('key:003'));
var_dump(phpiredis_utils_crc16('key:004'));
var_dump(phpiredis_utils_crc16('key:005'));
var_dump(phpiredis_utils_crc16('key:006'));
var_dump(phpiredis_utils_crc16('key:007'));
var_dump(phpiredis_utils_crc16('key:008'));
var_dump(phpiredis_utils_crc16('key:009'));

--EXPECTF--
int(58359)
int(62422)
int(50101)
int(54164)
int(41843)
int(45906)
int(33585)
int(37648)
int(25343)
int(29406)
