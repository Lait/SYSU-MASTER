#include "pbench.h"

uint8_t client_parse(Client *client, int size) {
	char *end, *str;
	uint16_t status_code;

	switch (client->parser_state) {
		case PARSER_START:
			//printf("parse (START):\n%s\n", &client->buffer[client->parser_offset]);
			/* look for HTTP/1.1 200 OK */
			if (client->buffer_offset < sizeof("HTTP/1.1 200\r\n"))
				return 1;

			if (strncmp(client->buffer, "HTTP/1.1 ", sizeof("HTTP/1.1 ")-1) != 0) {
				//printf("Entry 0\n"); 
				return 0;
			}

			// now the status code
			status_code = 0;
			str = client->buffer + sizeof("HTTP/1.1 ") - 1;
			for (end = str + 3; str != end; str++) {
				if (*str < '0' || *str > '9') {
					//printf("Entry 1\n");
					return 0;
				}
					
				status_code *= 10;
				status_code += *str - '0';
			}

			if (status_code >= 200 && status_code < 300) {
				client->worker->stats.req_2xx++;
				client->status_success = 1;
			} else if (status_code < 400) {
				client->worker->stats.req_3xx++;
				client->status_success = 1;
			} else if (status_code < 500) {
				client->worker->stats.req_4xx++;
			} else if (status_code < 600) {
				client->worker->stats.req_5xx++;
			} else {
				// invalid status code
				//printf("Entry 2:Invalid status code\n");
				return 0;
			}

			// look for next \r\n
			end = strchr(end, '\r');
			if (!end || *(end+1) != '\n') {
				//printf("Entry 3\n");
				return 0;
			}
				
			client->parser_offset = end + 2 - client->buffer;
			client->parser_state = PARSER_HEADER;

		case PARSER_HEADER:
			//printf("parse (HEADER)\n");
			/* look for Content-Length and Connection header */
			while (NULL != (end = strchr(&client->buffer[client->parser_offset], '\r'))) {
				if (*(end+1) != '\n') {
					//printf("Entry 4\n");
					return 0;
				}
					
				if (end == &client->buffer[client->parser_offset]) {
					/* body reached */
					client->parser_state = PARSER_BODY;
					client->header_size = end + 2 - client->buffer;
					//printf("body reached\n");

					return client_parse(client, size - client->header_size);
				}

				*end = '\0';
				str = &client->buffer[client->parser_offset];
				//printf("checking header: '%s'\n", str);

				if (strncasecmp(str, "Content-Length: ", sizeof("Content-Length: ")-1) == 0) {
					/* content length header */
					//printf("Set client->content_length to ");
					client->content_length = str_to_uint64(str + sizeof("Content-Length: ") - 1);
					//printf("%" PRIu64 "\n", client->content_length);
				} else if (strncasecmp(str, "Connection: ", sizeof("Connection: ")-1) == 0) {
					/* connection header */
					str += sizeof("Connection: ") - 1;

					if (strncasecmp(str, "close", sizeof("close")-1) == 0)
						client->keepalive = 0;
					else if (strncasecmp(str, "keep-alive", sizeof("keep-alive")-1) == 0)
						client->keepalive = client->worker->config->keep_alive;
					else {
						//printf("Entry 5\n");
						return 0;
					}
						
				} else if (strncasecmp(str, "Transfer-Encoding: ", sizeof("Transfer-Encoding: ")-1) == 0) {
					/* transfer encoding header */
					str += sizeof("Transfer-Encoding: ") - 1;

					if (strncasecmp(str, "chunked", sizeof("chunked")-1) == 0)
						client->chunked = 1;
					else{
						//printf("Entry 6\n");
						return 0;
					}
				}


				if (*(end+2) == '\r' && *(end+3) == '\n') {
					/* body reached */
					client->parser_state = PARSER_BODY;
					client->header_size = end + 4 - client->buffer;
					client->parser_offset = client->header_size;
					//printf("body reached\n");

					return client_parse(client, size - client->header_size);
				}

				client->parser_offset = end - client->buffer + 2;
			}

			return 1;

		case PARSER_BODY:
			//printf("parse (BODY)\n");
			/* do nothing, just consume the data */
			/*printf("content-l: %"PRIu64", header: %d, recevied: %"PRIu64"\n",
			client->content_length, client->header_size, client->bytes_received);*/

			if (client->chunked) {
				int consume_max;

				str = &client->buffer[client->parser_offset];
				/*printf("parsing chunk: '%s'\n(%"PRIi64" received, %"PRIi64" size, %d parser offset)\n",
					str, client->chunk_received, client->chunk_size, client->parser_offset
				);*/

				if (client->chunk_size == -1) {
					/* read chunk size */
					client->chunk_size = 0;
					client->chunk_received = 0;
					end = str + size;

					for (; str < end; str++) {
						if (*str == ';' || *str == '\r')
							break;

						client->chunk_size *= 16;
						if (*str >= '0' && *str <= '9')
							client->chunk_size += *str - '0';
						else if (*str >= 'A' && *str <= 'Z')
							client->chunk_size += 10 + *str - 'A';
						else if (*str >= 'a' && *str <= 'z')
							client->chunk_size += 10 + *str - 'a';
						else{
							//printf("Entry 7\n");
							return 0;
						}
					}

					str = strstr(str, "\r\n");
					if (!str) {
						//printf("Entry 8\n");
						return 0;
					}
	
					str += 2;

					//printf("---------- chunk size: %"PRIi64", %d read, %d offset, data: '%s'\n", client->chunk_size, size, client->parser_offset, str);

					if (client->chunk_size == 0) {
						/* chunk of size 0 marks end of content body */
						client->state = CLIENT_END;
						client->success = client->status_success ? 1 : 0;
						return 1;
					}

					size -= str - &client->buffer[client->parser_offset];
					client->parser_offset = str - client->buffer;
				}

				/* consume chunk till chunk_size is reached */
				consume_max = client->chunk_size - client->chunk_received;

				if (size < consume_max)
					consume_max = size;

				client->chunk_received += consume_max;
				client->parser_offset += consume_max;

				//printf("---------- chunk consuming: %d, received: %"PRIi64" of %"PRIi64", offset: %d\n", consume_max, client->chunk_received, client->chunk_size, client->parser_offset);

				if (client->chunk_received == client->chunk_size) {
					if (client->buffer[client->parser_offset] != '\r' || client->buffer[client->parser_offset+1] != '\n') {
						//printf("Entry 9\n");
						return 0;
					}
						
					/* got whole chunk, next! */
					//printf("---------- got whole chunk!!\n");
					client->chunk_size = -1;
					client->chunk_received = 0;
					client->parser_offset += 2;
					consume_max += 2;

					/* there is stuff left to parse */
					if (size - consume_max > 0)
						return client_parse(client, size - consume_max);
				}

				client->parser_offset = 0;
				client->buffer_offset = 0;

				return 1;
			} else {
				/* not chunked, just consume all data till content-length is reached */
				client->buffer_offset = 0;

				if (client->content_length == -1) {
					//printf("Entry 10\n");
					return 0;
				}
					

				if (client->bytes_received == (uint64_t) (client->header_size + client->content_length)) {
					/* full response received */
					client->state = CLIENT_END;
					client->success = client->status_success ? 1 : 0;
				}
			}
			return 1;
	}
	return 1;
}