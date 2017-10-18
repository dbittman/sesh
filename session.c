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

/* TODO:
 *   - exchange session data instead of the sleep
 *   - support multiple things instead of just one on port 9001 (shared daemon w/ shmem?)
 *   - follow forks to inject into child process a sesh_main thread.
 */

static int (*accept_real)(int, struct sockaddr *, socklen_t *) = NULL;
static int (*bind_real)(int, const struct sockaddr *, socklen_t) = NULL;
static int (*connect_real)(int, const struct sockaddr *, socklen_t) = NULL;

static short port = 0;

struct session {
	int fd;

} sessions[1024];

static int connsock = -1;
static struct sockaddr connaddr;
static socklen_t connlen;

int connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	fprintf(stderr, "got here...\n");
	if(addr->sa_family == AF_INET) {
	//	const char *s = getenv("SESH");
	//	if(s) {
	//		fprintf(stderr, "Attempting to reconnect...\n");
	//		struct sockaddr_in *sin = (void *)addr;
	//		sin->sin_port = htons(9001);
	//	} else {
		connsock = fd;
		memcpy(&connaddr, addr, len);
		connlen = len;
	}
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
	if(sessions[cl].fd == -1) {
		sessions[cl].fd = cl;
		fprintf(stderr, "Est. session %d\n", cl);
	}

	return cl;
}

static pthread_t mainthrd;
static void reconnect(int cl)
{
	fprintf(stderr, "searching for session...\n");
	struct session *s = NULL;
	for(int i=0;i<1024;i++) {
		if(sessions[i].fd >= 0) {
			fprintf(stderr, "found!\n");
			s = &sessions[i];
		}
	}
	if(s == NULL) return;

	fprintf(stderr, "reconnecting session %d\n", s->fd);
	pthread_kill(mainthrd, SIGSTOP);
	perror("...");
	close(s->fd);
	dup2(cl, s->fd);
	//pthread_kill(mainthrd, SIGUSR1);
	pthread_kill(mainthrd, SIGCONT);
}

static void *_sess_main(void *arg)
{
	int sock = (int)(long)arg;
	fprintf(stderr, "Session: backgrounding (%d)\n", sock);
	int cl;
	struct sockaddr_in client;
	socklen_t clen = sizeof(client);
	while((cl = accept_real(sock, (struct sockaddr *)&client, &clen)) != -1) {
		reconnect(cl);
		close(cl);
	}

	fprintf(stderr, "Session: thread exiting early\n");
	return NULL;
}

static void _handle_sig_reconn(int sig)
{
	(void)sig;

	fprintf(stderr, "Reconnecting via signal\n");

	int ss = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr serveraddr;
	memcpy(&serveraddr, &connaddr, connlen);
	struct sockaddr_in *sin = (void *)&serveraddr;
	sin->sin_port = htons(9001);
	if(connect_real(ss, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
		perror("reconnect");
		exit(1);
	}
	sleep(1); //TODO: need to actually exchange session info, agree to close old sockets, etc.
	close(connsock);
	dup2(ss, connsock);
}

__attribute__((constructor)) static void __init_session(void)
{
	bind_real = dlsym(RTLD_NEXT, "bind");
	accept_real = dlsym(RTLD_NEXT, "accept");
	connect_real = dlsym(RTLD_NEXT, "connect");
	for(int i=0;i<1024;i++) sessions[i].fd = -1;
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	int one = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(int));

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)9001);

	if(bind_real(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
		perror("bind_real");
	}
	listen(sockfd, 4);

	mainthrd = pthread_self();
	pthread_t thrd;
	pthread_create(&thrd, NULL, _sess_main, (void *)(long)sockfd);

	struct sigaction sa = {
		.sa_handler = SIG_IGN,
		.sa_flags = SA_RESTART,
	};
	sigaction(SIGUSR1, &sa, NULL);
	struct sigaction sa2 = {
		.sa_handler = _handle_sig_reconn,
		.sa_flags = SA_RESTART,
	};

	sigaction(SIGUSR2, &sa2, NULL);
}

