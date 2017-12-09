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
static int (*accept_real)(int, struct sockaddr *, socklen_t *) = NULL;
static int (*bind_real)(int, const struct sockaddr *, socklen_t) = NULL;
static int (*connect_real)(int, const struct sockaddr *, socklen_t) = NULL;
static int (*sigaction_real)(int, const struct sigaction *restrict act,
		struct sigaction *restrict oact) = NULL;
static int (*close_real)(int) = NULL;
static int (*shutdown_real)(int, int) = NULL;

static short port = 0;

struct session {
	int fd;
	char sid[32];
	struct session *next, *prev;
};

struct session session_list = { .next = &session_list, .prev = &session_list };

static struct session *session_new(int fd)
{
	struct session *s = malloc(sizeof(*s));
	s->fd = fd;
	s->next = session_list.next;
	s->prev = &session_list;
	s->prev->next = s;
	s->next->prev = s;
	for(int i=0;i<31;i++) {
		s->sid[i] = (rand() % 26) + 'a';
	}
	s->sid[31] = 0;
	fprintf(stderr, "sesh: created new session for %d: %s\n", s->fd, s->sid);
	return s;
}

static struct session *session_find(const char *id)
{
	for(struct session *s = session_list.next;
			s != &session_list;s=s->next) {
		if(!memcmp(id, s->sid, 31)) {
			return s;
		}
	}
	return NULL;
}

static struct session *session_find_fd(int fd)
{
	for(struct session *s = session_list.next;
			s != &session_list;s=s->next) {
		if(s->fd == fd) {
			return s;
		}
	}
	return NULL;
}

static void session_close(struct session *s)
{
	if(s == NULL) return;
	s->prev->next = s->next;
	s->next->prev = s->prev;
	free(s);
}

int shutdown(int fd, int how)
{
	session_close(session_find_fd(fd));
	return shutdown_real(fd, how);
}

int close(int fd)
{
	session_close(session_find_fd(fd));
	return close_real(fd);
}

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	return connect_real(fd, addr, len);
}

int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	if(addr->sa_family == AF_INET) {
		struct sockaddr_in *sin = (void *)addr;
		port = ntohs(sin->sin_port);
		fprintf(stderr, "bound to port %d\n", port);
	}

	return bind_real(fd, addr, len);
}

int accept(int fd, struct sockaddr *addr, socklen_t *len)
{
	int cl = accept_real(fd, addr, len);
	
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	char *buffer = NULL;
	size_t blen = 0;
	FILE *peer = fdopen(cl, "r+");
	getline(&buffer, &blen, peer);
	struct session *s = session_find(buffer);
//	fprintf(stderr, "accept: got session %s: %p\n", buffer, s);
	if(s) {
//		fprintf(stderr, "accept: assign fd=%d\n", cl);
		s->fd = cl;
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	fprintf(stderr, "BENCH server accept time: %ld\n", timespec_diff_nano(&start, &end));

	return cl;
}

static void (*(handler[128]))(int) = {NULL};

int sigaction(int sig, const struct sigaction *restrict act,
           struct sigaction *restrict oact)
{
	if(act && sig == SIGUSR1) {
		handler[sig] = act->sa_handler;
		return 0;
	}
	return sigaction_real(sig, act, oact);
}

static pthread_t mainthrd;
static void reconnect(int cl)
{
	char *buffer = NULL;
	size_t blen = 0;
	FILE *peer = fdopen(cl, "r+");
	getline(&buffer, &blen, peer);
	fprintf(stderr, "ctl: <%s>\n", buffer);
	if(!strcmp(buffer, "SESSION ESTABLISH\n")) {
		struct session *s = session_new(-1);
		fprintf(stderr, "sending seshid %s\n", s->sid);
		dprintf(cl, "%s", s->sid);
	} else if(!strcmp(buffer, "SESSION RECONNECT\n")) {
		struct timespec start, end;
		clock_gettime(CLOCK_MONOTONIC, &start);
		pthread_kill(mainthrd, SIGUSR1);
		dprintf(cl, "ACK\n");
	//	fprintf(stderr, "searching for session...\n");
		getline(&buffer, &blen, peer);
	//	fprintf(stderr, "Got:: %s\n", buffer);
		struct session *s = session_find(buffer);
		if(s == NULL) {
			fprintf(stderr, "could not find: %s\n", buffer);
			pthread_kill(mainthrd, SIGUSR2);
			return;
		}
		fprintf(stderr, "reconnecting session %d\n", s->fd);
		if(dup2(cl, s->fd) == -1) {
			perror("dup2");
		}
		pthread_kill(mainthrd, SIGUSR2);
		clock_gettime(CLOCK_MONOTONIC, &end);
		fprintf(stderr, "BENCH server reconnect time: %ld\n", timespec_diff_nano(&start, &end));
	}
}

static void *_sess_main(void *arg)
{
	int sock = (int)(long)arg;
	fprintf(stderr, "Session: backgrounding (%d)\n", sock);
	int cl;
	struct sockaddr_in client;
	socklen_t clen = sizeof(client);
	while((cl = accept_real(sock, (struct sockaddr *)&client,
					&clen)) != -1) {
		reconnect(cl);
		close(cl);
	}

	fprintf(stderr, "Session: thread exiting early\n");
	return NULL;
}

static void _handle_sig_seshre(int sig)
{
	sigset_t wait_mask;
	sigfillset(&wait_mask);
	sigdelset(&wait_mask, SIGUSR2);
	sigsuspend(&wait_mask);
	if(handler[sig]) {
		handler[sig](sig);
	}
}

static void _handler_sigres(int s) {(void)s;}

static void _init_sesh(void)
{
	bind_real = dlsym(RTLD_NEXT, "bind");
	accept_real = dlsym(RTLD_NEXT, "accept");
	connect_real = dlsym(RTLD_NEXT, "connect");
	sigaction_real = dlsym(RTLD_NEXT, "sigaction");
	close_real = dlsym(RTLD_NEXT, "close");
	shutdown_real = dlsym(RTLD_NEXT, "shutdown");
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	int one = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
			(const void *)&one, sizeof(int));

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)9001);

	if(bind_real(sockfd, (struct sockaddr *)&serveraddr,
				sizeof(serveraddr)) == -1) {
		perror("bind_real");
	}
	listen(sockfd, 4);

	mainthrd = pthread_self();
	pthread_t thrd;
	pthread_create(&thrd, NULL, _sess_main, (void *)(long)sockfd);

	struct sigaction sa = {
		.sa_handler = _handle_sig_seshre,
		.sa_flags = SA_RESTART,
	};
	sigfillset(&sa.sa_mask);
	sigaction_real(SIGUSR1, &sa, NULL);
	struct sigaction sa2 = {
		.sa_handler = _handler_sigres,
	};
	sigfillset(&sa2.sa_mask);
	sigaction_real(SIGUSR2, &sa2, NULL);
}

__attribute__((constructor)) static void __init_lib(void)
{
	_init_sesh();
}

