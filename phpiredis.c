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
    PHP_FE(phpiredis_pconnect, NULL)
    PHP_FE(phpiredis_disconnect, NULL)
    PHP_FE(phpiredis_command, NULL)
    PHP_FE(phpiredis_multi_command, NULL)
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
#define PHPIREDIS_PERSISTENT_CONNECTION_NAME "phpredis connection persistent"

static void php_redis_connection_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    phpiredis_connection *connection = (phpiredis_connection*)rsrc->ptr;

    if (connection) {
        redisFree(connection->c);
    }
}

static void php_redis_connection_persist(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    phpiredis_connection *connection = (phpiredis_connection*)rsrc->ptr;

    if (connection) {
       redisFree(connection->c);
    }
}


int le_redis_context;
int le_redis_persistent_context;
PHP_MINIT_FUNCTION(phpiredis)
{
    le_redis_context = zend_register_list_destructors_ex(php_redis_connection_dtor, NULL, PHPIREDIS_CONNECTION_NAME, module_number);
    le_redis_persistent_context = zend_register_list_destructors_ex(NULL, php_redis_connection_persist, PHPIREDIS_PERSISTENT_CONNECTION_NAME, module_number);
    return SUCCESS;
}

PHP_FUNCTION(phpiredis_pconnect)
{
    char *ip;
    int ip_size;
    long port = 6379;

    char *hashed_details=NULL;
    int hashed_details_length;
    phpiredis_connection *connection;

    zend_rsrc_list_entry *le;


    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &ip, &ip_size, &port) == FAILURE) {
        return;
    }

    hashed_details_length = spprintf(&hashed_details, 0, "phpiredis_%s_%d", ip, (int)port);

    if (zend_hash_find(&EG(persistent_list), hashed_details, hashed_details_length+1, (void **) &le)!=FAILURE) {
        if (Z_TYPE_P(le) != le_redis_persistent_context) {
            RETURN_FALSE;
        }

        connection = (phpiredis_connection *) le->ptr;
        ZEND_REGISTER_RESOURCE(return_value, connection, le_redis_persistent_context);
        // TODO: ping and reconnect
        efree(hashed_details);
        return;
    }

    redisContext *c;
    c = redisConnect(ip, (int)port); // FIXME: unsafe cast

    if (c->err) {
        redisFree(c);
        RETURN_FALSE;
    }

    connection = emalloc(sizeof(phpiredis_connection));
    connection->c = c;

    zend_rsrc_list_entry new_le;
    new_le.type = le_redis_persistent_context;
    new_le.ptr = connection;
    if (zend_hash_update(&EG(persistent_list), hashed_details, hashed_details_length+1, (void *) &new_le, sizeof(zend_rsrc_list_entry), NULL)==FAILURE) {
        efree(connection);
        efree(hashed_details);
        RETURN_FALSE;
    }

    efree(hashed_details);
    ZEND_REGISTER_RESOURCE(return_value, connection, le_redis_persistent_context);
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

    ZEND_FETCH_RESOURCE2(c, redisContext *, &connection, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

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
                add_index_stringl(return_value, j, reply->element[j]->str, reply->element[j]->len, 1);
            }
            return;
        case REDIS_REPLY_NIL:
        default:
            ZVAL_NULL(return_value);
            return;
    }
}

PHP_FUNCTION(phpiredis_multi_command)
{
    zval         **tmp;
    HashPosition   pos;
    zval *resource;
    redisReply *reply;
    phpiredis_connection *connection;
    zval *arr;
    zval **arg2;
    int commands;
    int i;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rZ", &resource, &arg2) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE2(connection, redisContext *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    arr = *arg2;
    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(arr), &pos);

    commands = 0;
    while (zend_hash_get_current_data_ex(Z_ARRVAL_P(arr), (void **) &tmp, &pos) == SUCCESS) {
        switch ((*tmp)->type) {
                case IS_STRING:
                        ++commands;
                        redisAppendCommand(connection->c,Z_STRVAL_PP(tmp));
                        break;

                case IS_OBJECT: {
                        ++commands;
                        int copy;
                        zval expr;
                        zend_make_printable_zval(*tmp, &expr, &copy);
                        redisAppendCommand(connection->c,Z_STRVAL(expr));
                        if (copy) {
                                zval_dtor(&expr);
                        }
                }
                        break;

                default:
                        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Array argument must contain strings");
                        break;
        }
        zend_hash_move_forward_ex(Z_ARRVAL_P(arr), &pos);
    }

    array_init(return_value);
    for (i = 0; i < commands; ++i) {
        zval* result;
        MAKE_STD_ZVAL(result);
        redisReply *reply;
        redisGetReply(connection->c,&reply);
        convert_redis_to_php(result, reply);
        add_index_zval(return_value, i, result);
        freeReplyObject(reply);
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

    ZEND_FETCH_RESOURCE2(connection, redisContext *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    reply = redisCommand(connection->c,command);
    if (reply->type == REDIS_REPLY_ERROR) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, reply->str);
        RETURN_FALSE;
        return;
    }
    convert_redis_to_php(return_value, reply);
    freeReplyObject(reply);
}
