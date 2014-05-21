#include "uv.h"
#include <time.h>
#include <stdlib.h>  
#include <stdio.h>  
#include <assert.h>  
#include <string.h>  
#include <memory.h>
#include <pthread.h>

atomic_t req_num = ATOMIC_INIT(1);

#include "monitor.h"

uv_tcp_t   _server;  
uv_tcp_t   _client;  
uv_loop_t* _loop; 