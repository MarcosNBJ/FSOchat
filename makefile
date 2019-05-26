all:
	gcc chat.c -o chat -lrt -lpthread
run:
	./chat