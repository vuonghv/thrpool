#include "../src/thrpool.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void *foo(void *arg);

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <sleep seconds>", argv[0]);
        return EXIT_FAILURE;
    }

    unsigned int min = 4;
    unsigned int max = 7;
    unsigned int timeout = 100;
    int count = 19;
    unsigned int sleep_time = atoi(argv[1]);
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    thr_pool_t *pool = thr_pool_create(min, max, timeout, &attr);
    srand(time(NULL));
    for (int i = 0; i < count; i++) {
        thr_pool_add(pool, foo, (void *)i);
        sleep(rand() % 2 + 1);
    }

    pthread_attr_destroy(&attr);
    //sleep(sleep_time);
    thr_pool_wait(pool);
    return 0;
}

void *foo(void *arg)
{
    int i = (int)arg;
    printf("====== #%u: print i = %d\n", (unsigned int)pthread_self(), i);
    sleep(rand() % 3 + 1);
    return NULL;
}
