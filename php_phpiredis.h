#ifndef PHP_PHPIREDIS_H
#define PHP_PHPIREDIS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"

#define PHP_PHPIREDIS_VERSION "1.0.0"
#define PHP_PHPIREDIS_EXTNAME "phpiredis"

PHP_MINIT_FUNCTION(phpiredis);
PHP_FUNCTION(phpiredis_connect);
PHP_FUNCTION(phpiredis_create_from_stream);
PHP_FUNCTION(phpiredis_pconnect);
PHP_FUNCTION(phpiredis_disconnect);
PHP_FUNCTION(phpiredis_command_bs);
PHP_FUNCTION(phpiredis_command);
PHP_FUNCTION(phpiredis_multi_command);
PHP_FUNCTION(phpiredis_multi_command_bs);
PHP_FUNCTION(phpiredis_format_command);
PHP_FUNCTION(phpiredis_read_reply);
PHP_FUNCTION(phpiredis_reader_create);
PHP_FUNCTION(phpiredis_reader_reset);
PHP_FUNCTION(phpiredis_reader_feed);
PHP_FUNCTION(phpiredis_reader_get_state);
PHP_FUNCTION(phpiredis_reader_get_error);
PHP_FUNCTION(phpiredis_reader_get_reply);
PHP_FUNCTION(phpiredis_reader_destroy);
PHP_FUNCTION(phpiredis_reader_set_error_handler);
PHP_FUNCTION(phpiredis_reader_set_status_handler);

extern zend_module_entry phpiredis_module_entry;
#define phpext_phpiredis_ptr &phpiredis_module_entry

#endif
