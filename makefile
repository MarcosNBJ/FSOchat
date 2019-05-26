all:
	gcc chat.c -o chat -lrt -lpthread -lncurses
run:
	./chat