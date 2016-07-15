CFLAGS = -std=c99 -Wall
LIBS = -pthread

test: test_thrpool test_destroy
	./test_thrpool && ./test_destroy

test_thrpool: test_thrpool.o thrpool.o
	$(CC) $(CFLAGS) $(LIBS) test_thrpool.o thrpool.o -o test_thrpool

test_destroy: test_destroy.o thrpool.o
	$(CC) $(CFLAGS) $(LIBS) test_destroy.o thrpool.o -o test_destroy

test_thrpool.o: ./test/test_thrpool.c
	$(CC) $(CFLAGS) -c ./test/test_thrpool.c

test_destroy.o: ./test/test_destroy.c
	$(CC) $(CFLAGS) -c ./test/test_destroy.c

thrpool.o: ./src/thrpool.c ./src/thrpool.h
	$(CC) $(CFLAGS) -c ./src/thrpool.c

clean:
	rm *.o$
