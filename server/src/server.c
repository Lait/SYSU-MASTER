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
	uv_write(w, stream, &buf, 1, after_uv_write);
}  

uv_buf_t on_uv_alloc(uv_handle_t *handle, size_t suggested_size) {
    return uv_buf_init((char*) malloc(suggested_size), suggested_size);
}


void close_client(uv_stream_t* client) {
	if (!uv_is_closing((uv_handle_t*)client)) {
		uv_close((uv_handle_t*)client, after_uv_close);
	} else {
		free(client);
	}
}

void on_uv_read(uv_stream_t* client, ssize_t nread, uv_buf_t buf) {
	results[size++] = getCurrentTimeval();
	if(nread > 0) {
		write_uv_data((uv_stream_t*)client, http_respone, -1, 0);
	} else if (nread <= -1) {
		close_client(client);
	}
}

void on_connection(uv_stream_t* server, int status) { 
	//assert(server == (uv_stream_t*)&_server);  
	//results[size++] = getCurrentTimeval();
	if(status == 0) {
		uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));  
		uv_tcp_init(_loop, client);
		uv_accept((uv_stream_t*)&_server, (uv_stream_t*)client);  
		//write_uv_data((uv_stream_t*)client, http_respone, -1, 0);
		uv_read_start((uv_stream_t*)client, on_uv_alloc, on_uv_read);
	}
}

void analyse2() {
	int count = 0;

	if (size > 0) {
		count++;
	}

	int intervel = 5000;

	long last_sec = results[0].tv_sec;
	long last_usec = results[0].tv_usec;

	int  i = 1;
	while (i < size) {
		if ((results[i].tv_sec == last_sec) && (results[i].tv_usec - last_usec < intervel)) {
			count++;
			i++;
		} else if (results[i].tv_sec < last_sec) {
			printf(">> Sucks!!!!!!!!!!!\n");
			size = 0;
			return;
		} else {
			printf(">> %d req / 5ms\n", count);
			count = 1;
			if (last_usec + intervel >= 1000000) {
				last_sec++;
				last_usec += last_usec + intervel - 10000;
			} else {
				last_usec = last_usec + intervel;
			}
		}
	}
	printf(">> %d req / 5ms\n", count);
}

void analyse1() {
	if (size <= 0) {
		printf("Request arrive rate : 0 req / s.\n");
	}

	double start = (double)results[0].tv_sec * 1000 + ((double)results[0].tv_usec / 1000);
	double end = (double)results[size - 1].tv_sec * 1000  + ((double)results[size - 1].tv_usec / 1000);

	printf("Request arrive rate : %lf req / ms.\n", size / (end - start));
}

void server_start(uv_loop_t* loop, const char* ip, int port) {  
	_loop = loop;  
	uv_tcp_init(_loop, &_server);  
	uv_tcp_bind(&_server, uv_ip4_addr(ip&&ip[0]?ip:"0.0.0.0", port));  
	uv_listen((uv_stream_t*)&_server, 10240, on_connection);
	printf("Web server is runnning on %s:%d\n", ip, port);
}

//#define IP "172.18.182.65"
#define IP "127.0.0.1"

void* worker_thread(void* arg) {
	uv_loop_t* loop = (uv_loop_t*)arg;
	server_start(loop, IP, 8080);  
	uv_run(loop, UV_RUN_DEFAULT); 
}

int main(int argc, char *argv[]) {
	char input[300];
	memset(&input, 0, sizeof(input));

	pthread_t* server_thread = NULL;
	uv_loop_t* loop = uv_default_loop();
	size = 0;
	results = (struct timeval *)calloc(1000000, sizeof(struct timeval));

	printf("Waiting for actions:\n");

	if (argc > 1 && (strncmp(argv[1], "keepalive", sizeof("keepalive") - 1) == 0)) {
		keepalive = 1;
		printf("Server is keepalived !\n");
	}

	while(1) {
		//printf("Waiting for actions ==>\n");
		scanf("%s", input);

		if (strncmp(input, "end", sizeof("end") - 1) == 0) {
			printf("Shuting down.....\n");
			if (server_thread == NULL) return 0;

			if(pthread_cancel(*server_thread) == 0)  { 
				printf( ">> pthread_cancel OK.\n" ); 
			} else {
				pthread_kill(*server_thread,0);
			}
			free(results);
			//uv_loop_close(loop);
			printf(">> Bye ! \n");
			return 0;
		} else if (strncmp(input, "start", sizeof("start") - 1) == 0) {
			printf("Starting.....\n");
			server_thread = malloc(sizeof(pthread_t));
			int err = pthread_create(server_thread, NULL, worker_thread, loop);
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
			analyse1();
			//analyse2();
			size = 0;
			//memset(&results, 0, sizeof(results));
		} else {
			printf(">> ERROR:Invalid input! Try again.\n");
		}
	}
}  
