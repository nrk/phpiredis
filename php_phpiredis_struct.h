#include "lib/hiredis/hiredis.h"

typedef struct _phpiredis_connection {
    redisContext *c;
    char* ip;
    int port;
} phpiredis_connection;

typedef struct _phpiredis_reader {
    void *reader;
    void *bufferedReply;
    char* error;
} phpiredis_reader;
