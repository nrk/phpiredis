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

#include <zend_API.h>

#ifdef ZEND_ENGINE_3
	#define PHPIREDIS_LEN_TYPE size_t
	#define PHPIREDIS_RESOURCE_TYPE zend_resource

#define PHPIREDIS_RETURN_RESOURCE(connection, context) \
    RETURN_RES(zend_register_resource(connection, context))
#else
#define PHPIREDIS_LEN_TYPE int
#define PHPIREDIS_RESOURCE_TYPE zend_rsrc_list_entry


#define PHPIREDIS_RETURN_RESOURCE(connection, context) \
    ZEND_REGISTER_RESOURCE(return_value, connection, context)
#endif

typedef struct callback {

#ifdef ZEND_ENGINE_3
    zval function;
#else
    zval *function;
#endif
} callback;

static
void convert_redis_to_php(phpiredis_reader* reader, zval* return_value, redisReply* reply TSRMLS_DC);

static
void s_destroy_connection(phpiredis_connection *connection TSRMLS_DC)
{
    if (connection) {
        pefree(connection->ip, connection->is_persistent);
        if (connection->c != NULL) {
            redisFree(connection->c);
        }
        pefree(connection, connection->is_persistent);
    }
}

#ifdef ZEND_ENGINE_3

static
void php_redis_connection_dtor(zend_resource *rsrc TSRMLS_DC)
{
    phpiredis_connection *connection = (phpiredis_connection*) rsrc->ptr;
    s_destroy_connection(connection TSRMLS_CC);
}

#else

static
void php_redis_connection_dtor(PHPIREDIS_RESOURCE_TYPE *rsrc TSRMLS_DC)
{
    phpiredis_connection *connection = (phpiredis_connection*) rsrc->ptr;
    s_destroy_connection(connection TSRMLS_CC);
}

#endif

static
void php_redis_reader_dtor(PHPIREDIS_RESOURCE_TYPE *rsrc TSRMLS_DC)
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
#ifdef ZEND_ENGINE_3
            Z_TRY_DELREF_P(&((callback*) _reader->error_callback)->function);
#else
            efree(((callback*) _reader->error_callback)->function);
#endif
            efree(_reader->error_callback);
        }

        if (_reader->status_callback != NULL) {
#ifdef ZEND_ENGINE_3
            Z_TRY_DELREF_P(&((callback*) _reader->status_callback)->function);
#else
            efree(((callback*) _reader->status_callback)->function);
#endif
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

    if (ip[0] == '/') {
        // We simply ignore the value of "port" if the string value in "ip"
        // starts with a slash character indicating a UNIX domain socket path.
        c = redisConnectUnix(ip);
    } else {
        c = redisConnect(ip, port);
    }

    if (!c || c->err) {
        redisFree(c);
        return NULL;
    }

    connection                = pemalloc(sizeof(phpiredis_connection), is_persistent);
    connection->c             = c;
    connection->ip            = pestrdup(ip, is_persistent);
    connection->port          = port;
    connection->is_persistent = is_persistent;

    return connection;
}

PHP_FUNCTION(phpiredis_connect)
{
    phpiredis_connection *connection;
    char *ip;
    PHPIREDIS_LEN_TYPE ip_size;
    long port = 6379;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &ip, &ip_size, &port) == FAILURE) {
        return;
    }

    connection = s_create_connection(ip, port, 0);

    if (!connection) {
        RETURN_FALSE;
    }

    PHPIREDIS_RETURN_RESOURCE(connection, le_redis_context);
}

PHP_FUNCTION(phpiredis_pconnect)
{
    char *ip;
    PHPIREDIS_LEN_TYPE ip_size;
    long port = 6379;

    char *hashed_details = NULL;
    PHPIREDIS_LEN_TYPE hashed_details_length;
    phpiredis_connection *connection;
#ifdef ZEND_ENGINE_3
    zval *p_zval;
    zval new_resource;
    zend_resource *p_new_resource;
    zval new_le_zval;
    zend_resource new_le;
#else
    zend_rsrc_list_entry new_le, *le;
#endif


    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &ip, &ip_size, &port) == FAILURE) {
        return;
    }

    hashed_details_length = spprintf(&hashed_details, 0, "phpiredis_%s_%d", ip, (int)port);

#ifdef ZEND_ENGINE_3
    //Both of these should work - but weird stuff is happening in the shutdown.
    //zend_string *hash_string;
    //hash_string = strpprintf(0, "phpiredis_%s_%d", ip, (int)port);
    //p_zval = zend_hash_find_ptr(&EG(persistent_list), hash_string);
    p_zval = zend_hash_str_find_ptr(&EG(persistent_list), hashed_details, hashed_details_length);
        
    if (p_zval != NULL) {
        if (Z_RES_P(p_zval)->type != le_redis_persistent_context) {
            RETURN_FALSE;
        }
        connection = (phpiredis_connection *) Z_RES_P(p_zval)->ptr;
#else
    if (zend_hash_find(&EG(persistent_list), hashed_details, hashed_details_length+1, (void **) &le)!=FAILURE) {
        if (Z_TYPE_P(le) != le_redis_persistent_context) {
            RETURN_FALSE;
        }
        connection = (phpiredis_connection *) le->ptr;
#endif
        efree(hashed_details);
        PHPIREDIS_RETURN_RESOURCE(connection, le_redis_persistent_context);
        return;
    }

    connection = s_create_connection(ip, port, 1);

    

    if (!connection) {
        efree(hashed_details);
        RETURN_FALSE;
    }

#ifdef ZEND_ENGINE_3  
    p_new_resource = zend_register_resource(connection, le_redis_persistent_context);
    RETVAL_RES(p_new_resource);

    new_le.type = le_redis_persistent_context;
    new_le.ptr = connection;
    
    ZVAL_NEW_PERSISTENT_RES(&new_le, -1, mysql, le_plink);
    
    if (zend_hash_str_update_mem(&EG(persistent_list), hashed_details, hashed_details_length, &new_le, sizeof(zend_resource)) == NULL) {
#else
    new_le.type = le_redis_persistent_context;
    new_le.ptr = connection;

    if (zend_hash_update(&EG(persistent_list), hashed_details, hashed_details_length+1, (void *) &new_le, sizeof(zend_rsrc_list_entry), NULL)==FAILURE) {
#endif

        

        s_destroy_connection (connection TSRMLS_CC);
        efree(hashed_details);
        RETURN_FALSE;
    }

    efree(hashed_details);
    
#ifdef ZEND_ENGINE_3
    return;
#else
    PHPIREDIS_RETURN_RESOURCE(connection, le_redis_persistent_context);
#endif    
}

PHP_FUNCTION(phpiredis_disconnect)
{
    zval *connection;
    redisContext *c;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &connection) == FAILURE) {
        return;
    }

#ifdef ZEND_ENGINE_3
    c = (redisContext *)zend_fetch_resource2_ex(connection, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    if (c == NULL) {
        //Resource fetching can fail.
        RETURN_FALSE;
    }
    zend_list_delete(Z_RES_P(connection));
#else
    ZEND_FETCH_RESOURCE2(c, redisContext *, &connection, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);
    zend_list_delete(Z_LVAL_P(connection));
#endif

    RETURN_TRUE;
}

PHP_FUNCTION(phpiredis_multi_command)
{
    HashPosition pos;
    zval *resource;
    phpiredis_connection *connection;
    zval *p_arg2;
    
    int commands;
    int i;

#ifdef ZEND_ENGINE_3
    zend_resource *connection_resource;
    zval *p_zval;
#else 
    zval **tmp;
    zval temp;
#endif
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz", &resource, &p_arg2) == FAILURE) {
        return;
    }
    
#ifdef ZEND_ENGINE_3
    connection_resource = Z_RES_P(resource);
    connection = (phpiredis_connection *)zend_fetch_resource2(connection_resource, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);
    //Needs error check
#else
    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);
#endif

    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(p_arg2), &pos);

    commands = 0;
    
#ifdef ZEND_ENGINE_3
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(p_arg2), p_zval) {
        ++commands;
        redisAppendCommand(connection->c, Z_STRVAL_P(p_zval));
    } ZEND_HASH_FOREACH_END();
#else
    while (zend_hash_get_current_data_ex(Z_ARRVAL_P(p_arg2), (void **) &tmp, &pos) == SUCCESS) {
        temp = **tmp;
        zval_copy_ctor(&temp);
        convert_to_string(&temp);
        ++commands;
        redisAppendCommand(connection->c, Z_STRVAL(temp));
        zend_hash_move_forward_ex(Z_ARRVAL_P(p_arg2), &pos);
        zval_dtor(&temp);
    }
#endif

    array_init(return_value);
    for (i = 0; i < commands; ++i) {
        redisReply *reply = NULL;
#ifdef ZEND_ENGINE_3
        zval result;
        zval *p_result = &result;
#else
        zval* p_result;
        MAKE_STD_ZVAL(p_result);
#endif

        if (redisGetReply(connection->c, (void *)&reply) != REDIS_OK) {
            for (; i < commands; ++i) {
                add_index_bool(return_value, i, 0);
            }

            if (reply) freeReplyObject(reply);
#ifndef ZEND_ENGINE_3
            efree(p_result);
#endif
            break;
        }

        convert_redis_to_php(NULL, p_result, reply TSRMLS_CC);
        add_index_zval(return_value, i, p_result);
        freeReplyObject(reply);
    }
}

#ifdef ZEND_ENGINE_3

PHP_FUNCTION(phpiredis_multi_command_bs)
{
    zval *resource;
    phpiredis_connection *connection;
    zval *cmds;
    zval *p_cmdArgs;
    zval *p_cmdArg;

    int cmdPos;
    int cmdSize;
    char **cmdElements;
    size_t *cmdElementslen;

    int commands;
    int i;

    zend_resource *connection_resource;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ra", &resource, &cmds) == FAILURE) {
        return;
    }

    connection_resource = Z_RES_P(resource);
    connection = (phpiredis_connection *)zend_fetch_resource2(connection_resource, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    if (connection == NULL) {
        //Resource fetching can fail.
        //RETURN_FALSE;
    }

    commands = 0;
//    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(cmds), &cmdsPos);
    //while (zend_hash_get_current_data_ex(Z_ARRVAL_P(cmds), (void **) &tmp, &cmdsPos) == SUCCESS) {
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(cmds), p_cmdArgs) {
        //cmdArgs = **tmp;
        //zval_copy_ctor(&cmdArgs);

        cmdPos = 0;
        cmdSize = zend_hash_num_elements(Z_ARRVAL_P(p_cmdArgs));
        cmdElements = emalloc(sizeof(char*) * cmdSize);
        cmdElementslen = emalloc(sizeof(size_t) * cmdSize);

        //zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(&cmdArgs), &cmdArgsPos);
        //while (zend_hash_get_current_data_ex(Z_ARRVAL_P(&cmdArgs), (void **) &tmpArg, &cmdArgsPos) == SUCCESS) {
        ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(p_cmdArgs), p_cmdArg) {        
            zend_string *str = zval_get_string(p_cmdArg);
            //cmdArg = **tmpArg;
            //zval_copy_ctor(&cmdArg);
            //convert_to_string(&cmdArg);
            cmdElementslen[cmdPos] = (size_t) str->len;
            cmdElements[cmdPos] = emalloc(sizeof(char) * cmdElementslen[cmdPos]);
            memcpy(cmdElements[cmdPos], str->val, cmdElementslen[cmdPos]);

            //zval_dtor(&cmdArg);
            //zend_hash_move_forward_ex(Z_ARRVAL_P(&cmdArgs), &cmdArgsPos);
            ++cmdPos;
        } ZEND_HASH_FOREACH_END();


        redisAppendCommandArgv(connection->c, cmdSize, (const char **)cmdElements, cmdElementslen);

        for (; cmdPos > 0; --cmdPos) {
           efree(cmdElements[cmdPos-1]);
        }
        efree(cmdElements);
        efree(cmdElementslen);

        //zend_hash_move_forward_ex(Z_ARRVAL_P(cmds), &cmdsPos);
        //zval_dtor(&cmdArgs);
        ++commands;
    } ZEND_HASH_FOREACH_END();


    array_init(return_value);
    for (i = 0; i < commands; ++i) {
        redisReply *reply = NULL;
        zval result;

        if (redisGetReply(connection->c, (void *)&reply) != REDIS_OK) {
            for (; i < commands; ++i) {
                add_index_bool(return_value, i, 0);
            }

            if (reply) freeReplyObject(reply);

            break;
        }

        convert_redis_to_php(NULL, &result, reply TSRMLS_CC);
        add_index_zval(return_value, i, &result);
        freeReplyObject(reply);
    }
}


#else

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

        redisAppendCommandArgv(connection->c, cmdSize, (const char **)cmdElements, cmdElementslen);

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

        if (redisGetReply(connection->c, (void **)&reply) != REDIS_OK) {
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

#endif


PHP_FUNCTION(phpiredis_command)
{
    zval *resource;
    redisReply *reply = NULL;
    phpiredis_connection *connection;
    char *command;
    PHPIREDIS_LEN_TYPE command_size;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &resource, &command, &command_size) == FAILURE) {
        return;
    }

#ifdef ZEND_ENGINE_3
    connection = (phpiredis_connection *)zend_fetch_resource2_ex(resource, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);
    if (connection == NULL) {
        //Resource fetching can fail.
        //RETURN_FALSE;
    }
#else
    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);
#endif

    reply = redisCommand(connection->c, command);

    if (reply == NULL) {
        RETURN_FALSE;
        return;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, reply->str);
        freeReplyObject(reply);

        RETURN_FALSE;
        return;
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
    zval *p_zv;
    HashPosition pos;
    int i;
#ifndef ZEND_ENGINE_3
     zval **tmp;
#endif

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ra", &resource, &params) == FAILURE) {
        return;
    }

#ifdef ZEND_ENGINE_3
    connection = (phpiredis_connection *)zend_fetch_resource2_ex(resource, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);
    if (connection == NULL) {
        //Resource fetching can fail.
        //RETURN_FALSE;
    }
#else
    ZEND_FETCH_RESOURCE2(connection, phpiredis_connection *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);
#endif

    argc = zend_hash_num_elements(Z_ARRVAL_P(params));
    argvlen = emalloc(sizeof(size_t) * argc);
    argv = emalloc(sizeof(char*) * argc);

    i = 0;

#ifdef ZEND_ENGINE_3

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(params), p_zv) {
        switch (Z_TYPE_P(p_zv)) {
                case IS_STRING: {
                        argvlen[i] = (size_t) Z_STRLEN_P(p_zv);
                        argv[i] = emalloc(sizeof(char) * argvlen[i]);
                        memcpy(argv[i], Z_STRVAL_P(p_zv), argvlen[i]);
                    }
                    break;

                case IS_OBJECT: {
                        zval expr;
                        zend_make_printable_zval(p_zv, &expr);
                        argvlen[i] = Z_STRLEN(expr);
                        argv[i] = emalloc(sizeof(char) * argvlen[i]);
                        memcpy(argv[i], Z_STRVAL(expr), argvlen[i]);
                    }
                    break;

                default:
                    php_error_docref(NULL TSRMLS_CC, E_WARNING, "Array argument must contain strings");
                    break;
        }

        //zend_hash_move_forward_ex(Z_ARRVAL_P(params), &pos);
        ++i;
    } ZEND_HASH_FOREACH_END();

#else

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
#endif

    redisAppendCommandArgv(connection->c, argc, (const char **)argv, (const size_t *) argvlen);

    for (i = 0; i < argc; i++) {
        efree(argv[i]);
    }
    efree(argv);
    efree(argvlen);

    if (redisGetReply(connection->c, (void **)&reply) != REDIS_OK) {
        // only free if the reply was actually created
        if (reply) freeReplyObject(reply);

        RETURN_FALSE;
        return;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s", reply->str);
        freeReplyObject(reply);

        RETURN_FALSE;
        return;
    }

    convert_redis_to_php(NULL, return_value, reply TSRMLS_CC);
    freeReplyObject(reply);
}


#ifdef ZEND_ENGINE_3

PHP_FUNCTION(phpiredis_format_command)
{
    HashPosition pos;
    zval *arr;
    zval *p_zv;

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

    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(arr), p_zv) {
        zend_string *str = zval_get_string(p_zv);

        elementslen[currpos] = (size_t) str->len;
        elements[currpos] = emalloc(sizeof(char) * elementslen[currpos]);
        memcpy(elements[currpos], str->val, elementslen[currpos]);
        zend_hash_move_forward_ex(Z_ARRVAL_P(arr), &pos);

        ++currpos;
    } ZEND_HASH_FOREACH_END();

    cmdlen = redisFormatCommandArgv(&cmd, size, (const char **)elements, elementslen);
    ZVAL_STRINGL(return_value, cmd, cmdlen);

    for (; currpos > 0; --currpos) {
       efree(elements[currpos-1]);
    }
    efree(elements);
    efree(elementslen);
    free(cmd);
}

#else 

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

    cmdlen = redisFormatCommandArgv(&cmd, size, (const char **)elements, elementslen);
    ZVAL_STRINGL(return_value, cmd, cmdlen, 1);

    for (; currpos > 0; --currpos) {
       efree(elements[currpos-1]);
    }
    efree(elements);
    efree(elementslen);
    free(cmd);
}

#endif

static
void free_reader_status_callback(phpiredis_reader *reader TSRMLS_DC)
{
    if (reader->status_callback) {
#ifdef ZEND_ENGINE_3
        Z_TRY_DELREF_P(&((callback*) reader->status_callback)->function);
#else
        efree(((callback*) reader->status_callback)->function);
#endif
        efree(reader->status_callback);
        reader->status_callback = NULL;
    }
}

static
void free_reader_error_callback(phpiredis_reader *reader TSRMLS_DC)
{
    if (reader->error_callback) {

#ifdef ZEND_ENGINE_3
        Z_TRY_DELREF_P(&((callback*) reader->error_callback)->function);
#else
        efree(((callback*) reader->error_callback)->function);
#endif
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
                                                
#ifdef ZEND_ENGINE_3
                        zval arg;
                        ZVAL_STRINGL(&arg, reply->str, reply->len);
 
                        if (call_user_function(EG(function_table), NULL, &((callback*) reader->error_callback)->function, return_value, 1, &arg TSRMLS_CC) == FAILURE) {
                            zval_ptr_dtor(return_value);
                            ZVAL_NULL(return_value);
                        }
#else
                        zval *arg[1];
                        zval *p_arg;
                        MAKE_STD_ZVAL(arg[0]);
                        ZVAL_STRINGL(arg[0], reply->str, reply->len, 1);
                        if (call_user_function(EG(function_table), NULL, ((callback*) reader->error_callback)->function, return_value, 1, arg TSRMLS_CC) == FAILURE) {
                            zval_ptr_dtor(&return_value);
                            ZVAL_NULL(return_value);
                        }

                        zval_ptr_dtor(&arg[0]);
#endif                        
                        return;
                    }
                } else if (reply->type == REDIS_REPLY_STATUS) {
                    if (reader->status_callback != NULL) {

#ifdef ZEND_ENGINE_3
                        zval arg;
                        ZVAL_STRINGL(&arg, reply->str, reply->len);
                        ZVAL_STRINGL(return_value, reply->str, reply->len);
                        if (call_user_function(EG(function_table), NULL, &((callback*) reader->status_callback)->function, return_value, 1, &arg TSRMLS_CC) == FAILURE) {
                            zval_ptr_dtor(return_value);
                            ZVAL_NULL(return_value);
                        }
#else
                        zval *arg[1];

                        MAKE_STD_ZVAL(arg[0]);

                        ZVAL_STRINGL(arg[0], reply->str, reply->len, 1);
                        if (call_user_function(EG(function_table), NULL, ((callback*) reader->status_callback)->function, return_value, 1, arg TSRMLS_CC) == FAILURE) {
                            zval_ptr_dtor(&return_value);
                            ZVAL_NULL(return_value);
                        }

                        zval_ptr_dtor(&arg[0]);
#endif
                        return;
                    }
                }
            }
            // NO BREAK! For status and error returning the string content

        case REDIS_REPLY_STRING:
#ifdef ZEND_ENGINE_3
            ZVAL_STRINGL(return_value, reply->str, reply->len);
#else
            ZVAL_STRINGL(return_value, reply->str, reply->len, 1);
#endif
            return;

        case REDIS_REPLY_ARRAY: {
#ifdef ZEND_ENGINE_3
                zval val;
#else
                zval *val;
#endif
                int j;

                array_init(return_value);
                for (j = 0; j < reply->elements; j++) {
#ifdef ZEND_ENGINE_3
                    convert_redis_to_php(reader, &val, reply->element[j] TSRMLS_CC);
                    add_index_zval(return_value, j, &val);
#else
                    MAKE_STD_ZVAL(val);
                    convert_redis_to_php(reader, val, reply->element[j] TSRMLS_CC);
                    add_index_zval(return_value, j, val);
#endif
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

    PHPIREDIS_RETURN_RESOURCE(reader, le_redis_reader_context);
}


#ifdef ZEND_ENGINE_3

PHP_FUNCTION(phpiredis_reader_set_error_handler)
{
    zval *ptr, *function = NULL;
    phpiredis_reader *reader;
    zend_resource *reader_resource;
    zend_string *name;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz", &ptr, &function) == FAILURE) {
        return;
    }
 
    reader_resource = Z_RES_P(ptr);
    reader = (void *)zend_fetch_resource(reader_resource, PHPIREDIS_READER_NAME, le_redis_reader_context);

    if (Z_TYPE_P(function) == IS_NULL) {
        free_reader_error_callback(reader TSRMLS_CC);
    } else {
        if (!zend_is_callable(function, 0, &name TSRMLS_CC)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument is not a valid callback");
            zend_string_release(name);
            RETURN_FALSE;
        }

        zend_string_release(name);
        free_reader_error_callback(reader TSRMLS_CC);

        reader->error_callback = emalloc(sizeof(callback));

        Z_TRY_ADDREF_P(function); //TODO - this is incorrect? We are copying value not reference.
        ZVAL_COPY_VALUE(&((callback*) reader->error_callback)->function, function);
    }

    RETURN_TRUE;
}

PHP_FUNCTION(phpiredis_reader_set_status_handler)
{
    zval *ptr, *function;
    phpiredis_reader *reader;
    zend_resource *reader_resource;
    zend_string *name;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rz", &ptr, &function) == FAILURE) {
        return;
    }

    reader_resource = Z_RES_P(ptr);
    reader = (void *)zend_fetch_resource(reader_resource, PHPIREDIS_READER_NAME, le_redis_reader_context);

    if (Z_TYPE_P(function) == IS_NULL) {
        free_reader_status_callback(reader TSRMLS_CC);
    } else {
        if (!zend_is_callable(function, 0, &name TSRMLS_CC)) {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument is not a valid callback");
            zend_string_release(name);
            RETURN_FALSE;
        }

        zend_string_release(name);
        free_reader_status_callback(reader TSRMLS_CC);

        reader->status_callback = emalloc(sizeof(callback));

        Z_ADDREF_P(function);
        ZVAL_COPY_VALUE(&((callback*) reader->status_callback)->function, function);
    }

    RETURN_TRUE;
}

#else

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

#endif


PHP_FUNCTION(phpiredis_reader_reset)
{
    zval *ptr;
    phpiredis_reader *reader;
#ifdef ZEND_ENGINE_3
    zend_resource *reader_resource;
#endif

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ptr) == FAILURE) {
        return;
    }

#ifdef ZEND_ENGINE_3
    reader_resource = Z_RES_P(ptr);
    reader = (void *)zend_fetch_resource(reader_resource, PHPIREDIS_READER_NAME, le_redis_reader_context);
#else
    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);
#endif

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
#ifdef ZEND_ENGINE_3
    zend_resource *reader_resource;
#endif

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ptr) == FAILURE) {
        return;
    }

#ifdef ZEND_ENGINE_3
    reader_resource = Z_RES_P(ptr);
    reader = (void *)zend_fetch_resource(reader_resource, PHPIREDIS_READER_NAME, le_redis_reader_context);
    zend_list_delete(reader_resource);
#else
    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);
    zend_list_delete(Z_LVAL_P(ptr));
#endif

    RETURN_TRUE;
}

PHP_FUNCTION(phpiredis_reader_feed)
{
    zval *ptr;
    phpiredis_reader *reader;
    char *bytes;
    PHPIREDIS_LEN_TYPE size;

#ifdef ZEND_ENGINE_3
    zend_resource *reader_resource;
#endif

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &ptr, &bytes, &size) == FAILURE) {
        return;
    }

#ifdef ZEND_ENGINE_3
    reader_resource = Z_RES_P(ptr);
    reader = (void *)zend_fetch_resource(reader_resource, PHPIREDIS_READER_NAME, le_redis_reader_context);
#else
    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);
#endif

    redisReplyReaderFeed(reader->reader, bytes, size);
}

PHP_FUNCTION(phpiredis_reader_get_error)
{
    zval *ptr;
    phpiredis_reader *reader;
#ifdef ZEND_ENGINE_3
    zend_resource *reader_resource;
#endif

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ptr) == FAILURE) {
        return;
    }
    
#ifdef ZEND_ENGINE_3
    reader_resource = Z_RES_P(ptr);
    reader = (void *)zend_fetch_resource(reader_resource, PHPIREDIS_READER_NAME, le_redis_reader_context);
    //zend_list_delete(reader_resource);
#else
    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);
#endif

    if (reader->error == NULL) {
        RETURN_FALSE;
    }

#ifdef ZEND_ENGINE_3
    ZVAL_STRINGL(return_value, reader->error, strlen(reader->error));
#else
    ZVAL_STRINGL(return_value, reader->error, strlen(reader->error), 1);
#endif
}

PHP_FUNCTION(phpiredis_reader_get_reply)
{
    zval *ptr;
    zval **type = NULL;
    phpiredis_reader *reader;
#ifdef ZEND_ENGINE_3
    zend_resource *reader_resource;
#endif
    redisReply* aux;

#ifdef ZEND_ENGINE_3
    zval *p_type = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|z", &ptr, &p_type) == FAILURE) {
        return;
    }

    type = &p_type;    
#else
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|Z", &ptr, &type) == FAILURE) {
        return;
    }
#endif


#ifdef ZEND_ENGINE_3
    reader_resource = Z_RES_P(ptr);
    reader = (void *)zend_fetch_resource(reader_resource, PHPIREDIS_READER_NAME, le_redis_reader_context);
#else
    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);
#endif

    if (reader->bufferedReply) {
        aux = reader->bufferedReply;
        reader->bufferedReply = NULL;
    } else {
        if (redisReplyReaderGetReply(reader->reader, (void **)&aux) == REDIS_ERR) {
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

//#ifdef ZEND_ENGINE_3
//    //This should work - but doesn't. :-P
//    if (p_type != NULL) {
//        // ZVAL_DEREF(p_type); is this needed?
//        zval_ptr_dtor(p_type);
//        ZVAL_LONG(p_type, aux->type);
//    }
//#else
//#endif
    if (ZEND_NUM_ARGS() > 1) {
        zval_dtor(*type);
        ZVAL_LONG(*type, aux->type);
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

#ifdef ZEND_ENGINE_3
    reader = (phpiredis_reader *)zend_fetch_resource(Z_RES_P(ptr), PHPIREDIS_READER_NAME, le_redis_reader_context);

    if (reader == NULL) {
        RETURN_FALSE;
    }
#else
    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);
#endif

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

    return SUCCESS;
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_phpiredis_reader_get_reply, 0, 0, 1)
	ZEND_ARG_INFO(0, ptr)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

static zend_function_entry phpiredis_functions[] = {
    PHP_FE(phpiredis_connect, NULL)
    PHP_FE(phpiredis_pconnect, NULL)
    PHP_FE(phpiredis_disconnect, NULL)
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
    PHP_FE(phpiredis_reader_get_reply, arginfo_phpiredis_reader_get_reply)
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
