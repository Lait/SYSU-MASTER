struct Config {
	uint64_t req_count;
	uint8_t thread_count;
	uint16_t concur_count;
	uint8_t keep_alive;

	char *request;
	uint32_t request_size;
	struct addrinfo *saddr;
};