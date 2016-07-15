#include "../src/thrpool.h"
#include "../src/thrpool_assert.h"

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
    pthread_mutex_destroy(&counter_lock);
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
    pthread_mutex_lock(&pool.mutex);
    ASSERT_EQ_INT(err, 0);
    ASSERT_EQ_INT(pool.min, min_threads);
    ASSERT_EQ_INT(pool.max, max_threads);
    ASSERT_EQ_INT(pool.timeout, timeout);
    ASSERT_EQ_INT(pool.status, THR_POOL_NEW);
    ASSERT_EQ_INT(pool.nthreads, 0);
    ASSERT_EQ_INT(pool.idle, 0);
    ASSERT_IS_NULL(pool.worker);
    ASSERT_IS_NULL(pool.job_head);
    ASSERT_IS_NULL(pool.job_tail);
    pthread_mutex_unlock(&pool.mutex);

    counter = 0;
    for (int i = 0; i < num_jobs; i++) {
        thr_pool_add(&pool, counter_func, &counter);
    }

    pthread_mutex_lock(&pool.mutex);
    ASSERT_LE_INT(pool.nthreads, max_threads);
    pthread_mutex_unlock(&pool.mutex);

    thr_pool_wait(&pool);

    /* test thr_pool_add and thr_pool_wait */
    pthread_mutex_lock(&pool.mutex);
    ASSERT_EQ_INT(counter, num_jobs);
    ASSERT_EQ_INT(pool.status & THR_POOL_WAIT, 0);
    ASSERT_IS_NULL(pool.job_head);
    ASSERT_IS_NULL(pool.job_tail);
    ASSERT_IS_NULL(pool.worker);
    pthread_mutex_unlock(&pool.mutex);

    thr_pool_destroy(&pool);

    /* test thr_pool_destroy */
    pthread_mutex_lock(&pool.mutex);
    ASSERT_NE_INT(pool.status & THR_POOL_DESTROY, 0);
    ASSERT_EQ_INT(pool.nthreads, 0);
    ASSERT_IS_NULL(pool.worker);
    ASSERT_IS_NULL(pool.job_head);
    ASSERT_IS_NULL(pool.job_tail);
    pthread_mutex_unlock(&pool.mutex);
}
