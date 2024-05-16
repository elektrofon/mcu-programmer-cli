cli: main.c
	gcc -o cli main.c -I.

install: cli
	install -m 644 cli /usr/local/bin
