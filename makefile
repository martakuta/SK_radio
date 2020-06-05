TARGET: echo-server radio-client

CC	= gcc
CFLAGS	= -Wall -O2
LFLAGS	= -Wall


echo-server.o radio-client.o err.o: err.h

echo-server: echo-server.o err.o
	$(CC) $(LFLAGS) $^ -o $@

radio-client: radio-client.o err.o
	$(CC) $(LFLAGS) $^ -o $@

.PHONY: clean TARGET
clean:
	rm -f echo-server radio-client *.o *~ *.bak
