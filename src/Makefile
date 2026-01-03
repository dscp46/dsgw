CFLAGS += -Wall

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $^

dsgwd: main.o dextra.o dextra_peer.o kiss.o
	$(CC) -o $@ $^
clean:
	rm -f *.o dsgwd
