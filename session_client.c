#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#define __USE_GNU
#include <dlfcn.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

static int (*accept_real)(int, struct sockaddr *, socklen_t *) = NULL;
static int (*bind_real)(int, const struct sockaddr *, socklen_t) = NULL;
static int (*connect_real)(int, const struct sockaddr *, socklen_t) = NULL;

static int connsock = -1;
static struct sockaddr connaddr;
static socklen_t connlen;
static char *seshid = NULL;

#include <time.h>
static long timespec_nano(struct timespec *t)
{
	return t->tv_nsec + t->tv_sec*1000000000;
}

static void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}

static long timespec_diff_nano(struct timespec *start, struct timespec *end)
{
	struct timespec diff;
	timespec_diff(start, end, &diff);
	return timespec_nano(&diff);
}

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	if(addr->sa_family == AF_INET) {
		struct timespec start, end, diff;
		clock_gettime(CLOCK_MONOTONIC, &start);
		connsock = fd;
		memcpy(&connaddr, addr, len);
		connlen = len;

		int ss = socket(AF_INET, SOCK_STREAM, 0);
		if(ss == -1) perror("socket");
		struct sockaddr serveraddr;
		memcpy(&serveraddr, &connaddr, connlen);
		struct sockaddr_in *sin = (void *)&serveraddr;
		sin->sin_port = htons(9001);
		if(connect_real(ss, (struct sockaddr *)&serveraddr,
					sizeof(serveraddr)) == -1) {
			perror("reconnect");
			exit(1);
		}
		dprintf(ss, "SESSION ESTABLISH\n");
		char *buffer = NULL;
		size_t blen = 0;
		FILE *peer = fdopen(ss, "r+");
		getline(&buffer, &blen, peer);
		seshid = buffer;
	//	fprintf(stderr, "client seshid got %s\n", buffer);
		close(ss);
		clock_gettime(CLOCK_MONOTONIC, &end);
		timespec_diff(&start, &end, &diff);
		fprintf(stderr, "BENCH client connect time: %ld\n", timespec_nano(&diff));
	}
	int r = connect_real(fd, addr, len);
	if(seshid) {
		fprintf(stderr, "cl sending seshid %d %d\n", r, fd);
		dprintf(fd, "%s\n", seshid);
	}
	return r;
}

int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	return bind_real(fd, addr, len);
}

int accept(int fd, struct sockaddr *addr, socklen_t *len)
{
	return accept_real(fd, addr, len);
}

static void _handle_sig_reconn(int sig)
{
	(void)sig;

	fprintf(stderr, "Reconnecting via signal\n");

	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	int ss = socket(AF_INET, SOCK_STREAM, 0);
	if(ss == -1) perror("socket");
	struct sockaddr serveraddr;
	memcpy(&serveraddr, &connaddr, connlen);
	struct sockaddr_in *sin = (void *)&serveraddr;
	sin->sin_port = htons(9001);
	if(connect_real(ss, (struct sockaddr *)&serveraddr,
				sizeof(serveraddr)) == -1) {
		perror("reconnect");
		exit(1);
	}
	dprintf(ss, "SESSION RECONNECT\n");
	char *buffer = NULL;
	size_t blen = 0;
	FILE *peer = fdopen(ss, "r+");
	getline(&buffer, &blen, peer);
	dprintf(ss, "%s\n", seshid);
	dup2(ss, connsock);
	close(ss);
	clock_gettime(CLOCK_MONOTONIC, &end);
	fprintf(stderr, "BENCH reconnect client time: %ld\n", timespec_diff_nano(&start, &end));
}

__attribute__((constructor)) static void __init_session(void)
{
	bind_real = dlsym(RTLD_NEXT, "bind");
	accept_real = dlsym(RTLD_NEXT, "accept");
	connect_real = dlsym(RTLD_NEXT, "connect");
	struct sigaction sa2 = {
		.sa_handler = _handle_sig_reconn,
		.sa_flags = SA_RESTART,
	};

	sigaction(SIGUSR2, &sa2, NULL);
	fprintf(stderr, "client PID = %d\n", getpid());
}

