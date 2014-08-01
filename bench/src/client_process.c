#include "pbench.h"

// Create a new client for worker.
Client *client_new(Worker *worker) {
	Client *client;

	client = W_MALLOC(Client, 1);
	client->state = CLIENT_START;
	client->worker = worker;
	client->sock_watcher.fd = -1;
	client->sock_watcher.data = client;
	client->content_length = -1;
	client->buffer_offset = 0;
	client->request_offset = 0;
	client->keepalive = client->worker->config->keep_alive;
	client->chunked = 0;
	client->chunk_size = -1;
	client->chunk_received = 0;

	return client;
}

// Remove client from event loop and release it.
void client_free(Client *client) {
	if (client->sock_watcher.fd != -1) {
		ev_io_stop(client->worker->loop, &client->sock_watcher);
		shutdown(client->sock_watcher.fd, SHUT_WR);
		close(client->sock_watcher.fd);
	}

	free(client);
}



void client_set_events(Client *client, int events) {
	struct ev_loop *loop = client->worker->loop;
	ev_io *watcher = &client->sock_watcher;

	if (events == (watcher->events & (EV_READ | EV_WRITE)))
		return;

	ev_io_stop(loop, watcher);
	ev_io_set(watcher, watcher->fd, (watcher->events & ~(EV_READ | EV_WRITE)) | events);
	ev_io_start(loop, watcher);
}

void client_reset(Client *client) {
	//printf("keep alive: %d\n", client->keepalive);
	if (!client->keepalive) {
		if (client->sock_watcher.fd != -1) {
			ev_io_stop(client->worker->loop, &client->sock_watcher);
			shutdown(client->sock_watcher.fd, SHUT_WR);
			close(client->sock_watcher.fd);
			client->sock_watcher.fd = -1;
		}

		client->state = CLIENT_START;
	} else {
		// Keepalived
		client_set_events(client, EV_WRITE);
		client->state = CLIENT_WRITING;
		client->worker->stats.req_started++;
	}

	client->parser_state = PARSER_START;
	client->buffer_offset = 0;
	client->parser_offset = 0;
	client->request_offset = 0;
	client->ts_start = 0;
	client->ts_end = 0;
	client->status_success = 0;
	client->success = 0;
	client->content_length = -1;
	client->bytes_received = 0;
	client->header_size = 0;
	client->keepalive = client->worker->config->keep_alive;
	client->chunked = 0;
	client->chunk_size = -1;
	client->chunk_received = 0;
}

uint8_t client_connect(Client *client) {
	
	if (-1 == connect(client->sock_watcher.fd, client->worker->config->saddr->ai_addr, client->worker->config->saddr->ai_addrlen)) {
		switch (errno) {
			case EINPROGRESS:
			case EALREADY:
				// async connect now in progress.
				client->state = CLIENT_CONNECTING;
				return 1;
			case EISCONN:
				// Is connected!
				break;
			case EINTR:
				client_connect(client);
			default:
			{
				strerror_r(errno, client->buffer, sizeof(client->buffer));
				W_ERROR("connect() failed: %s (%d)", client->buffer, errno);
				client->state = CLIENT_ERROR;
				return 0;
			}
		}
	}

	/* successfully connected */
	client->state = CLIENT_WRITING;
	return 1;
}

// Callback for libev event loop. 
void client_io_cb(struct ev_loop *loop, ev_io *w, int revents) {
	Client *client = w->data;

	UNUSED(loop);
	UNUSED(revents);

	client_state_machine(client);
}


