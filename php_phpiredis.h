#ifndef PHP_PHPIREDIS_H
#define PHP_PHPIREDIS_H 1

#define PHP_PHPIREDIS_WORLD_VERSION "1.0"
#define PHP_PHPIREDIS_WORLD_EXTNAME "phpiredis"

PHP_FUNCTION(hello_world);

extern zend_module_entry phpiredis_module_entry;
#define phpext_phpiredis_ptr &phpiredis_module_entry

#endif
