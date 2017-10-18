all: client server libsession_server.so libsession_client.so

clean:
	-rm server client *.so *.d

CFLAGS=-Wall -Wextra -Og -g -std=gnu11 -MD

client: client.c libsession_client.so
	$(CC) $(CFLAGS) -o $@ $< ./libsession_client.so

server: server.c libsession_server.so
	$(CC) $(CFLAGS) -o $@ $< ./libsession_server.so

libsession_client.so: session_client.c
	$(CC) -fPIC -shared $(CFLAGS) session_client.c -o libsession_client.so -ldl

libsession_server.so: session_server.c
	$(CC) -fPIC -shared $(CFLAGS) session_server.c -o libsession_server.so -ldl -lpthread

-include server.d session.d client.d

