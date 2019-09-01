besttq: besttq.c
	mkdir bin
	cc -o bin/besttq -Werror -Wall besttq.c

clean:
	rm -r bin