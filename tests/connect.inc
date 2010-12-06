<?php
if (!function_exists('my_phpiredis_connect')) {
        /* wrapper to simplify test porting */
        function my_phpiredis_connect($host, $port = 6379, $persistent = false) {
                global $connect_flags;

                if ($persistent) {
                        $link = phpiredis_pconnect($host, $port);
                } else {
                        $link = phpiredis_connect($host, $port);
                }

                if (!$link) {
                        printf("[000-a] Cannot connect using host '%s', on port [%d]\n",
                                $host, $port);
                        return false;
                }

                return $link;
        }
} else {
        printf("skip Eeeek/BUG/FIXME - connect.inc included twice! skipif bug?\n");
}

