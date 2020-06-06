TARGET: echo-server radio-client radio-proxy

CC	= gcc
CFLAGS	= -Wall -O2
LFLAGS	= -Wall


echo-server.o radio-client.o radio-proxy.o err.o: err.h

echo-server: echo-server.o err.o
	$(CC) $(LFLAGS) $^ -o $@

radio-client: radio-client.o err.o
	$(CC) $(LFLAGS) $^ -o $@

radio-proxy: radio-proxy.o err.o
	$(CC) $(LFLAGS) $^ -o $@

.PHONY: clean TARGET
clean:
	rm radio-client.o
	rm radio-proxy.o 
	rm echo-server.o 
	rm err.o 
	rm echo-server
	rm radio-client
	rm radio-proxy
