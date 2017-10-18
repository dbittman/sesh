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

#if 0
#define WRAPPER(ret, name, ...) \
	ret name ( __VA_ARGS__ ) { \
		static ret (* name ## _real ) ( __VA_ARGS__ ) = NULL; \
		if(name ## _real == NULL) name ## _real = dlsym(RTLD_NEXT, #name);

WRAPPER(int, bind, const struct sockaddr *addr, socklen_t len)


}
#endif

static short port = 0;

struct session {
	int fd;

} sessions[1024];

int bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	static int (*bind_real)(int, const struct sockaddr *, socklen_t) = NULL;
	if(bind_real == NULL) bind_real = dlsym(RTLD_NEXT, "bind");

	struct sockaddr_in *sin = (void *)addr;
	port = ntohs(sin->sin_port);
	fprintf(stderr, "bound to port %d\n", port);

	return bind_real(fd, addr, len);
}

int accept(int fd, struct sockaddr *addr, socklen_t *len)
{
	static int (*accept_real)(int, struct sockaddr *, socklen_t *) = NULL;
	if(accept_real == NULL) accept_real = dlsym(RTLD_NEXT, "accept");

	int cl = accept_real(fd, addr, len);

}

static void *_sess_main(void *arg)
{
	int sock = (int)(long)arg;
	int cl;
	struct sockaddr_in client;
	socklen_t clen = sizeof(client);
	while((cl = accept(sock, (struct sockaddr *)&client, &clen)) != -1) {
		printf("!!!!\n");
		close(cl);
	}

	return NULL;
}

__attribute__((constructor)) static void __init_session(void)
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	int one = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(int));

	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)9001);

	bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	listen(sockfd, 4);

	pthread_t thrd;
	pthread_create(&thrd, NULL, _sess_main, (void *)(long)sockfd);

	printf(":)\n");
}

