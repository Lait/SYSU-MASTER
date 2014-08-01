#include "pbench.h"

extern int optind, optopt; /* getopt */

static void show_help(void) {
	printf("bench <options> <url>\n");
	printf("  -n num   number of requests    (mandatory)\n");
	printf("  -t num   threadcount           (default: 1)\n");
	printf("  -c num   concurrent clients    (default: 1)\n");
	printf("  -k       keep alive            (default: no)\n");
	printf("  -6       use ipv6              (default: no)\n");
	printf("  -H str   add header to request\n");
	printf("  -h       show help and exit\n");
	printf("example: bench -n 10000 -c 10 -t 2 -k -H \"User-Agent: foo\" localhost/index.html\n\n");
}

/*
	Resolve hostname to get real ip.
*/
static struct addrinfo *resolve_host(char *hostname, uint16_t port, uint8_t use_ipv6) {
	int err;
	char port_str[6];
	struct addrinfo hints, *res, *res_first, *res_last;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	sprintf(port_str, "%d", port);

	err = getaddrinfo(hostname, port_str, &hints, &res_first);

	if (err) {
		W_ERROR("could not resolve hostname: %s", hostname);
		return NULL;
	}

	/* search for an ipv4 address, no ipv6 yet */
	res_last = NULL;
	for (res = res_first; res != NULL; res = res->ai_next) {
		if (res->ai_family == AF_INET && !use_ipv6)
			break;
		else if (res->ai_family == AF_INET6 && use_ipv6)
			break;

		res_last = res;
	}

	// No ipv4 address exist.
	if (!res) {
		freeaddrinfo(res_first);
		W_ERROR("could not resolve hostname: %s", hostname);
		return NULL;
	}

	if (res != res_first) {
		/* unlink from list and free rest */
		res_last->ai_next = res->ai_next;
		freeaddrinfo(res_first);
		res->ai_next = NULL;
	}

	return res;
}

uint64_t str_to_uint64(char *str) {
	uint64_t i;

	for (i = 0; *str; str++) {
		if (*str < '0' || *str > '9')
			return UINT64_MAX;

		i *= 10;
		i += *str - '0';
	}

	return i;
}

/*
	Config initalization.
*/
int init_config(Config* config, int argc, char *argv[]) {
	int i;
	int opt;
	
	char *host = NULL;
	uint16_t port;
	uint8_t use_ipv6;
	
	char **headers;
	uint8_t headers_num;

	headers = NULL;
	headers_num = 0;
	config->request = NULL;
	config->saddr = NULL;

	/* default settings */
	use_ipv6             = 0;
	config->thread_count = 1;
	config->concur_count = 1;
	config->req_count    = 0;
	config->keep_alive   = 0;

	while ((opt = getopt(argc, argv, ":hv6kn:t:c:H:")) != -1) {
		switch (opt) {
			case 'h':
				show_help();
				return 1;
			case '6':
				use_ipv6 = 1;
				break;
			case 'k':
				config->keep_alive = 1;
				break;
			case 'n':
				config->req_count = str_to_uint64(optarg);
				break;
			case 't':
				config->thread_count = atoi(optarg);
				break;
			case 'c':
				config->concur_count = atoi(optarg);
				break;
			case 'H':
				headers = W_REALLOC(headers, char*, headers_num+1);
				headers[headers_num] = optarg;
				headers_num++;
				break;
			case '?':
				if ('?' != optopt) W_ERROR("unkown option: -%c\n", optopt);
				show_help();
				return 1;
			case ':':
				W_ERROR("option requires an argument: -%c\n", optopt);
				show_help();
				return 1;
		}
	}

	if ((argc - optind) < 1) {
		W_ERROR("%s", "missing url argument\n");
		show_help();
		return 1;
	} else if ((argc - optind) > 1) {
		W_ERROR("%s", "too many arguments\n");
		show_help();
		return 1;
	}

	/* check for sane arguments */
	if (!config->thread_count) {
		W_ERROR("%s", "thread count has to be > 0\n");
		show_help();
		return 1;
	}

	if (!config->concur_count) {
		W_ERROR("%s", "number of concurrent clients has to be > 0\n");
		show_help();
		return 1;
	}
	if (!config->req_count) {
		W_ERROR("%s", "number of requests has to be > 0\n");
		show_help();
		return 1;
	}
	if (config->req_count == UINT64_MAX || config->thread_count > config->req_count || config->thread_count > config->concur_count || config->concur_count > config->req_count) {
		W_ERROR("%s", "insane arguments\n");
		show_help();
		return 1;
	}

	if (NULL == (config->request = forge_request(argv[optind], config->keep_alive, &host, &port, headers, headers_num))) {
		if (host)    free(host);
		if (headers) free(headers);
		return 1;
	}

	config->request_size = strlen(config->request);

	/* resolve hostname */
	if(!(config->saddr = resolve_host(host, port, use_ipv6))) {
		if (host)    free(host);
		if (headers) free(headers);
		return 1;
	}

	if (host)           free(host);
	if (headers)        free(headers);

	return 0;
}

/*
	Collect test results from all the mpi nodes.
*/
int collect_results(int numprocs) {

	int64_t total_req_count    = 0;
	uint64_t total_req_started = 0;
	uint64_t total_req_done    = 0;
	uint64_t total_req_success = 0;
	uint64_t total_req_failed  = 0;
	uint64_t total_req_error   = 0;
	uint64_t total_bytes       = 0;

	int sec, millisec, microsec;
	sec = microsec = millisec = 0;

	uint64_t args[10];
	int i;

	for(i = 0; i < numprocs; i++) {
		if (i == 0) continue;
		// BUG 0002: This may block process 0.
		MPI_Recv(args, 10, MPI_UNSIGNED_LONG, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		// Find out the max time spended.
		if (args[0] > sec) {
			sec = args[0];
			millisec = args[1];
			microsec = args[2];
		} else if (args[1] > millisec) {
			millisec = args[1];
			microsec = args[2];
		} else if (args[2] > microsec) {
			microsec = args[2];
		}

		total_req_count   += args[3];
		total_req_started += args[4];
		total_req_done    += args[5];
		total_req_success += args[6];
		total_req_failed  += args[7];
		total_req_error   += args[8];
		total_bytes       += args[9];
	}

	double dur  = (double)sec + (((double)millisec) + (((double)microsec) / 1000)) / 1000;

	uint64_t rps  = (uint64_t)(total_req_done / dur);
	uint64_t kbps = (uint64_t)(total_bytes / 1024 / dur);

	printf("\nfinished in %d sec, %d millisec and %d microsec, %"PRIu64" req/s, %"PRIu64" kbyte/s\n", sec, millisec, microsec, rps, kbps);
	printf("requests: %"PRIu64" total, %"PRIu64" started, %"PRIu64" done, %"PRIu64" succeeded, %"PRIu64" failed, %"PRIu64" errored\n",
		total_req_count, total_req_started, total_req_done, total_req_success, total_req_failed, total_req_error
	);
}

/*
	Send test result to process 0.
*/
int send_result(Stats* stats, Config* config, ev_tstamp duration) {
	int sec, millisec, microsec;

	uint64_t args[10];
	
	sec = duration;
	duration -= sec;
	duration = duration * 1000;
	millisec = duration;
	duration -= millisec;
	microsec = duration * 1000;

	args[0] = sec;
	args[1] = millisec;
	args[2] = microsec;
	args[3] = config->req_count;
	args[4] = stats->req_started;
	args[5] = stats->req_done;
	args[6] = stats->req_success;
	args[7] = stats->req_failed;
	args[8] = stats->req_error;
	args[9] = stats->bytes_total;

	MPI_Send(args, 10, MPI_UNSIGNED_LONG, 0, 0, MPI_COMM_WORLD);
}

/*
	Run benchmark.
*/
int bench(Config* config, int myid, int numprocs, char processor_name[]) {

	struct ev_loop *loop = ev_default_loop(0);
	ev_tstamp ts_start, ts_end;
	uint16_t rest_concur, rest_req;
	Worker *worker = NULL;

	if (!loop) {
		W_ERROR("%s", "could not initialize libev\n");
		return 2;
	}

	rest_concur = config->concur_count % config->thread_count;
	rest_req = config->req_count % config->thread_count;
		
	uint64_t reqs   = config->req_count / config->thread_count;
	uint16_t concur = config->concur_count / config->thread_count;

	if (rest_concur) {
		concur += 1;
		rest_concur -= 1;
	}

	if (rest_req) {
		reqs += 1;
		rest_req -= 1;
	}

	//Create a new worker.
	worker = worker_new(1, config, concur, reqs);

	if (!worker) {
		W_ERROR("%s", "failed to allocate worker or client");
		return 1;
	}

	fprintf(stderr,"Process %d of %d on %s :\n", myid, numprocs - 1, processor_name);
	fprintf(stderr, "Working on %"PRIu16" concurrent requests, %"PRIu64" total requests.\n", 
			worker->num_clients, 
			worker->stats.req_todo);

	//Start benchmark worker*******************************
	ts_start = ev_time();
	printf("## Process %d started at %lf", myid, ts_start);

	worker_run(worker);

	ts_end = ev_time();
	ev_tstamp duration = ts_end - ts_start;
	printf("## Process %d cost %lf\n", myid, duration);
	//*****************************************************

	send_result(&worker->stats, config, duration);

	ev_default_destroy();

	if (worker) free(worker);
}


int main(int argc, char *argv[]) {
	int myid, numprocs;
	int namelen;
	char processor_name[MPI_MAX_PROCESSOR_NAME]; 
	MPI_Status mpi_status;
 
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD,&myid);
	MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
	MPI_Get_processor_name(processor_name, &namelen);

	Config config;
	if (init_config(&config, argc, argv)) {
		printf("%s\n");
		MPI_Finalize();
		return 1;
	}

	//Wait for the other process to start togethor.
	MPI_Barrier(MPI_COMM_WORLD);

	//If this is process 0
	if (myid == 0) {
		collect_results(numprocs);
	} else {
		bench(&config, myid, numprocs, processor_name);
	}
	
	if (config.request) free(config.request);
	if (config.saddr)   freeaddrinfo(config.saddr);
	
	MPI_Finalize();

	return 0;
}
