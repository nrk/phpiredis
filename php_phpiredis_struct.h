#include "lib/hiredis/hiredis.h"

typedef struct _phpiredis_connection {
    redisContext *c;
    char* ip;
    int port;
} phpiredis_connection;
