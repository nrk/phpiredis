<?php

require_once 'testsuite_configuration.inc';

if (!function_exists('create_phpiredis_connection')) {
    function create_phpiredis_connection($host, $port = 6379, $persistent = false) {
        global $connect_flags;

        if ($persistent) {
            $link = phpiredis_pconnect($host, $port);
        } else {
            $link = phpiredis_connect($host, $port);
        }

        if (!$link) {
            printf("Cannot connect to host '%s' on port [%d]\n", $host, $port);

            return false;
        }

        return $link;
    }
} else {
    printf("skip Eeeek/BUG/FIXME - connect.inc included twice! skipif bug?\n");
}
