all:
	$(CC) cliente_psta.c -o client
	$(CC) -pthread servidor_psta.c -o server
clean:
	rm client
	rm server
