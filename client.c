#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

int main(int argc, char **argv)
{
	(void)argv;
	int reconn = 0;
	if(argc > 1) {
		reconn = 1;
	}

	int sockfd;
	if(reconn && 0) {
		int ss = socket(AF_INET, SOCK_STREAM, 0);
		struct hostent *server = gethostbyname("localhost");
		struct sockaddr_in serveraddr;
		memset(&serveraddr, 0, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		memcpy((char *)&serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);
		serveraddr.sin_port = htons(9001);
		if(connect(ss, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
			perror("reconnect");
			exit(1);
		}
		sockfd = ss;
	} else {

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		struct hostent *server = gethostbyname("localhost");
		struct sockaddr_in serveraddr;
		memset(&serveraddr, 0, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		memcpy((char *)&serveraddr.sin_addr.s_addr, server->h_addr, server->h_length);
		serveraddr.sin_port = htons(1234);

		if(connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1) {
			perror("connect");
			exit(1);
		}
	}
	while(1) {
		char *buf = NULL;
		size_t bl = 0;
		ssize_t r = getline(&buf, &bl, stdin);
		if(r <= 0) break;
		write(sockfd, buf, r);
		char rb[bl];
		memset(rb, 0, bl);
		read(sockfd, rb, bl);
		printf(":: %s\n", rb);
	}
	close(sockfd);
	return 0;
}

