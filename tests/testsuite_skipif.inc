<?php

require_once 'testsuite_configuration.inc';

if (!extension_loaded("phpiredis")) {
    die("skip phpiredis extension is not available");
}
