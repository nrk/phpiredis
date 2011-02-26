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
    PHP_FE(phpiredis_command_bs, NULL)
    PHP_FE(phpiredis_multi_command, NULL)
    PHP_FE(phpiredis_format_command, NULL)
    PHP_FE(phpiredis_reader_create, NULL)
    PHP_FE(phpiredis_reader_feed, NULL)
    PHP_FE(phpiredis_reader_get_state, NULL)
    PHP_FE(phpiredis_reader_get_error, NULL)
    PHP_FE(phpiredis_reader_get_reply, NULL)
    PHP_FE(phpiredis_reader_destroy, NULL)
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
#define PHPIREDIS_READER_NAME "phpredis reader"

static void php_redis_reader_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    phpiredis_reader *_reader = (void *)rsrc->ptr;

	if (_reader) {
		if (_reader->bufferedReply != NULL) {
			freeReplyObject(_reader->bufferedReply);
		}

		if (_reader->reader != NULL) {
			redisReplyReaderFree(_reader->reader);
		}
	}
}

static void php_redis_connection_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    phpiredis_connection *connection = (phpiredis_connection*)rsrc->ptr;

    if (connection) {
        free(connection->ip);
        if (connection->c != NULL)
        {
            redisFree(connection->c);
        }
    }
}

static void php_redis_connection_persist(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    phpiredis_connection *connection = (phpiredis_connection*)rsrc->ptr;

    if (connection) {
       free(connection->ip);
       if (connection->c != NULL)
       {
           redisFree(connection->c);
       }
    }
}


int le_redis_reader_context;
int le_redis_context;
int le_redis_persistent_context;
PHP_MINIT_FUNCTION(phpiredis)
{
    le_redis_context = zend_register_list_destructors_ex(php_redis_connection_dtor, NULL, PHPIREDIS_CONNECTION_NAME, module_number);
    le_redis_persistent_context = zend_register_list_destructors_ex(NULL, php_redis_connection_persist, PHPIREDIS_PERSISTENT_CONNECTION_NAME, module_number);
    le_redis_reader_context = zend_register_list_destructors_ex(php_redis_reader_dtor, NULL, PHPIREDIS_READER_NAME, module_number);
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
        efree(hashed_details);
        return;
    }

    redisContext *c;
    c = redisConnect(ip, (int)port); // FIXME: unsafe cast

    if (c->err) {
        efree(ip);
        redisFree(c);
        RETURN_FALSE;
    }

    connection = emalloc(sizeof(phpiredis_connection));
    connection->c = c;
    connection->ip = malloc(sizeof(char) * strlen(ip) + 1);
    strcpy(connection->ip, ip);
    connection->port = port;

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

PHP_FUNCTION(phpiredis_reader_create)
{
	phpiredis_reader* reader = emalloc(sizeof(phpiredis_reader));
	reader->reader = redisReplyReaderCreate();
	reader->error = NULL;
	reader->bufferedReply = NULL;
	ZEND_REGISTER_RESOURCE(return_value, reader, le_redis_reader_context);
}

PHP_FUNCTION(phpiredis_reader_destroy)
{
    zval *ptr;
    void *reader;

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
    phpiredis_reader *reader;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ptr) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE(reader, void *, &ptr, -1, PHPIREDIS_READER_NAME, le_redis_reader_context);

	void* aux;
	if (reader->bufferedReply) {
		aux = reader->bufferedReply;
		reader->bufferedReply = NULL;
	} else {
		if (redisReplyReaderGetReply(reader->reader, &aux) == REDIS_ERR) {
			if (reader->error != NULL) free(reader->error);
			reader->error = redisReplyReaderGetError(reader->reader);
			RETURN_FALSE; // error
		} else if (aux == NULL) {
			RETURN_FALSE; // incomplete
		}

	}
	convert_redis_to_php(return_value, aux);
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
			if (reader->error != NULL) free(reader->error);
			reader->error = redisReplyReaderGetError(reader->reader);
		} else {
			reader->bufferedReply = aux;
		}
	}

	if (reader->error != NULL) {
		RETURN_FALSE;
	}

	if (reader->bufferedReply != NULL) {
		RETURN_TRUE;
	}
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
    connection->ip = malloc(sizeof(char) * strlen(ip) + 1);
    strcpy(connection->ip, ip);
    connection->port = port;
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
        if (redisGetReply(connection->c,&reply) != REDIS_OK)
        {
            for (; i < commands; ++i) {
                add_index_bool(return_value, i, 0);
            }
            break;
        }
        convert_redis_to_php(result, reply);
        add_index_zval(return_value, i, result);
        freeReplyObject(reply);
    }
}

PHP_FUNCTION(phpiredis_format_command)
{
    zval         **tmp;
    HashPosition   pos;
    zval *arr;
    zval **arg;

    int size = 0;
    char **elements;
    size_t *elementslen;
    int elementstmpsize = 10;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "Z", &arg) == FAILURE) {
        return;
    }

    arr = *arg;
    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(arr), &pos);

    elements = malloc(sizeof(char*) * elementstmpsize);
    elementslen = malloc(sizeof(int) * elementstmpsize);
    while (zend_hash_get_current_data_ex(Z_ARRVAL_P(arr), (void **) &tmp, &pos) == SUCCESS) {
        if (size == elementstmpsize) {
            elementstmpsize *= 2;
            if (elementstmpsize == 0) elementstmpsize = 1;
            elements = (char **)realloc(elements, sizeof(char*) * elementstmpsize);
            elementslen = (size_t *)realloc(elementslen, sizeof(int) * elementstmpsize);
        }

        switch ((*tmp)->type) {
                case IS_STRING: {
                        elementslen[size] = (size_t) Z_STRLEN_PP(tmp);
                        elements[size] = malloc(sizeof(char) * elementslen[size]);
                        memcpy(elements[size], Z_STRVAL_PP(tmp), elementslen[size]);
                }
                        break;

                case IS_OBJECT: {
                        int copy;
                        zval expr;
                        zend_make_printable_zval(*tmp, &expr, &copy);
                        elementslen[size] = (size_t) Z_STRLEN(expr);
                        elements[size] = malloc(sizeof(char) * elementslen[size]);
                        memcpy(elements[size], Z_STRVAL(expr), elementslen[size]);
                        if (copy) {
                                zval_dtor(&expr);
                        }
                }
                        break;

                default: {
                        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Array argument must contain strings");
                        elementslen[size] = 0;
                }
                        break;
        }

        zend_hash_move_forward_ex(Z_ARRVAL_P(arr), &pos);
        ++size;
    }

    char *cmd;
    int len;
    len = redisFormatCommandArgv(&cmd,size,elements,elementslen);

    ZVAL_STRINGL(return_value, cmd, len, 1);
    for (;size>0;--size)
	    free(elements[size-1]);
    free(elements);
    free(elementslen);
    free(cmd);
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

    ZEND_FETCH_RESOURCE2(connection, redisContext *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    while (reply == NULL || reply->type == REDIS_REPLY_ERROR)
    {
        reply = redisCommand(connection->c,command);
        if (reply == NULL) {
            redisFree(connection->c);
            redisContext* c = redisConnect(connection->ip, connection->port);

            if (c->err) {
                redisFree(c);
                connection->c = NULL;
                RETURN_FALSE;
                return;
            }
            connection->c = c;
        } else if (reply->type == REDIS_REPLY_ERROR) {
            if (connection->c == REDIS_OK) { // The problem was the command
                php_error_docref(NULL TSRMLS_CC, E_WARNING, reply->str+4);
                RETURN_FALSE;
                return;
            } else {
                // TODO: whats happening here?
            }
        }
    }
    convert_redis_to_php(return_value, reply);
    freeReplyObject(reply);
}

PHP_FUNCTION(phpiredis_command_bs)
{
    zval *resource;
    redisReply *reply;
    phpiredis_connection *connection;
    zval *params;
    int argc;
    char ** argv;
    size_t * argvlen;
    zval         **tmp;
    HashPosition   pos;
    int i;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ra", &resource, &params) == FAILURE) {
        return;
    }

    ZEND_FETCH_RESOURCE2(connection, redisContext *, &resource, -1, PHPIREDIS_CONNECTION_NAME, le_redis_context, le_redis_persistent_context);

    argc = zend_hash_num_elements(Z_ARRVAL_P(params));
    argvlen = malloc(sizeof(size_t) * argc);
    argv = malloc(sizeof(char*) * argc);

    i = 0;
    zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(params), &pos);
    while (zend_hash_get_current_data_ex(Z_ARRVAL_P(params), (void **) &tmp, &pos) == SUCCESS) {
        switch ((*tmp)->type) {
                case IS_STRING: {
                        argvlen[i] = (size_t) Z_STRLEN_PP(tmp);
                        argv[i] = malloc(sizeof(char) * argvlen[i]);
                        memcpy(argv[i], Z_STRVAL_PP(tmp), argvlen[i]);
                }
                        break;

                case IS_OBJECT: {
                        int copy;
                        zval expr;
                        zend_make_printable_zval(*tmp, &expr, &copy);
                        argvlen[i] = Z_STRLEN(expr);
                        argv[i] = malloc(sizeof(char) * argvlen[i]);
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
        free(argv[i]);
    }
    free(argv);
    free(argvlen);

    if (redisGetReply(connection->c,&reply) != REDIS_OK) {
        efree(params);
        RETURN_FALSE;
        return;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        efree(params);
        php_error_docref(NULL TSRMLS_CC, E_WARNING, reply->str+4);
        RETURN_FALSE;
        return;
    }
    convert_redis_to_php(return_value, reply);
    freeReplyObject(reply);
}
