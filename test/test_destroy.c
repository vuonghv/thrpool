#include "../src/thrpool.h"
#include "../src/thrpool_assert.h"
#include <unistd.h>

void test_thr_pool_destroy(void);
void *forever_task(void *arg);
void *quick_task(void *arg);

int main(void)
{
    test_thr_pool_destroy();
    return 0;
}

void test_thr_pool_destroy(void)
{
    const int min_threads = 2;
    const int max_threads = 4;
    const int timeout = 60;

    thr_pool_t pool;
    int err = thr_pool_create(&pool, min_threads, max_threads, timeout, NULL);

    ASSERT_EQ_INT(err, 0);

    thr_pool_add(&pool, forever_task, NULL);
    ASSERT_EQ_INT(pool.nthreads, 1);

    thr_pool_add(&pool, forever_task, NULL);
    ASSERT_EQ_INT(pool.nthreads, 2);

    thr_pool_add(&pool, quick_task, NULL);
    thr_pool_add(&pool, quick_task, NULL);

    sleep(1);

    thr_pool_destroy(&pool);

    ASSERT_NE_INT(pool.status & THR_POOL_DESTROY, 0);
    ASSERT_EQ_INT(pool.nthreads, 0);
    ASSERT_EQ_INT(pool.idle, 0);
    ASSERT_IS_NULL(pool.worker);
    ASSERT_IS_NULL(pool.job_head);
    ASSERT_IS_NULL(pool.job_tail);
}

void *forever_task(void *arg) { for (;;) {} }
void *quick_task(void *arg) { return NULL; }

