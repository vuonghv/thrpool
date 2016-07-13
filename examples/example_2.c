#include "../src/thrpool.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

void *foo(void *arg);

int main(int argc, char *argv[])
{

    int min = 2;
    int max = 4;
    int timeout = 100;
    int count = 19;
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    printf("CREATE THREAD POOL:\n"
            "min_threads: %d, max_threads: %d, timeout: %d s\n",
            min, max, timeout);

    int err;
    thr_pool_t pool;
    err = thr_pool_create(&pool, min, max, timeout, &attr);
    if (err) {
        fprintf(stderr, "thr_pool_create failed!\n");
        return err;
    }

    srand(time(NULL));
    for (int i = 0; i < count; i++) {
        thr_pool_add(&pool, foo, (void *)i);
        //sleep(rand() % 2 + 1);
    }

    thr_pool_wait(&pool);
    thr_pool_destroy(&pool);
    pthread_attr_destroy(&attr);
    return 0;
}

void *foo(void *arg)
{
    int i = (int)arg;
    int time = rand() % 3 + 1;
    printf("====== #%u: print i = %d  sleep time: %d s\n", (unsigned int)pthread_self(), i, time);
    sleep(time);
    return NULL;
}
