#ifndef PHP_PHPIREDIS_H
#define PHP_PHPIREDIS_H 1

#define PHP_PHPIREDIS_VERSION "1.0"
#define PHP_PHPIREDIS_EXTNAME "phpiredis"

PHP_MINIT_FUNCTION(phpiredis);
PHP_FUNCTION(phpiredis_connect);

extern zend_module_entry phpiredis_module_entry;
#define phpext_phpiredis_ptr &phpiredis_module_entry

#endif
