struct Worker {
	uint8_t id;
	Config*config;
	struct ev_loop *loop;
	char *request;
	Client **clients;
	uint16_t num_clients;
	Stats stats;
	uint64_t progress_interval;
	struct timeval * sendlog;
};

//Definition of public functions.
Worker*   worker_new(uint8_t id, Config *config, uint16_t num_clients, uint64_t num_requests);
void          worker_free(Worker *worker);
void*        worker_run(void* arg);
