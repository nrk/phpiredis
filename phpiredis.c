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
    PHP_FE(phpiredis_disconnect, NULL)
    PHP_FE(phpiredis_command, NULL)
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

PHP_FUNCTION(phpiredis_disconnect)
{
    zval *connection;
    redisContext *c;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &connection) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(c, redisContext *, &connection, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context);

    zend_list_delete(Z_LVAL_P(connection));

    RETURN_TRUE;
}


void convert_redis_to_php(zval* return_value, redisReply* reply) {
    //int type; /* REDIS_REPLY_* */
    //long long integer; /* The integer when type is REDIS_REPLY_INTEGER */
    //int len; /* Length of string */
    //char *str; /* Used for both REDIS_REPLY_ERROR and REDIS_REPLY_STRING */
    //size_t elements; /* number of elements, for REDIS_REPLY_ARRAY */
    //struct redisReply **element; /* elements vector for REDIS_REPLY_ARRAY */
    switch (reply->type) {
        case REDIS_REPLY_INTEGER:
            ZVAL_LONG(return_value, reply->integer);
            return;
        case REDIS_REPLY_STATUS:
        case REDIS_REPLY_STRING:
            ZVAL_STRINGL(return_value, reply->str, reply->len, 1);
            return;
        case REDIS_REPLY_ARRAY:
            array_init(return_value);
            int j;
            for (j = 0; j < reply->elements; j++) {
                add_index_string(return_value, j, reply->element[j]->str, 1);
            }
            return;
	case REDIS_REPLY_NIL:
            ZVAL_NULL(return_value);
            return;
    }
}

PHP_FUNCTION(phpiredis_command)
{
    zval *resource;
    redisReply *reply;
    phpiredis_connection *connection;
    char *command;
    int command_size;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &resource, &command, &command_size) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(connection, redisContext *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context);

    reply = redisCommand(connection->c,command);
    if (reply->type == REDIS_REPLY_ERROR) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, reply->str);
        RETURN_FALSE;
        return;
    }
    convert_redis_to_php(return_value, reply);
    freeReplyObject(reply);
}
