#include "uv.h"
#include <stdlib.h>  
#include <stdio.h>  
#include <assert.h>  
#include <string.h>  
#include <memory.h>  
  
uv_tcp_t   _server;  
uv_tcp_t   _client;  
uv_loop_t* _loop;  
  
static void on_connection(uv_stream_t* server, int status);  
  
void server_start(uv_loop_t* loop, const char* ip, int port) {  
    _loop = loop;  
    uv_tcp_init(_loop, &_server);  
    uv_tcp_bind(&_server, uv_ip4_addr(ip&&ip[0]?ip:"0.0.0.0", port));  
    uv_listen((uv_stream_t*)&_server, 1024, on_connection);
	printf("Web server is runnning on %s:%d\n", ip, port);
}
  
static void after_uv_close(uv_handle_t* handle) {  
    free(handle); //uv_tcp_t* client  
}  
  
static void after_uv_write(uv_write_t* w, int status) {  
    if(w->data)  
        free(w->data);  
    uv_close((uv_handle_t*)w->handle, after_uv_close); //close client  
    free(w);  
}  
  
static void write_uv_data(uv_stream_t* stream, const void* data, unsigned int len, int need_copy_data) {  
    if(data == NULL || len == 0) return;  
    if(len ==(unsigned int)-1)  
        len = strlen(data);  
  
    void* newdata  = (void*)data;  
    if(need_copy_data) {  
        newdata = malloc(len);  
        memcpy(newdata, data, len);  
    }  
  
    uv_buf_t buf = uv_buf_init(newdata, len);  
    uv_write_t* w = (uv_write_t*)malloc(sizeof(uv_write_t));  
    w->data = need_copy_data ? newdata : NULL;  
    //free w and w->data in after_uv_write()  
    uv_write(w, stream, &buf, 1, after_uv_write);  
}  
  
static const char* http_respone = "HTTP/1.1 200 OK\r\n"  
    "Content-Type: text/html;charset=utf-8\r\n"  
    "Content-Length: 11\r\n"  
    "\r\n"  
    "Hello world";  
  
static void on_connection(uv_stream_t* server, int status) {  
    assert(server == (uv_stream_t*)&_server);  
    if(status == 0) {  
        uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));  
        uv_tcp_init(_loop, client);  
        uv_accept((uv_stream_t*)&_server, (uv_stream_t*)client);  
        write_uv_data((uv_stream_t*)client, http_respone, -1, 0);  
        //close client after uv_write, and free it in after_uv_close()  
    }  
}

int main() {  
    server_start(uv_default_loop(), "127.0.0.1", 8080);  
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);  
}  
