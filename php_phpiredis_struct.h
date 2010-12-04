#include "lib/hiredis/hiredis.h"

typedef struct _phpiredis_connection {
    redisContext *c
} phpiredis_connection;
