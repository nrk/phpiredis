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
void s_destroy_connection(phpiredis_connection *connection TSRMLS_DC)
{
    if (connection) {
        //if our IP is null, then we were initialized with a FD - don't close it.
        if (connection->ip == NULL) {
            if (connection->reader != NULL) {
                zend_list_delete(connection->reader_index);
            }
            redisFreeKeepFd(connection->c);
            zend_list_delete(connection->stream_index);
        } else {
            pefree(connection->ip, connection->is_persistent);
            //if our IP is null, we did not create our context - don't free it.
            if (connection->c != NULL) {
                redisFree(connection->c);
            }
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

    connection                = pemalloc(sizeof(phpiredis_connection), is_persistent);
    connection->c             = c;
    connection->ip            = pestrdup(ip, is_persistent);
    connection->port          = port;
    connection->is_persistent = is_persistent;
    connection->reader        = NULL;

    return connection;
}

static
void getCommandElements(zval *arr, char ***elements, size_t **elementslen, int *size)
{
    zval **tmp;
    zval temp;
    HashPosition pos;
    int currpos = 0;
    
    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(arr), &pos);
    *size = zend_hash_num_elements(Z_ARRVAL_P(arr));
    *elements = emalloc(sizeof(char*) * (*size));
    *elementslen = emalloc(sizeof(size_t) * (*size));
    while (zend_hash_get_current_data_ex(Z_ARRVAL_P(arr), (void **) &tmp, &pos) == SUCCESS) {
        temp = **tmp;
        zval_copy_ctor(&temp);
        convert_to_string(&temp);
        
        (*elementslen)[currpos] = (size_t) Z_STRLEN(temp);
        (*elements)[currpos] = emalloc(sizeof(char) * (*elementslen)[currpos]);
        memcpy((*elements)[currpos], Z_STRVAL(temp), (*elementslen)[currpos]);
        
        ++currpos;
        
        zval_dtor(&temp);
        zend_hash_move_forward_ex(Z_ARRVAL_P(arr), &pos);
    }
}

//redisGetReply always tries to read from the buffer.
//this wrapper function shortcuts that when possible, to avoid un-necessary network polls
static
int phpiredisGetReply(phpiredis_connection *connection, redisReply **replyP) {
    redisReply *reply;
    int result;
    
    /**
    result = redisGetReply(connection->c, &reply);
    *replyP = reply;
    return result;
    */
    
    if (redisGetReplyFromReader(connection->c, &reply) == REDIS_ERR) {
        return REDIS_ERR;
    } else if (reply == NULL) {
        result = redisGetReply(connection->c, &reply);
    }
    *replyP = reply;
    return result;
}

static
void freeCommandElements(char ***elements, size_t **elementslen, int *size)
{
    for (; *size > 0; --*size) {
        efree((*elements)[*size-1]);
    }
    efree((*elements));
    efree((*elementslen));
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

PHP_FUNCTION(phpiredis_import_stream) {
    zval *streamResource;
    phpiredis_connection *connection;
    redisContext *context;
    php_stream *stream;
    phpiredis_reader *reader;
    int fd = -1;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &streamResource) == FAILURE) {
        RETURN_FALSE;
    }
    
    php_stream_from_zval_no_verify(stream, &streamResource);
    if (stream == NULL) {
        RETURN_FALSE;
    }
    
    if (php_stream_cast(stream, PHP_STREAM_AS_FD | PHP_STREAM_CAST_INTERNAL,  (void*)&fd, 1) != SUCCESS || fd < 0) {
        RETURN_FALSE;
    }
    
    context = redisConnectFd(fd);
    if (!context) {
        RETURN_FALSE;
    }
    
    connection = pemalloc(sizeof(phpiredis_connection), 0);
    connection->c = context;
    connection->ip = NULL;
    connection->stream_index = streamResource->value.lval;
    connection->is_persistent = 0;
    connection->reader = NULL;
    zend_list_addref(connection->stream_index);
    
    ZEND_REGISTER_RESOURCE(return_value, connection, le_redis_context);
}

PHP_FUNCTION(phpiredis_set_reader) {
    zval *connectionResource;
    zval *readerResource;
    phpiredis_connection *connection;
    phpiredis_reader *reader;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rr", &connectionResource, &readerResource) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(connection, void *, &connectionResource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context);
    ZEND_FETCH_RESOURCE(reader, void *, &readerResource, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);

    connection->reader_index = readerResource->value.lval;
    connection->reader = reader;
    zend_list_addref(connection->reader_index);
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

PHP_FUNCTION(phpiredis_multi_command)
{
    zval **tmp;
    HashPosition pos;
    zval *resource;
    phpiredis_connection *connection;
    zval **arg2;
    int commands;
    int i;
    zval temp;
    int result;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rZ", &resource, &arg2) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);
    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(*arg2), &pos);

    commands = 0;
    while (zend_hash_get_current_data_ex(Z_ARRVAL_P(*arg2), (void **) &tmp, &pos) == SUCCESS) {
        temp = **tmp;
        zval_copy_ctor(&temp);
        convert_to_string(&temp);
        ++commands;
        redisAppendCommand(connection->c, Z_STRVAL(temp));
        zend_hash_move_forward_ex(Z_ARRVAL_P(*arg2), &pos);
        zval_dtor(&temp);
    }

    array_init(return_value);
    for (i = 0; i < commands; ++i) {
        redisReply *reply = NULL;
        zval* result;
        MAKE_STD_ZVAL(result);

        if (phpiredisGetReply(connection, &reply) != REDIS_OK) {
            for (; i < commands; ++i) {
                add_index_bool(return_value, i, 0);
            }

            if (reply) freeReplyObject(reply);

            efree(result);
            break;
        }

        convert_redis_to_php(NULL, result, reply TSRMLS_CC);
        add_index_zval(return_value, i, result);
        freeReplyObject(reply);
    }
}

PHP_FUNCTION(phpiredis_append_command)
{
    zval *resource;
    zval *arr;
    phpiredis_connection *connection;
    int size;
    char **elements;
    size_t *elementslen;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ra", &resource, &arr) == FAILURE) {
        return;
    }
    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);
    
    getCommandElements(arr, &elements, &elementslen, &size);
    redisAppendCommandArgv(connection->c, size, elements, elementslen);
    freeCommandElements(&elements, &elementslen, &size);
    RETURN_TRUE;
}

PHP_FUNCTION(phpiredis_multi_command_bs)
{
    zval **tmp;
    HashPosition cmdsPos;
    zval *resource;
    phpiredis_connection *connection;
    zval *cmds;
    zval cmdArgs;

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

        getCommandElements(&cmdArgs, &cmdElements, &cmdElementslen, &cmdSize);

        redisAppendCommandArgv(connection->c, cmdSize, cmdElements, cmdElementslen);

        freeCommandElements(&cmdElements, &cmdElementslen, &cmdSize);

        zend_hash_move_forward_ex(Z_ARRVAL_P(cmds), &cmdsPos);
        zval_dtor(&cmdArgs);
        ++commands;
    }

    array_init(return_value);
    for (i = 0; i < commands; ++i) {
        redisReply *reply = NULL;
        zval* result;
        MAKE_STD_ZVAL(result);

        if (phpiredisGetReply(connection, &reply) != REDIS_OK) {
            for (; i < commands; ++i) {
                add_index_bool(return_value, i, 0);
            }

            if (reply) freeReplyObject(reply);

            efree(result);
            break;
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
    //ignored, but required for zend_parse_parameters
    int commandSize;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &resource, &command, &commandSize) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    reply = redisCommand(connection->c, command);

    if (reply == NULL) {
        RETURN_FALSE;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, reply->str);
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

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ra", &resource, &params) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);
    getCommandElements(params, &argv, &argvlen, &argc);
    
    redisAppendCommandArgv(connection->c, argc, argv, (const size_t *) argvlen);
    
    freeCommandElements(&argv, &argvlen, &argc);

    if (phpiredisGetReply(connection, &reply) == REDIS_ERR) {
        RETURN_FALSE;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", reply->str);
        freeReplyObject(reply);

        RETURN_FALSE;
    }
    
    convert_redis_to_php(connection->reader, return_value, reply TSRMLS_CC);
    freeReplyObject(reply);
}

PHP_FUNCTION(phpiredis_format_command)
{
    zval *arr;
    int size;
    char **elements;
    size_t *elementslen;
    char *cmd;
    int cmdlen;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &arr) == FAILURE) {
        return;
    }

    getCommandElements(arr, &elements, &elementslen, &size);

    cmdlen = redisFormatCommandArgv(&cmd, size, elements, elementslen);
    ZVAL_STRINGL(return_value, cmd, cmdlen, 1);

    freeCommandElements(&elements, &elementslen, &size);
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
    void* aux;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|Z", &ptr, &type) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);


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

PHP_FUNCTION(phpiredis_read_reply) {
    zval *resource;
    redisReply *reply = NULL;
    phpiredis_connection *connection;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &resource) == FAILURE) {
        return;
    }
    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    if (phpiredisGetReply(connection, &reply) == REDIS_ERR) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, connection->c->errstr);
        RETURN_FALSE;
    }
    
    convert_redis_to_php(connection->reader, return_value, reply TSRMLS_CC);
    freeReplyObject(reply);
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

    return SUCCESS;
}

static zend_function_entry phpiredis_functions[] = {
    PHP_FE(phpiredis_connect, NULL)
    PHP_FE(phpiredis_pconnect, NULL)
    PHP_FE(phpiredis_disconnect, NULL)
    PHP_FE(phpiredis_append_command, NULL)
    PHP_FE(phpiredis_command, NULL)
    PHP_FE(phpiredis_command_bs, NULL)
    PHP_FE(phpiredis_import_stream, NULL)
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
    PHP_FE(phpiredis_read_reply, NULL)
    PHP_FE(phpiredis_set_reader, NULL)
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
