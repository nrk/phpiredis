#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "lib/hiredis/hiredis.h"
#include "php.h"
#include "php_phpiredis.h"

static function_entry phpiredis_functions[] = {
    PHP_FE(hello_world, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry phpiredis_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_PHPIREDIS_WORLD_EXTNAME,
    phpiredis_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_PHPIREDIS_WORLD_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PHPIREDIS
ZEND_GET_MODULE(phpiredis)
#endif

PHP_FUNCTION(hello_world)
{
    redisContext *c;
    redisReply *reply;

    c = redisConnect((char*)"127.0.0.1", 6379);
    if (c->err) {
        printf("Connection error: %s\n", c->errstr);
        exit(1);
    }

    /* PING server */
    reply = redisCommand(c,"PING");
    //printf("PING: %s\n", reply->str);
    //freeReplyObject(reply);


    RETURN_STRING(reply->str, 1);
}
