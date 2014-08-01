#ifndef PBENCH_H
#define PBENCH_H 1

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netdb.h>

#include <ev.h>

#define CLIENT_BUFFER_SIZE 32 * 2048

#define W_MALLOC(t, n) ((t*) calloc((n), sizeof(t)))
#define W_REALLOC(p, t, n) ((t*) realloc(p, (n) * sizeof(t)))
#define W_ERROR(f, ...) fprintf(stderr, "error: " f "\n", __VA_ARGS__)
#define UNUSED(x) ( (void)(x) )

struct Config;
typedef struct Config Config;
struct Stats;
typedef struct Stats Stats;
struct Worker;
typedef struct Worker Worker;
struct Client;
typedef struct Client Client;


#include "stats.h"
#include "config.h"

#include "worker.h"
#include "client.h"
#include "mpi.h"

char *forge_request(char *url, char keep_alive, char **host, uint16_t *port, char **headers, uint8_t headers_num);

#endif
