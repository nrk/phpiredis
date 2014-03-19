#include "hiredis/hiredis.h"
#include "php_phpiredis.h"
#include "php_phpiredis_struct.h"

#include "ext/standard/head.h"
#include "ext/standard/info.h"

#define PHPIREDIS_CONNECTION_NAME "phpredis connection"
#define PHPIREDIS_PERSISTENT_CONNECTION_NAME "phpredis connection persistent"
#define PHPIREDIS_READER_NAME "phpredis reader"

int le_redis_reader_context;
int le_redis_context;
int le_redis_persistent_context;

typedef struct callback {
    zval *function;
} callback;

static
void convert_redis_to_php(phpiredis_reader* reader, zval* return_value, redisReply* reply TSRMLS_DC);

static
void free_error_callback(phpiredis_connection *connection TSRMLS_DC)
{
    if (connection->error_callback) {
        efree(((callback*) connection->error_callback)->function);
        efree(connection->error_callback);
        connection->error_callback = NULL;
    }
}

static
void s_destroy_connection(phpiredis_connection *connection TSRMLS_DC)
{
    if (connection) {
        pefree(connection->ip, connection->is_persistent);
        if (connection->c != NULL) {
            redisFree(connection->c);
        }
        if (connection->error_callback != NULL) {
            free_error_callback(connection TSRMLS_CC);
        }
        pefree(connection, connection->is_persistent);
    }
}

static
void php_redis_connection_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    phpiredis_connection *connection = (phpiredis_connection*) rsrc->ptr;
    s_destroy_connection(connection TSRMLS_CC);
}

static
void php_redis_reader_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    phpiredis_reader *_reader = (void *) rsrc->ptr;

    if (_reader) {
        if (_reader->bufferedReply != NULL) {
            freeReplyObject(_reader->bufferedReply);
        }

        if (_reader->reader != NULL) {
            redisReplyReaderFree(_reader->reader);
        }

        if (_reader->error_callback != NULL) {
            efree(((callback*) _reader->error_callback)->function);
            efree(_reader->error_callback);
        }

        if (_reader->status_callback != NULL) {
            efree(((callback*) _reader->status_callback)->function);
            efree(_reader->status_callback);
        }

        efree(_reader);
    }
}

static
phpiredis_connection *s_create_connection (const char *ip, int port, zend_bool is_persistent)
{
    redisContext *c;
    phpiredis_connection *connection;

    c = redisConnect(ip, port);

    if (!c || c->err) {
        redisFree(c);
        return NULL;
    }

    connection                 = pemalloc(sizeof(phpiredis_connection), is_persistent);
    connection->c              = c;
    connection->ip             = pestrdup(ip, is_persistent);
    connection->port           = port;
    connection->is_persistent  = is_persistent;
    connection->error_callback = NULL;

    return connection;
}

static
int handle_error_callback(phpiredis_connection *connection, int type, char *msg, int len TSRMLS_DC) {
    if (connection->error_callback == NULL) {
        // raise PHP error if no handler is set
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", msg);
        return SUCCESS;
    }

    zval *arg[2];
    zval *return_value;
    int retval = SUCCESS;

    MAKE_STD_ZVAL(arg[0]);
    ZVAL_LONG(arg[0], type);
    MAKE_STD_ZVAL(arg[1]);

    // only set second argument when msg is given
    if (msg != NULL && len > 0) {
        ZVAL_STRINGL(arg[1], msg, len, 1);
    }

    MAKE_STD_ZVAL(return_value);

    if (call_user_function(EG(function_table), NULL, ((callback*) connection->error_callback)->function, return_value, 2, arg TSRMLS_CC) == FAILURE) {
        // return FAILURE to signal something went wrong with the error handler
        retval = FAILURE;
    }

    // TODO: we could also return whatever the error handler returned to allow for more flexibility
    zval_ptr_dtor(&return_value);
    zval_ptr_dtor(&arg[0]);
    zval_ptr_dtor(&arg[1]);

    // return SUCCESS to signal successful execution of the error handler
    return retval;
}

PHP_FUNCTION(phpiredis_connect)
{
    phpiredis_connection *connection;
    char *ip;
    int ip_size;
    long port = 6379;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &ip, &ip_size, &port) == FAILURE) {
        return;
    }

    connection = s_create_connection(ip, port, 0);

    if (!connection) {
        RETURN_FALSE;
    }

    ZEND_REGISTER_RESOURCE(return_value, connection, le_redis_context);
}

PHP_FUNCTION(phpiredis_pconnect)
{
    char *ip;
    int ip_size;
    long port = 6379;

    char *hashed_details = NULL;
    int hashed_details_length;
    phpiredis_connection *connection;

    zend_rsrc_list_entry new_le, *le;

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
        efree(hashed_details);

        return;
    }

    connection = s_create_connection(ip, port, 1);

    if (!connection) {
        efree(hashed_details);
        RETURN_FALSE;
    }

    new_le.type = le_redis_persistent_context;
    new_le.ptr = connection;

    if (zend_hash_update(&EG(persistent_list), hashed_details, hashed_details_length+1, (void *) &new_le, sizeof(zend_rsrc_list_entry), NULL)==FAILURE) {
        s_destroy_connection (connection TSRMLS_CC);
        efree(hashed_details);

        RETURN_FALSE;
    }

    efree(hashed_details);

    ZEND_REGISTER_RESOURCE(return_value, connection, le_redis_persistent_context);
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

PHP_FUNCTION(phpiredis_set_error_handler)
{
    zval *ptr, **function;
    phpiredis_connection *connection;
    char *name;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rZ", &ptr, &function) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &ptr, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    if ((*function)->type == IS_NULL) {
        free_error_callback(connection TSRMLS_CC);
    } else {
        if (!zend_is_callable(*function, 0, &name TSRMLS_CC)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument is not a valid callback");
            efree(name);
            RETURN_FALSE;
        }

        efree(name);
        free_error_callback(connection TSRMLS_CC);

        connection->error_callback = emalloc(sizeof(callback));

        Z_ADDREF_PP(function);
        ((callback*) connection->error_callback)->function = *function;
    }

    RETURN_TRUE;
}

PHP_FUNCTION(phpiredis_multi_command)
{
    zval **tmp;
    HashPosition pos;
    zval *resource;
    phpiredis_connection *connection;
    zval *arr;
    zval **arg2;
    int commands;
    int i;
    zval temp;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rZ", &resource, &arg2) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    arr = *arg2;
    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(arr), &pos);

    commands = 0;
    while (zend_hash_get_current_data_ex(Z_ARRVAL_P(arr), (void **) &tmp, &pos) == SUCCESS) {
        temp = **tmp;
        zval_copy_ctor(&temp);
        convert_to_string(&temp);
        ++commands;
        redisAppendCommand(connection->c, Z_STRVAL(temp));
        zend_hash_move_forward_ex(Z_ARRVAL_P(arr), &pos);
        zval_dtor(&temp);
    }

    array_init(return_value);
    for (i = 0; i < commands; ++i) {
        redisReply *reply = NULL;
        zval* result;
        MAKE_STD_ZVAL(result);

        if (redisGetReply(connection->c, &reply) != REDIS_OK) {
            for (; i < commands; ++i) {
                add_index_bool(return_value, i, 0);
            }

            handle_error_callback(connection, PHPIREDIS_ERROR_CONNECTION, connection->c->errstr, strlen(connection->c->errstr) TSRMLS_CC);
            if (reply) freeReplyObject(reply);

            zval_ptr_dtor(result);
            break;
        }

        if (reply->type == REDIS_REPLY_ERROR) {
            handle_error_callback(connection, PHPIREDIS_ERROR_PROTOCOL, reply->str, reply->len TSRMLS_CC);
        }

        convert_redis_to_php(NULL, result, reply TSRMLS_CC);
        add_index_zval(return_value, i, result);
        freeReplyObject(reply);
    }
}

PHP_FUNCTION(phpiredis_multi_command_bs)
{
    zval **tmp;
    zval **tmpArg;
    HashPosition cmdsPos;
    HashPosition cmdArgsPos;
    zval *resource;
    phpiredis_connection *connection;
    zval *cmds;
    zval cmdArgs;
    zval cmdArg;

    int cmdPos;
    int cmdSize;
    char **cmdElements;
    size_t *cmdElementslen;

    int commands;
    int i;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ra", &resource, &cmds) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    commands = 0;
    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(cmds), &cmdsPos);

    while (zend_hash_get_current_data_ex(Z_ARRVAL_P(cmds), (void **) &tmp, &cmdsPos) == SUCCESS) {
        cmdArgs = **tmp;
        zval_copy_ctor(&cmdArgs);

        cmdPos = 0;
        cmdSize = zend_hash_num_elements(Z_ARRVAL_P(&cmdArgs));
        cmdElements = emalloc(sizeof(char*) * cmdSize);
        cmdElementslen = emalloc(sizeof(size_t) * cmdSize);

        zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(&cmdArgs), &cmdArgsPos);
        while (zend_hash_get_current_data_ex(Z_ARRVAL_P(&cmdArgs), (void **) &tmpArg, &cmdArgsPos) == SUCCESS) {
            cmdArg = **tmpArg;
            zval_copy_ctor(&cmdArg);
            convert_to_string(&cmdArg);

            cmdElementslen[cmdPos] = (size_t) Z_STRLEN(cmdArg);
            cmdElements[cmdPos] = emalloc(sizeof(char) * cmdElementslen[cmdPos]);
            memcpy(cmdElements[cmdPos], Z_STRVAL(cmdArg), cmdElementslen[cmdPos]);

            zval_dtor(&cmdArg);
            zend_hash_move_forward_ex(Z_ARRVAL_P(&cmdArgs), &cmdArgsPos);
            ++cmdPos;
        }

        redisAppendCommandArgv(connection->c, cmdSize, cmdElements, cmdElementslen);

        for (; cmdPos > 0; --cmdPos) {
           efree(cmdElements[cmdPos-1]);
        }
        efree(cmdElements);
        efree(cmdElementslen);

        zend_hash_move_forward_ex(Z_ARRVAL_P(cmds), &cmdsPos);
        zval_dtor(&cmdArgs);
        ++commands;
    }

    array_init(return_value);
    for (i = 0; i < commands; ++i) {
        redisReply *reply = NULL;
        zval* result;
        MAKE_STD_ZVAL(result);

        if (redisGetReply(connection->c, &reply) != REDIS_OK) {
            for (; i < commands; ++i) {
                add_index_bool(return_value, i, 0);
            }

            handle_error_callback(connection, PHPIREDIS_ERROR_CONNECTION, connection->c->errstr, strlen(connection->c->errstr) TSRMLS_CC);
            if (reply) freeReplyObject(reply);

            zval_ptr_dtor(result);
            break;
        }

        if (reply->type == REDIS_REPLY_ERROR) {
            handle_error_callback(connection, PHPIREDIS_ERROR_PROTOCOL, reply->str, reply->len TSRMLS_CC);
        }

        convert_redis_to_php(NULL, result, reply TSRMLS_CC);
        add_index_zval(return_value, i, result);
        freeReplyObject(reply);
    }
}

PHP_FUNCTION(phpiredis_command)
{
    zval *resource;
    redisReply *reply = NULL;
    phpiredis_connection *connection;
    char *command;
    int command_size;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &resource, &command, &command_size) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    reply = redisCommand(connection->c, command);

    if (reply == NULL) {
        handle_error_callback(connection, PHPIREDIS_ERROR_CONNECTION, connection->c->errstr, strlen(connection->c->errstr) TSRMLS_CC);

        RETURN_FALSE;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        handle_error_callback(connection, PHPIREDIS_ERROR_PROTOCOL, reply->str, reply->len TSRMLS_CC);
        freeReplyObject(reply);

        RETURN_FALSE;
    }

    convert_redis_to_php(NULL, return_value, reply TSRMLS_CC);
    freeReplyObject(reply);
}

PHP_FUNCTION(phpiredis_command_bs)
{
    zval *resource;
    redisReply *reply = NULL;
    phpiredis_connection *connection;
    zval *params;
    int argc;
    char ** argv;
    size_t * argvlen;
    zval **tmp;
    HashPosition pos;
    int i;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ra", &resource, &params) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    argc = zend_hash_num_elements(Z_ARRVAL_P(params));
    argvlen = emalloc(sizeof(size_t) * argc);
    argv = emalloc(sizeof(char*) * argc);

    i = 0;
    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(params), &pos);

    while (zend_hash_get_current_data_ex(Z_ARRVAL_P(params), (void **) &tmp, &pos) == SUCCESS) {
        switch ((*tmp)->type) {
                case IS_STRING: {
                        argvlen[i] = (size_t) Z_STRLEN_PP(tmp);
                        argv[i] = emalloc(sizeof(char) * argvlen[i]);
                        memcpy(argv[i], Z_STRVAL_PP(tmp), argvlen[i]);
                    }
                    break;

                case IS_OBJECT: {
                        int copy;
                        zval expr;
                        zend_make_printable_zval(*tmp, &expr, &copy);
                        argvlen[i] = Z_STRLEN(expr);
                        argv[i] = emalloc(sizeof(char) * argvlen[i]);
                        memcpy(argv[i], Z_STRVAL(expr), argvlen[i]);
                        if (copy) {
                            zval_dtor(&expr);
                        }
                    }
                    break;

                default:
                    php_error_docref(NULL TSRMLS_CC, E_WARNING, "Array argument must contain strings");
                    break;
        }

        zend_hash_move_forward_ex(Z_ARRVAL_P(params), &pos);
        ++i;
    }

    redisAppendCommandArgv(connection->c, argc, argv, (const size_t *) argvlen);

    for (i = 0; i < argc; i++) {
        efree(argv[i]);
    }
    efree(argv);
    efree(argvlen);

    if (redisGetReply(connection->c, &reply) != REDIS_OK) {
        handle_error_callback(connection, PHPIREDIS_ERROR_CONNECTION, connection->c->errstr, strlen(connection->c->errstr) TSRMLS_CC);
        // only free if the reply was actually created
        if (reply) freeReplyObject(reply);

        RETURN_FALSE;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        handle_error_callback(connection, PHPIREDIS_ERROR_PROTOCOL, reply->str, reply->len TSRMLS_CC);
        freeReplyObject(reply);

        RETURN_FALSE;
    }

    convert_redis_to_php(NULL, return_value, reply TSRMLS_CC);
    freeReplyObject(reply);
}

PHP_FUNCTION(phpiredis_format_command)
{
    zval **tmp;
    HashPosition pos;
    zval *arr;
    zval temp;

    int size;
    char **elements;
    size_t *elementslen;
    int currpos = 0;
    char *cmd;
    int cmdlen;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &arr) == FAILURE) {
        return;
    }

    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(arr), &pos);

    size = zend_hash_num_elements(Z_ARRVAL_P(arr));
    elements = emalloc(sizeof(char*) * size);
    elementslen = emalloc(sizeof(size_t) * size);

    while (zend_hash_get_current_data_ex(Z_ARRVAL_P(arr), (void **) &tmp, &pos) == SUCCESS) {
        temp = **tmp;
        zval_copy_ctor(&temp);
        convert_to_string(&temp);

        elementslen[currpos] = (size_t) Z_STRLEN(temp);
        elements[currpos] = emalloc(sizeof(char) * elementslen[currpos]);
        memcpy(elements[currpos], Z_STRVAL(temp), elementslen[currpos]);

        zval_dtor(&temp);
        zend_hash_move_forward_ex(Z_ARRVAL_P(arr), &pos);

        ++currpos;
    }

    cmdlen = redisFormatCommandArgv(&cmd, size, elements, elementslen);
    ZVAL_STRINGL(return_value, cmd, cmdlen, 1);

    for (; currpos > 0; --currpos) {
       efree(elements[currpos-1]);
    }
    efree(elements);
    efree(elementslen);
    free(cmd);
}

static
void free_reader_status_callback(phpiredis_reader *reader TSRMLS_DC)
{
    if (reader->status_callback) {
        efree(((callback*) reader->status_callback)->function);
        efree(reader->status_callback);
        reader->status_callback = NULL;
    }
}

static
void free_reader_error_callback(phpiredis_reader *reader TSRMLS_DC)
{
    if (reader->error_callback) {
        efree(((callback*) reader->error_callback)->function);
        efree(reader->error_callback);
        reader->error_callback = NULL;
    }
}

static
void convert_redis_to_php(phpiredis_reader* reader, zval* return_value, redisReply* reply TSRMLS_DC) {
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

        case REDIS_REPLY_ERROR:
        case REDIS_REPLY_STATUS:
            if (reader != NULL) {
                if (reply->type == REDIS_REPLY_ERROR) {
                    if (reader->error_callback != NULL) {
                        zval *arg[1];

                        MAKE_STD_ZVAL(arg[0]);
                        ZVAL_STRINGL(arg[0], reply->str, reply->len, 1);

                        if (call_user_function(EG(function_table), NULL, ((callback*) reader->error_callback)->function, return_value, 1, arg TSRMLS_CC) == FAILURE) {
                            zval_ptr_dtor(&return_value);
                            ZVAL_NULL(return_value);
                        }

                        zval_ptr_dtor(&arg[0]);
                        return;
                    }
                } else if (reply->type == REDIS_REPLY_STATUS) {
                    if (reader->status_callback != NULL) {
                        zval *arg[1];

                        MAKE_STD_ZVAL(arg[0]);
                        ZVAL_STRINGL(arg[0], reply->str, reply->len, 1);

                        if (call_user_function(EG(function_table), NULL, ((callback*) reader->status_callback)->function, return_value, 1, arg TSRMLS_CC) == FAILURE) {
                            zval_ptr_dtor(&return_value);
                            ZVAL_NULL(return_value);
                        }

                        zval_ptr_dtor(&arg[0]);
                        return;
                    }
                }
            }
            // NO BREAK! For status and error returning the string content

        case REDIS_REPLY_STRING:
            ZVAL_STRINGL(return_value, reply->str, reply->len, 1);
            return;

        case REDIS_REPLY_ARRAY: {
                zval *val;
                int j;

                array_init(return_value);
                for (j = 0; j < reply->elements; j++) {
                    MAKE_STD_ZVAL(val);
                    convert_redis_to_php(reader, val, reply->element[j] TSRMLS_CC);
                    add_index_zval(return_value, j, val);
                }
            }
            return;

        case REDIS_REPLY_NIL:
        default:
            ZVAL_NULL(return_value);
            return;
    }
}

PHP_FUNCTION(phpiredis_reader_create)
{
    phpiredis_reader* reader = emalloc(sizeof(phpiredis_reader));
    reader->reader = redisReplyReaderCreate();
    reader->error = NULL;
    reader->bufferedReply = NULL;
    reader->status_callback = NULL;
    reader->error_callback = NULL;

    ZEND_REGISTER_RESOURCE(return_value, reader, le_redis_reader_context);
}

PHP_FUNCTION(phpiredis_reader_set_error_handler)
{
    zval *ptr, **function;
    phpiredis_reader *reader;
    char *name;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rZ", &ptr, &function) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);

    if ((*function)->type == IS_NULL) {
        free_reader_error_callback(reader TSRMLS_CC);
    } else {
        if (!zend_is_callable(*function, 0, &name TSRMLS_CC)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument is not a valid callback");
            efree(name);
            RETURN_FALSE;
        }

        efree(name);
        free_reader_error_callback(reader TSRMLS_CC);

        reader->error_callback = emalloc(sizeof(callback));

        Z_ADDREF_PP(function);
        ((callback*) reader->error_callback)->function = *function;
    }

    RETURN_TRUE;
}

PHP_FUNCTION(phpiredis_reader_set_status_handler)
{
    zval *ptr, **function;
    phpiredis_reader *reader;
    char *name;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rZ", &ptr, &function) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);

    if ((*function)->type == IS_NULL) {
        free_reader_status_callback(reader TSRMLS_CC);
    } else {
        if (!zend_is_callable(*function, 0, &name TSRMLS_CC)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument is not a valid callback");
            efree(name);
            RETURN_FALSE;
        }

        efree(name);
        free_reader_status_callback(reader TSRMLS_CC);

        reader->status_callback = emalloc(sizeof(callback));

        Z_ADDREF_PP(function);
        ((callback*) reader->status_callback)->function = *function;
    }

    RETURN_TRUE;
}

PHP_FUNCTION(phpiredis_reader_reset)
{
    zval *ptr;
    phpiredis_reader *reader;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ptr) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);

    if (reader->bufferedReply != NULL) {
        freeReplyObject(reader->bufferedReply);
        reader->bufferedReply = NULL;
    }

    if (reader->reader != NULL) {
        redisReplyReaderFree(reader->reader);
    }

    reader->reader = redisReplyReaderCreate();
}

PHP_FUNCTION(phpiredis_reader_destroy)
{
    zval *ptr;
    phpiredis_reader *reader;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ptr) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);
    zend_list_delete(Z_LVAL_P(ptr));

    RETURN_TRUE;
}

PHP_FUNCTION(phpiredis_reader_feed)
{
    zval *ptr;
    phpiredis_reader *reader;
    char *bytes;
    int size;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &ptr, &bytes, &size) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);
    redisReplyReaderFeed(reader->reader, bytes, size);
}

PHP_FUNCTION(phpiredis_reader_get_error)
{
    zval *ptr;
    phpiredis_reader *reader;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ptr) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);

    if (reader->error == NULL) {
        RETURN_FALSE;
    }

    ZVAL_STRINGL(return_value, reader->error, strlen(reader->error), 1);
}

PHP_FUNCTION(phpiredis_reader_get_reply)
{
    zval *ptr;
    zval **type;
    phpiredis_reader *reader;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|Z", &ptr, &type) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);

    void* aux;

    if (reader->bufferedReply) {
        aux = reader->bufferedReply;
        reader->bufferedReply = NULL;
    } else {
        if (redisReplyReaderGetReply(reader->reader, &aux) == REDIS_ERR) {
            if (reader->error != NULL) {
                efree(reader->error);
            }
            reader->error = redisReplyReaderGetError(reader->reader);

            RETURN_FALSE; // error
        } else if (aux == NULL) {
            RETURN_FALSE; // incomplete
        }
    }

    convert_redis_to_php(reader, return_value, aux TSRMLS_CC);

    if (ZEND_NUM_ARGS() > 1) {
        zval_dtor(*type);
        ZVAL_LONG(*type, ((redisReply*) aux)->type);
    }

    freeReplyObject(aux);
}

PHP_FUNCTION(phpiredis_reader_get_state)
{
    zval *ptr;
    phpiredis_reader *reader;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ptr) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);

    if (reader->error == NULL && reader->bufferedReply == NULL) {
        void *aux;

        if (redisReplyReaderGetReply(reader->reader, &aux) == REDIS_ERR) {
            if (reader->error != NULL) {
                efree(reader->error);
            }
            reader->error = redisReplyReaderGetError(reader->reader);
        } else {
            reader->bufferedReply = aux;
        }
    }

    if (reader->error != NULL) {
        ZVAL_LONG(return_value, PHPIREDIS_READER_STATE_ERROR);
    } else if (reader->bufferedReply != NULL) {
        ZVAL_LONG(return_value, PHPIREDIS_READER_STATE_COMPLETE);
    } else {
        ZVAL_LONG(return_value, PHPIREDIS_READER_STATE_INCOMPLETE);
    }
}


PHP_MINIT_FUNCTION(phpiredis)
{
    le_redis_context = zend_register_list_destructors_ex(php_redis_connection_dtor, NULL, PHPIREDIS_CONNECTION_NAME, module_number);
    le_redis_persistent_context = zend_register_list_destructors_ex(NULL, php_redis_connection_dtor, PHPIREDIS_PERSISTENT_CONNECTION_NAME, module_number);
    le_redis_reader_context = zend_register_list_destructors_ex(php_redis_reader_dtor, NULL, PHPIREDIS_READER_NAME, module_number);

    REGISTER_LONG_CONSTANT("PHPIREDIS_READER_STATE_INCOMPLETE", PHPIREDIS_READER_STATE_INCOMPLETE, CONST_PERSISTENT|CONST_CS);
    REGISTER_LONG_CONSTANT("PHPIREDIS_READER_STATE_COMPLETE", PHPIREDIS_READER_STATE_COMPLETE, CONST_PERSISTENT|CONST_CS);
    REGISTER_LONG_CONSTANT("PHPIREDIS_READER_STATE_ERROR", PHPIREDIS_READER_STATE_ERROR, CONST_PERSISTENT|CONST_CS);
    REGISTER_LONG_CONSTANT("PHPIREDIS_REPLY_STRING", REDIS_REPLY_STRING, CONST_PERSISTENT|CONST_CS);
    REGISTER_LONG_CONSTANT("PHPIREDIS_REPLY_ARRAY", REDIS_REPLY_ARRAY, CONST_PERSISTENT|CONST_CS);
    REGISTER_LONG_CONSTANT("PHPIREDIS_REPLY_INTEGER", REDIS_REPLY_INTEGER, CONST_PERSISTENT|CONST_CS);
    REGISTER_LONG_CONSTANT("PHPIREDIS_REPLY_NIL", REDIS_REPLY_NIL, CONST_PERSISTENT|CONST_CS);
    REGISTER_LONG_CONSTANT("PHPIREDIS_REPLY_STATUS", REDIS_REPLY_STATUS, CONST_PERSISTENT|CONST_CS);
    REGISTER_LONG_CONSTANT("PHPIREDIS_REPLY_ERROR", REDIS_REPLY_ERROR, CONST_PERSISTENT|CONST_CS);

    REGISTER_LONG_CONSTANT("PHPIREDIS_ERROR_CONNECTION", PHPIREDIS_ERROR_CONNECTION, CONST_PERSISTENT|CONST_CS);
    REGISTER_LONG_CONSTANT("PHPIREDIS_ERROR_PROTOCOL", PHPIREDIS_ERROR_PROTOCOL, CONST_PERSISTENT|CONST_CS);

    return SUCCESS;
}

static zend_function_entry phpiredis_functions[] = {
    PHP_FE(phpiredis_connect, NULL)
    PHP_FE(phpiredis_pconnect, NULL)
    PHP_FE(phpiredis_disconnect, NULL)
    PHP_FE(phpiredis_set_error_handler, NULL)
    PHP_FE(phpiredis_command, NULL)
    PHP_FE(phpiredis_command_bs, NULL)
    PHP_FE(phpiredis_multi_command, NULL)
    PHP_FE(phpiredis_multi_command_bs, NULL)
    PHP_FE(phpiredis_format_command, NULL)
    PHP_FE(phpiredis_reader_create, NULL)
    PHP_FE(phpiredis_reader_reset, NULL)
    PHP_FE(phpiredis_reader_feed, NULL)
    PHP_FE(phpiredis_reader_get_state, NULL)
    PHP_FE(phpiredis_reader_get_error, NULL)
    PHP_FE(phpiredis_reader_get_reply, NULL)
    PHP_FE(phpiredis_reader_destroy, NULL)
    PHP_FE(phpiredis_reader_set_error_handler, NULL)
    PHP_FE(phpiredis_reader_set_status_handler, NULL)
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
