#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "lib/hiredis/hiredis.h"
#include "php.h"
#include "php_ini.h"
#include "php_phpiredis.h"
#include "ext/standard/head.h"
#include "ext/standard/info.h"
#include "php_phpiredis_struct.h"

static function_entry phpiredis_functions[] = {
    PHP_FE(phpiredis_connect, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry phpiredis_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_PHPIREDIS_EXTNAME,
    phpiredis_functions,
    PHP_MINIT(phpiredis),
    NULL,
    NULL,
    NULL,
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_PHPIREDIS_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PHPIREDIS
ZEND_GET_MODULE(phpiredis)
#endif

#define PHPIREDIS_CONNECTION_NAME "phpredis connection"

static void php_redis_connection_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    phpiredis_connection *connection = (phpiredis_connection*)rsrc->ptr;

    if (connection) {
       redisFree(connection->c);
    }
}

int le_redis_context;
PHP_MINIT_FUNCTION(phpiredis)
{
    le_redis_context = zend_register_list_destructors_ex(php_redis_connection_dtor, NULL, PHPIREDIS_CONNECTION_NAME, module_number);
    return SUCCESS;
}

PHP_FUNCTION(phpiredis_connect)
{
    char *ip;
    int ip_size;
    long port = 6379;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &ip, &ip_size, &port) == FAILURE) {
        return;
    }

    redisContext *c;
    c = redisConnect(ip, (int)port); // FIXME: unsafe cast

    if (c->err) {
        redisFree(c);
        RETURN_FALSE;
    }

    phpiredis_connection *connection = emalloc(sizeof(phpiredis_connection));
    connection->c = c;
    ZEND_REGISTER_RESOURCE(return_value, connection, le_redis_context);
}

PHP_FUNCTION(phpiredis_get)
{
    /* PING server * /
    reply = redisCommand(c,"PING");
    char* ret = emalloc(sizeof(char) * reply->len);
    memcpy(ret, reply->str, reply->len);
    //printf("PING: %s\n", reply->str);
    freeReplyObject(reply);


    RETURN_STRING(reply->str, 1);
*/
}
