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

    unsigned int min = 3;
    unsigned int max = 100;
    unsigned int timeout = 100;
    unsigned int sleep_time = atoi(argv[1]);
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    thr_pool_t *pool = thr_pool_create(min, max, timeout, &attr);
    for (int i = 0; i < 3000000; i++)
        thr_pool_add(pool, foo, (void *)i);

    pthread_attr_destroy(&attr);
    sleep(sleep_time);
    return 0;
}

void *foo(void *arg)
{
    int i = (int)arg;
    printf("thread %u: print i = %d\n", (unsigned int)pthread_self(), i);
    return NULL;
}
