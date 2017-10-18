all: client server libsession.so

clean:
	-rm server client libsession.so *.d

CFLAGS=-Wall -Wextra -Og -g -std=gnu11 -MD

client: client.c libsession.so
	$(CC) $(CFLAGS) -o $@ $< #./libsession.so

server: server.c libsession.so
	$(CC) $(CFLAGS) -o $@ $< ./libsession.so

libsession.so: session.c
	$(CC) -fPIC -shared $(CFLAGS) session.c -o libsession.so -ldl -lpthread

-include server.d session.d client.d

