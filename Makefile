AR = ar
ARFLAGS = crv
CFLAGS = -std=c99 -Wall -DTHR_POOL_DEBUG
LIBS = -pthread -lrt
INCLUDE = ./src
SRC_DIR = ./src
TEST_DIR = ./test
TEST_PROGRAM = test_thrpool test_destroy test_timeout

all: libthrpool.a

test: $(TEST_PROGRAM)
	./test_thrpool && ./test_destroy && ./test_timeout

install: libthrpool.a
	echo Have not implemented yet!

libthrpool.a: thrpool.o
	$(AR) $(ARFLAGS) $@ $<

test_thrpool: test_thrpool.o thrpool.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_destroy: test_destroy.o thrpool.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_timeout: test_timeout.o thrpool.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_thrpool.o: $(TEST_DIR)/test_thrpool.c
	$(CC) $(CFLAGS) -c $<

test_destroy.o: $(TEST_DIR)/test_destroy.c
	$(CC) $(CFLAGS) -c $<

test_timeout.o: $(TEST_DIR)/test_timeout.c
	$(CC) $(CFLAGS) -c $<

thrpool.o: $(SRC_DIR)/thrpool.c $(INCLUDE)/thrpool.h
	$(CC) $(CFLAGS) -c $^

clean:
	rm *.[oa] $(TEST_PROGRAM)
