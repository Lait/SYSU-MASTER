#include "server.h"

void on_connection(uv_stream_t* server, int status);  
 
void server_start(uv_loop_t* loop, const char* ip, int port) {  
	_loop = loop;  
	uv_tcp_init(_loop, &_server);  
	uv_tcp_bind(&_server, uv_ip4_addr(ip&&ip[0]?ip:"0.0.0.0", port));  
	uv_listen((uv_stream_t*)&_server, 1024, on_connection);
	//printf("Web server is runnning on %s:%d\n", ip, port);
}
  
void after_uv_close(uv_handle_t* handle) {  
	free(handle); //uv_tcp_t* client  
}  
  
void after_uv_write(uv_write_t* w, int status) {  
	if(w->data)  
		free(w->data);
	uv_close((uv_handle_t*)w->handle, after_uv_close); //close client  
	free(w);  
}  
  
void write_uv_data(uv_stream_t* stream, const void* data, unsigned int len, int need_copy_data) {  
	if(data == NULL || len == 0) return;  
	if(len ==(unsigned int) - 1)  
		len = strlen(data);  
  
	void* newdata  = (void*)data;  
	if(need_copy_data) {  
		newdata = malloc(len);  
		memcpy(newdata, data, len);  
	}  
  
	uv_buf_t buf = uv_buf_init(newdata, len);  
	uv_write_t* w = (uv_write_t*)malloc(sizeof(uv_write_t));  
	w->data = need_copy_data ? newdata : NULL;  
	uv_write(w, stream, &buf, 1, after_uv_write);  
}  
  
struct timeval getCurrentTimeval() {
	struct timeval val;
	struct timezone zone;
	gettimeofday(&val, &zone);
	return val;
}
  
void on_connection(uv_stream_t* server, int status) { 
	results[size++] = getCurrentTimeval();
	assert(server == (uv_stream_t*)&_server);  
	if(status == 0) { 
		uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));  
		uv_tcp_init(_loop, client);  
		uv_accept((uv_stream_t*)&_server, (uv_stream_t*)client);  
		write_uv_data((uv_stream_t*)client, http_respone, -1, 0);  
	}
}

void* worker_thread(void* arg) {
	server_start(uv_default_loop(), "127.0.0.1", 8080);  
	uv_run(uv_default_loop(), UV_RUN_DEFAULT); 
}

void analyse() {
	int count = 0;
	int i = 0;
	if (size > 0) {
		count++;
	}
	for (i = 0; i < size - 1; i++) {
		if (results[i + 1].tv_sec == results[i].tv_sec) {
			count++;
		} else {
			printf(">> %d req/s\n", count);
			count = 0;
		}
	}
	printf(">> %d req/s\n", count);
	size = 0;
}

void printAll() {
	int i = 0;
	for (i = 0; i < size; i++) {
		printf("%ld ", results[i].tv_sec);
	}
	printf("\n");
}

int main() {
	char input[300];
	memset(&input, 0, sizeof(input));

	pthread_t* server_thread = NULL;
	size = 0;
	results = (struct timeval *)calloc(100000, sizeof(struct timeval));
	printf("Waiting for actions:\n");

	while(1) {
		//printf("Waiting for actions ==>\n");
		scanf("%s", input);

		if (strncmp(input, "end", sizeof("end") - 1) == 0) {
			printf(">> Shuting down.....\n");
			if (server_thread == NULL) return 0;

			if(pthread_cancel(*server_thread) == 0)  { 
				printf( ">> pthread_cancel OK.\n" ); 
			} else {
				pthread_kill(*server_thread,0);
			}
			free(results);
			return 0;
		} else if (strncmp(input, "start", sizeof("start") - 1) == 0) {
			printf(">> Starting.....\n");
			server_thread = malloc(sizeof(pthread_t));
			int err = pthread_create(server_thread, NULL, worker_thread, NULL);
			if (err != 0) {
				printf(">> ERROR: Can not start server!\n");
				return -1;
			}
			printf(">> Web server is started!\n");
		}  else if (strncmp(input, "analyse", sizeof("analyse") - 1)  == 0) {
			//todo
			printf(">> Analysing ....\n");
			printf(">> %ld of totol request:\n", size);

			//Debug
			//printAll();

			analyse();

			//memset(&results, 0, sizeof(results));
		} else {
			printf(">> ERROR:Invalid input! Try again.\n");
		}
	}
}  
