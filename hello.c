#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "lib/hiredis/hiredis.h"
#include "php.h"
#include "php_hello.h"

static function_entry hello_functions[] = {
    PHP_FE(hello_world, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry hello_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_HELLO_WORLD_EXTNAME,
    hello_functions,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_HELLO_WORLD_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_HELLO
ZEND_GET_MODULE(hello)
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
