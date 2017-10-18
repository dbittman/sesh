#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>

struct state {
	int ww, wr;
	int fd;
	char buffer[128];
	size_t bpos;
} cstate[120];

void close_client(struct state *cs)
{
	close(cs->fd);
	cs->ww = cs->wr = 0;
	cs->bpos = 0;
	cs->fd = -1;
}

void handle_client(int cn, struct pollfd *fd)
{
	struct state *cs = &cstate[cn];

	if((fd->revents & POLLERR) || (fd->revents & POLLHUP)) {
		close_client(cs);
		return;
	}

	if((fd->revents & POLLIN) && cs->bpos < 128) {
		ssize_t r = read(cs->fd, cs->buffer + cs->bpos, 128 - cs->bpos);
		if(r == -1) {
			if(errno == EAGAIN)
				cs->wr = 1;
			else {
				close_client(cs);
				return;
			}
		} else if(r > 0) {
			cs->bpos += r;
		} else {
			close_client(cs);
			return;
		}
	}
	
	if(cs->bpos > 0) {
		ssize_t r = write(cs->fd, cs->buffer, cs->bpos);
		if(r == -1) {
			if(errno == EAGAIN)
				cs->ww = 1;
			else {
				close_client(cs);
				return;
			}
		} else {
			memmove(cs->buffer, cs->buffer + cs->bpos, r);
			cs->bpos -= r;
		}
	}

	if(cs->bpos)
		cs->ww = 1;
	else
		cs->ww = 0;
	if(cs->bpos < 128)
		cs->wr = 1;
	else
		cs->wr = 0;
	fprintf(stderr, "client state: %d %d\n", cs->wr, cs->ww);
}

int main()
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int one = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(int));

	struct sockaddr_in serveraddr, clientaddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)1234);

	bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	listen(sockfd, 4);

	socklen_t clientlen = sizeof(clientaddr);

	struct pollfd fds[121];
	for(int i=0;i<120;i++) cstate[i].fd = -1;
	int num_clients = 0;
	while(1) {
		for(int i=0;i<120;i++) {
			fds[i].events = POLLERR | POLLHUP;
			if(cstate[i].ww) fds[i].events |= POLLOUT;
			if(cstate[i].wr) fds[i].events |= POLLIN;
			fds[i].fd = cstate[i].fd;
		}
		fds[120].fd = sockfd;
		fds[120].events = POLLIN;

		int r = poll(fds, 121, -1);
		if(r <= 0) {
			break;
		}
		for(int i=0;i<120;i++) {
			if(fds[i].revents) {
				handle_client(i, &fds[i]);
			}
		}
		if(fds[120].revents && num_clients < 120) {
			int cl = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
			int fl = fcntl(cl, F_GETFL);
			fcntl(cl, F_SETFL, fl | O_NONBLOCK);
			for(int i=0;i<120;i++) {
				if(cstate[i].fd == -1) {
					cstate[i].fd = cl;
					cstate[i].wr = 1;
					cstate[i].ww = 0;
					cstate[i].bpos = 0;
					num_clients++;
					break;
				}
			}
		}
	}

	return 0;
}

