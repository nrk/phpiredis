#include "hiredis/hiredis.h"

typedef struct _phpiredis_connection {
    redisContext *ctx;
    char* ip;
    int port;
	zend_bool is_persistent;
} phpiredis_connection;

typedef struct _phpiredis_reader {
    void *reader;
    void *bufferedReply;
    char* error;
    void* status_callback;
    void* error_callback;
} phpiredis_reader;

#define PHPIREDIS_READER_STATE_COMPLETE 1
#define PHPIREDIS_READER_STATE_INCOMPLETE 2
#define PHPIREDIS_READER_STATE_ERROR 3
