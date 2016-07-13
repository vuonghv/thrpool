#include "../src/thrpool.h"
#include <assert.h>

int counter = 0;
pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;

void *counter_func(void *arg) {
    pthread_mutex_lock(&counter_lock);
    ++counter;
    pthread_mutex_unlock(&counter_lock);
}

void test_thr_pool(void);

int main(int argc, char *argv[])
{
    test_thr_pool();
    return 0;
}

void test_thr_pool(void)
{
    int min_threads = 2;
    int max_threads = 4;
    int timeout = 60;
    int num_jobs = 15;

    thr_pool_t pool;
    int err = thr_pool_create(&pool, min_threads, max_threads, timeout, NULL);

    /* test thr_pool_create */
    assert(err == 0);
    assert(pool.min == min_threads);
    assert(pool.max == max_threads);
    assert(pool.timeout == timeout);
    assert(pool.status == THR_POOL_NEW);
    assert(pool.worker == NULL);
    assert(pool.job_head == NULL);
    assert(pool.job_tail == NULL);
    assert(pool.nthreads == 0);
    assert(pool.idle == 0);

    counter = 0;
    for (int i = 0; i < num_jobs; i++) {
        thr_pool_add(&pool, counter_func, &counter);
    }

    assert(pool.nthreads <= max_threads);

    thr_pool_wait(&pool);

    /* test thr_pool_add and thr_pool_wait */
    assert(counter == num_jobs);
    assert((pool.status & THR_POOL_WAIT) == 0);
    assert(pool.job_head == NULL);
    assert(pool.job_tail == NULL);
    assert(pool.idle == pool.nthreads);
    assert(pool.worker == NULL);

    thr_pool_destroy(&pool);

    /* test thr_pool_destroy */
    assert(pool.status & THR_POOL_DESTROY);
    assert(pool.nthreads == 0);
    assert(pool.worker == NULL);
    assert(pool.job_head == NULL);
    assert(pool.job_tail == NULL);
}
