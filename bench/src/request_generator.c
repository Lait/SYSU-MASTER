#include "pbench.h"
/*
Generate content of http request.
*/
char *forge_request(char *url, char keep_alive, char **host, uint16_t *port, char **headers, uint8_t headers_num) {
	char *c, *end;
	char *req;
	uint32_t len;
	uint8_t i;
	uint8_t have_user_agent;
	char *header_host;

	*host = NULL;
	*port = 0;
	header_host = NULL;

	if (strncmp(url, "http://", 7) == 0)
		url += 7;
	else if (strncmp(url, "https://", 8) == 0) {
		W_ERROR("%s", "no ssl support yet");
		url += 8;
		return NULL;
	}

	len = strlen(url);

	if ((c = strchr(url, ':'))) {
		/* found ':' => host:port */ 
		*host = W_MALLOC(char, c - url + 1);
		memcpy(*host, url, c - url);
		(*host)[c - url] = '\0';

		if ((end = strchr(c+1, '/'))) {
			*end = '\0';
			*port = atoi(c+1);
			*end = '/';
			url = end;
		} else {
			*port = atoi(c+1);
			url += len;
		}
	} else {
		*port = 80;

		if ((c = strchr(url, '/'))) {
			*host = W_MALLOC(char, c - url + 1);
			memcpy(*host, url, c - url);
			(*host)[c - url] = '\0';
			url = c;
		} else {
			*host = W_MALLOC(char, len + 1);
			memcpy(*host, url, len);
			(*host)[len] = '\0';
			url += len;
		}
	}

	if (*port == 0) {
		W_ERROR("%s", "could not parse url");
		free(*host);
		return NULL;
	}

	if (*url == '\0')
		url = "/";

	// total request size
	len = strlen("GET HTTP/1.1\r\nHost: :65536\r\nConnection: keep-alive\r\n\r\n") + 1;
	len += strlen(*host);
	len += strlen(url);

	have_user_agent = 0;
	for (i = 0; i < headers_num; i++) {
		if (strncmp(headers[i], "Host:", sizeof("Host:")-1) == 0) {
			if (header_host) {
				W_ERROR("%s", "Duplicate Host header");
				free(*host);
				return NULL;
			}
			header_host = headers[i] + 5;
			if (*header_host == ' ')
				header_host++;

			if (strlen(header_host) == 0) {
				W_ERROR("%s", "Invalid Host header");
				free(*host);
				return NULL;
			}

			len += strlen(header_host);
			continue;
		}
		len += strlen(headers[i]) + strlen("\r\n");
		if (strncmp(headers[i], "User-Agent:", sizeof("User-Agent:")-1) == 0)
			have_user_agent = 1;
	}

	if (!have_user_agent)
		len += strlen("User-Agent: benchmark/\r\n");

	req = W_MALLOC(char, len);

	strcpy(req, "GET ");
	strcat(req, url);
	strcat(req, " HTTP/1.1\r\nHost: ");
	if (header_host) {
		strcat(req, header_host);
	} else {
		strcat(req, *host);
		if (*port != 80)
			sprintf(req + strlen(req), ":%"PRIu16, *port);
	}

	strcat(req, "\r\n");

	if (!have_user_agent)
		sprintf(req + strlen(req), "User-Agent: benchmark/\r\n");

	for (i = 0; i < headers_num; i++) {
		if (strncmp(headers[i], "Host:", sizeof("Host:")-1) == 0)
			continue;
		strcat(req, headers[i]);
		strcat(req, "\r\n");
	}

	if (keep_alive)
		strcat(req, "Connection: keep-alive\r\n\r\n");
	else
		strcat(req, "Connection: close\r\n\r\n");

	return req;
}