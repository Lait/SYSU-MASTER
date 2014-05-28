#include "monitor.h"

long last = 

void count() {

}

timeval getCurrentTimeAsLong() {
    struct timeval val;
    struct timeval zone;
    gettimeofday(&val, &zone);
    return val;
}  

void* monitor_thread(void* arg) {
	signal(SIGALRM, printMsg);	
}