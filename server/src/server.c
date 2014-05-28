#include "server.h"

struct timeval getCurrentTimeval() {
	struct timeval val;
	struct timezone zone;
	gettimeofday(&val, &zone);
	return val;
} 
 
void after_uv_close(uv_handle_t* handle) { 
	//free(handle->data);
	free(handle); //uv_tcp_t* client 
}  

void after_uv_write(uv_write_t* w, int status) {
	if (!keepalive) {
		if(w->data)  
			free(w->data);
		uv_close((uv_handle_t*)w->handle, after_uv_close); //close client  
		free(w); 
	}
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
	/*
	int i = 0;
	for (i = 0; i < 10000; i++) {
		int j = i;
		if (i > 1000000) {
			printf("Impossibel!\n");
		}
	}*/
	uv_write(w, stream, &buf, 1, after_uv_write);
}  

uv_buf_t on_uv_alloc(uv_handle_t *handle, size_t suggested_size) {
    return uv_buf_init((char*) malloc(suggested_size), suggested_size);
}


void close_client(uv_stream_t* client) {
	uv_close((uv_handle_t*)client, after_uv_close);
}

void on_uv_read(uv_stream_t* client, ssize_t nread, uv_buf_t buf) {
	//results[size++] = getCurrentTimeval();
	if(nread > 0) {
		write_uv_data((uv_stream_t*)client, http_respone, -1, 0);
	} else if(nread == -1) {
		close_client(client);
	}
}

void on_connection(uv_stream_t* server, int status) { 
	assert(server == (uv_stream_t*)&_server);  
	results[size++] = getCurrentTimeval();
	if(status == 0) {
		uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));  
		uv_tcp_init(_loop, client);
		uv_accept((uv_stream_t*)&_server, (uv_stream_t*)client);  
		//write_uv_data((uv_stream_t*)client, http_respone, -1, 0);
		uv_read_start((uv_stream_t*)client, on_uv_alloc, on_uv_read);
	}
}

void server_start(uv_loop_t* loop, const char* ip, int port) {  
	_loop = loop;  
	uv_tcp_init(_loop, &_server);  
	uv_tcp_bind(&_server, uv_ip4_addr(ip&&ip[0]?ip:"0.0.0.0", port));  
	uv_listen((uv_stream_t*)&_server, 10240, on_connection);
	printf("Web server is runnning on %s:%d\n", ip, port);
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

	long last_sec = results[0].tv_sec;
	long last_usec = results[0].tv_usec;

	for (i = 1; i < size; i++) {
		if ((results[i].tv_sec == last_sec) && (results[i].tv_usec - last_usec < 50000)) {
			count++;
		} else {
			printf(">> %d req / 50ms\n", count);
			count = 1;
			last_sec = results[i].tv_sec;
			last_usec = results[i].tv_usec;
		}
	}
	printf(">> %d req / 50ms\n", count);
	size = 0;
}

void printAll() {
	int i = 0;
	printf(">> ");
	for (i = 0; i < size; i++) {
		printf("%ld:%ld ", results[i].tv_sec, results[i].tv_usec);
	}
	printf("\n");
}

int main(int argc, char *argv[]) {
	char input[300];
	memset(&input, 0, sizeof(input));

	pthread_t* server_thread = NULL;
	size = 0;
	results = (struct timeval *)calloc(100000, sizeof(struct timeval));
	printf("Waiting for actions:\n");

	if (argc > 1 && (strncmp(argv[1], "keepalive", sizeof("keepalive") - 1) == 0)) {
		keepalive = 1;
	}

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
