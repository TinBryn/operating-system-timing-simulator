CC = cc
CFLAGS = -Werror -Wall -std=c99

besttq: besttq.o
	$(CC) -o bin/$@ $(CFLAGS) bin/obj/$?

.c.o:
	$(CC) -o bin/obj/$@ $(CFLAGS) $<

besttq.o: besttq.c
	mkdir -p bin/obj
	$(CC) -o bin/obj/$@ $(CFLAGS) -c $?

clean:
	rm -rf bin