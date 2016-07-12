#include "../src/thrpool.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define NUM_TASK 5

void *task(void *arg);

int main(void)
{
    int min_threads = 2;
    int max_threads = 4;
    int timeout = 60;    
    int err = 0;

    /* Create a thread pool with default attributes, NULL */
    printf("Starting create a new thread pool:\n");
    printf("min_threads: %u,  max_threads: %u,  timeout: %u s\n",
          min_threads, max_threads, timeout);
    thr_pool_t pool;
    err = thr_pool_create(&pool, min_threads, max_threads, timeout, NULL);
    if (err) {
        fprintf(stderr, "thr_pool_create() failed!\n");
        exit(EXIT_FAILURE);
    }

    /* Add job to thread pool */
    printf("Starting add %d tasks to the thread pool\n", NUM_TASK);
    for (int i = 0; i < NUM_TASK; ++i) {
        thr_pool_add(&pool, task, (void *)i);
    }
    printf("Finish adding tasks.\n");

    /* wait for all job done */
    thr_pool_wait(&pool);
    printf("All tasks have done.\n");

    /* destroy pool after using; destroy all pool's living threads */
    printf("Destroy the thread pool.\n");
    thr_pool_destroy(&pool);

    return 0;
}

void *task(void *arg)
{
    int task_id = (int)arg;
    printf("START TASK %d BY THREAD #%u\n", task_id, (unsigned int)pthread_self());
    sleep(1);
    printf("FINISH TASK %d.\n", task_id);
    return NULL;
}
