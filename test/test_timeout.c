#include "../src/thrpool.h"
#include "../src/thrpool_assert.h"
#include <unistd.h>
#include <stdlib.h>

void test_positive_timeout(void);
void test_negative_timeout(void);
void *foo_task(void *arg);

int main(void)
{
    test_negative_timeout();
    test_positive_timeout();
    return 0;
}

void *foo_task(void *arg)
{
    printf("%u: running foo_task\n", (unsigned int)pthread_self());
    sleep(1);
    return arg;
}

void test_positive_timeout(void)
{
    const int min_threads = 4;
    const int max_threads = 8;
    const int timeout = 1;
    const int num_task = 10;
    int err = 0;

    thr_pool_t pool;
    err = thr_pool_create(&pool, min_threads, max_threads, timeout, NULL);
    if (err) {
        fprintf(stderr, "thr_pool_create() failed!\n");
        exit(EXIT_FAILURE);
    }

    /*
     * By setting num_task > max_threads and make foo_task sleep,
     * we want to make sure pool have to create `max_threads' worker threads.
     */
    for (int i = 0; i < num_task; i++) {
        err = thr_pool_add(&pool, foo_task, NULL);
        if (err) {
            fprintf(stderr, "thr_pool_create() failed!\n");
            exit(EXIT_FAILURE);
        }
    } 

    thr_pool_wait(&pool);

    /*
     * Now all jobs have done, we expect the number of worker threads
     * decrease to min_threads after timeout.
     */
    sleep(2); /* make sure pass timeout */
    pthread_mutex_lock(&pool.mutex);
    ASSERT_EQ_INT(pool.nthreads, min_threads);
    ASSERT_EQ_INT(pool.idle, min_threads);
    ASSERT_EQ_INT(pool.status & THR_POOL_WAIT, 0);
    pthread_mutex_unlock(&pool.mutex);

    thr_pool_destroy(&pool);
}

void test_negative_timeout(void)
{
    const int min_threads = 4;
    const int max_threads = 8;
    const int timeout = -1;
    const int num_task = 10;
    int err = 0;

    thr_pool_t pool;
    err = thr_pool_create(&pool, min_threads, max_threads, timeout, NULL);
    if (err) {
        fprintf(stderr, "thr_pool_create() failed!\n");
        exit(EXIT_FAILURE);
    }

    /*
     * By setting num_task > max_threads and make foo_task sleep,
     * we want to make sure pool have to create `max_threads' worker threads.
     */
    for (int i = 0; i < num_task; i++) {
        err = thr_pool_add(&pool, foo_task, NULL);
        if (err) {
            fprintf(stderr, "thr_pool_create() failed!\n");
            exit(EXIT_FAILURE);
        }
    } 

    thr_pool_wait(&pool);

    /*
     * Because we pass timeout is a negative number,
     * we expect all idel threads are still alive.
     */
    sleep(2); /* make sure pass timeout */
    pthread_mutex_lock(&pool.mutex);
    ASSERT_EQ_INT(pool.nthreads, max_threads);
    ASSERT_EQ_INT(pool.idle, max_threads);
    ASSERT_EQ_INT(pool.status & THR_POOL_WAIT, 0);
    pthread_mutex_unlock(&pool.mutex);

    thr_pool_destroy(&pool);
}
