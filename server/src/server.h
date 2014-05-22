#include "uv.h"
#include <time.h>
#include <stdlib.h>  
#include <stdio.h>  
#include <assert.h>  
#include <string.h>  
#include <memory.h>
#include <pthread.h>
#include <sys/time.h>


uv_tcp_t     _server;  
uv_tcp_t     _client;  
uv_loop_t* _loop;


long size;
struct timeval * results;

const char* http_respone = "HTTP/1.1 200 OK\r\n"  
    "Content-Type: text/html;charset=utf-8\r\n"  
    "Content-Length: 11\r\n"  
    "\r\n"  
    "Hello world";