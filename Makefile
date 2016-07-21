AR = ar
ARFLAGS = crv
CFLAGS = -std=c99 -Wall
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

%: %.o thrpool.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

test_%.o: $(TEST_DIR)/test_%.c
	$(CC) $(CFLAGS) -c $<

%.o: $(SRC_DIR)/%.c $(INCLUDE)/%.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm *.[oa] $(TEST_PROGRAM)
