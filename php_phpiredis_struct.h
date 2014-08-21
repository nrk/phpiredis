#include "hiredis/hiredis.h"

typedef struct _phpiredis_reader {
    void *reader;
    void *bufferedReply;
    char* error;
    void* status_callback;
    void* error_callback;
} phpiredis_reader;

typedef struct _phpiredis_connection {
    redisContext *c;
    char* ip;
    int port;
    zend_bool is_persistent;
    int stream_index;
    struct _phpiredis_reader *reader;
    int reader_index;
} phpiredis_connection;

#define PHPIREDIS_READER_STATE_COMPLETE 1
#define PHPIREDIS_READER_STATE_INCOMPLETE 2
#define PHPIREDIS_READER_STATE_ERROR 3
