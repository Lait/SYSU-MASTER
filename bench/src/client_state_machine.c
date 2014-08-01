#include "pbench.h"

/*

 */
void state_process_start(Client* client) {
	int r = 0;
	Config *config = client->worker->config;

	client->worker->stats.req_started++;
	
	do {
		r = socket(config->saddr->ai_family, config->saddr->ai_socktype, config->saddr->ai_protocol);
	} while (-1 == r && errno == EINTR);

	if (-1 == r) {
		// BUG 0001: Failed request will block the entire system, so Evil!!!! Fix it some day.
		client->state = CLIENT_ERROR;
		strerror_r(errno, client->buffer, sizeof(client->buffer));
		W_ERROR("socket() failed: %s (%d)", client->buffer, errno);
		client_state_machine(client);
		return;
	}

	// Set socket r as non-blocking. 
	fcntl(r, F_SETFL, O_NONBLOCK | O_RDWR);

	ev_init(&client->sock_watcher, client_io_cb);
	ev_io_set(&client->sock_watcher, r, EV_WRITE);
	ev_io_start(client->worker->loop, &client->sock_watcher);

	if (!client_connect(client)) {
		client_state_machine(client);
	} else {
		//Waiting for server SYN-ACK event.
		client_set_events(client, EV_WRITE);
	}
}

/*

 */
void state_process_connecting(Client* client) {
	client_connect(client);
	client_state_machine(client);
}

/*

 */
void state_process_writing(Client* client) {
	int r = 0;
	Config *config = client->worker->config;

	while(1) {
		r = write(client->sock_watcher.fd, 
			&config->request[client->request_offset], 
			config->request_size - client->request_offset);

		if (r == -1) {
			if (errno == EINTR) continue; 

			strerror_r(errno, client->buffer, sizeof(client->buffer));
			W_ERROR("write() failed: %s (%d)", client->buffer, errno);
			client->state = CLIENT_ERROR;
			client_state_machine(client);
			return;
		} else if (r != 0) {
			// Finish reading.
			client->request_offset += r;
			if (client->request_offset == config->request_size) {
				/* whole request was sent, start reading */
				client->state = CLIENT_READING;
				client_set_events(client, EV_READ);
			}
			return;
		} else {
			// Disconnect from SUT.
			client->state = CLIENT_END;
			client_state_machine(client);
			return;
		}
	}
}

/*

 */
void state_process_reading(Client* client) {
	int r = 0;
	Config *config = client->worker->config;

	while (1) {
		r = read(client->sock_watcher.fd, 
			&client->buffer[client->buffer_offset], 
			sizeof(client->buffer) - client->buffer_offset - 1);

		if (r == -1) {
			if (errno == EINTR) continue;

			strerror_r(errno, client->buffer, sizeof(client->buffer));
			W_ERROR("read() failed: %s (%d)", client->buffer, errno);
			client_state_machine(client);
			return;
		} else if (r != 0) {
			/* success */
			client->bytes_received += r;
			client->buffer_offset += r;
			client->worker->stats.bytes_total += r;

			if (client->buffer_offset >= sizeof(client->buffer)) {
				/* too big response header */
				client->state = CLIENT_ERROR;
				client_state_machine(client);
				return;
			}
			client->buffer[client->buffer_offset] = '\0';
			
			if (!client_parse(client, r)) {
				client->state = CLIENT_ERROR;
				//printf("parser failed\n");
				break;
			} else {
				if (client->state == CLIENT_END) {
					client_state_machine(client);
				} 
				return;
			}
		} else {
			/* disconnect */
			if (client->parser_state == PARSER_BODY && !client->keepalive && client->status_success
				&& !client->chunked && client->content_length == -1) {
				client->success = 1;
				client->state = CLIENT_END;
			} else {
				client->state = CLIENT_ERROR;
				//printf("Disconnected from server!\n");
			}

			client_state_machine(client);
		}
	}
}

/*

 */
void state_process_error(Client* client) {
	//printf("client error\n");
	client->worker->stats.req_error++;
	client->keepalive = 0;
	client->success = 0;
	client->state = CLIENT_END;
	client_state_machine(client);
}

/*

 */
void state_process_end(Client* client) {
	/* update worker stats */
	client->worker->stats.req_done++;

	if (client->success) {
		client->worker->stats.req_success++;
		client->worker->stats.bytes_body += client->bytes_received - client->header_size;
	} else {
		client->worker->stats.req_failed++;
	}

	if (client->worker->stats.req_started == client->worker->stats.req_todo) {
		/* this worker has started all requests */
		client->keepalive = 0;
		client_reset(client);

		if (client->worker->stats.req_done == client->worker->stats.req_todo) {					
			ev_unref(client->worker->loop);
		}
	} else {
		client_reset(client);
		client_state_machine(client);
	}
}

// Tcp state machine.
void client_state_machine(Client *client) {
	//printf("state: %d\n", client->state);
	switch (client->state) {
		case CLIENT_START: 
			state_process_start(client);
			break;

		case CLIENT_CONNECTING:
			state_process_connecting(client);
			break;

		case CLIENT_WRITING:
			state_process_writing(client);
			break;

		case CLIENT_READING:
			state_process_reading(client);
			break;

		case CLIENT_ERROR:
			state_process_error(client);
			break;

		case CLIENT_END:
			state_process_end(client);
			break;
	}
}