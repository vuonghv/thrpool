CC = gcc
AR = ar
ARFLAGS = crv
CFLAGS = -std=c99 -Wall
LIBS = -pthread
INCLUDE = ./src
SRC_DIR = ./src
TEST_DIR = ./test

all: libthrpool.a

test: test_thrpool test_destroy
	./test_thrpool && ./test_destroy

install: libthrpool.a
	echo Have not implemented yet!

libthrpool.a: thrpool.o
	$(AR) $(ARFLAGS) $@ $^

test_thrpool: test_thrpool.o thrpool.o
	$(CC) $(CFLAGS) $(LIBS) $? -o $@

test_destroy: test_destroy.o thrpool.o
	$(CC) $(CFLAGS) $(LIBS) $? -o $@

test_thrpool.o: $(TEST_DIR)/test_thrpool.c
	$(CC) $(CFLAGS) -c $?

test_destroy.o: $(TEST_DIR)/test_destroy.c
	$(CC) $(CFLAGS) -c $?

thrpool.o: $(SRC_DIR)/thrpool.c $(INCLUDE)/thrpool.h
	$(CC) $(CFLAGS) -c $?

clean:
	rm *.o$
