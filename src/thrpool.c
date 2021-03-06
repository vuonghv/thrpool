#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "thrpool.h"
#include "thrpool_assert.h"
#include <stdlib.h>
#include <errno.h>

static void clone_pthread_attr(pthread_attr_t *dst,
                               const pthread_attr_t *src);
static void worker_cleanup(void *arg);
static void job_cleanup(void *arg);
static void * worker_thread(void *arg);
static int create_worker(void *arg);

/*
 * Copy all attributes from src to dst.
 * If src is NULL, initialize dst with default values.
 */
static void clone_pthread_attr(pthread_attr_t *dst,
                               const pthread_attr_t *src)
{
    pthread_attr_init(dst);
    if (src == NULL) return;

    size_t size;
    void *addr;
    pthread_attr_getstack(src, &addr, &size);
    pthread_attr_setstack(dst, addr, size);

    pthread_attr_getguardsize(src, &size);
    pthread_attr_setguardsize(dst, size);

    struct sched_param param;
    pthread_attr_getschedparam(src, &param);
    pthread_attr_setschedparam(dst, &param);

    int value;
    pthread_attr_getdetachstate(src, &value);
    pthread_attr_setdetachstate(dst, value);

    pthread_attr_getschedpolicy(src, &value);
    pthread_attr_setschedpolicy(dst, value);

    pthread_attr_getinheritsched(src, &value);
    pthread_attr_setinheritsched(dst, value);

    pthread_attr_getscope(src, &value);
    pthread_attr_setdetachstate(dst, value);

    cpu_set_t cpuset;
    pthread_attr_getaffinity_np(src, sizeof(cpu_set_t), &cpuset);
    pthread_attr_setaffinity_np(dst, sizeof(cpu_set_t), &cpuset);
}

static void worker_cleanup(void *arg)
{
    if (arg == NULL) return;
    
    thr_pool_t *pool = (thr_pool_t *)arg;
    
    --pool->nthreads;
    if (pool->nthreads < pool->min && !(pool->status & THR_POOL_DESTROY))
        create_worker(pool);

    /* If this is the last thread, wake up thr_pool_destroy */
    if ((pool->status & THR_POOL_DESTROY) && pool->nthreads == 0) {
        pthread_cond_broadcast(&pool->busycv);
    }
    DEBUG("CLEANUP THREAD #%u", (unsigned int) pthread_self());
    pthread_mutex_unlock(&pool->mutex);
}

static void job_cleanup(void *arg)
{
    if (arg == NULL) return;

    thr_pool_t *pool = (thr_pool_t *)arg;
    pthread_t self = pthread_self();

    pthread_mutex_lock(&pool->mutex);

    /* TODO: Need to refactor this snippet code */
    worker_t *prev_worker = NULL;
    worker_t *curr_worker = pool->worker;
    while (curr_worker != NULL) {
        if (pthread_equal(curr_worker->thread, self)) {
            if (curr_worker == pool->worker)
                pool->worker = curr_worker->next;
            else
                prev_worker->next = curr_worker->next;

            break;
        }
        prev_worker = curr_worker;
        curr_worker = curr_worker->next;
    }

    /* If run out of job and all threads is idle */
    if (pool->status & THR_POOL_WAIT &&
        pool->job_head == NULL &&
        pool->worker == NULL) {

        pool->status &= ~THR_POOL_WAIT;
        pthread_cond_broadcast(&pool->waitcv);
        DEBUG("#%u broadcast pool->waitcv", (unsigned int) self);
    }
    pthread_mutex_unlock(&pool->mutex);
}

/*
 * Only call this function when acquire lock
 */
static int create_worker(void *arg)
{
    if (arg == NULL) return EINVAL;
    thr_pool_t *pool = (thr_pool_t *)arg;
    pthread_t thr;
    int err = pthread_create(&thr, &pool->attr, worker_thread, pool);
    if (err) return err;

    ++pool->nthreads;
    return 0;
}

static void *worker_thread(void *arg)
{
    if (arg == NULL) return NULL;
    thr_pool_t *pool = (thr_pool_t *)arg;
    job_t *job = NULL;
    worker_t self = { .thread = pthread_self(), .next = NULL };
    int rc = 0;
    struct timespec ts;

    pthread_mutex_lock(&pool->mutex);
    pthread_cleanup_push(worker_cleanup, pool);
    while (1) {
        /*
         * we don't know what the previous job do with cancelability state.
         * So we need to reset the cancelability state
         */
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

        while (pool->job_head == NULL &&
               !(pool->status & THR_POOL_DESTROY) &&
               rc == 0) {
            ++pool->idle;
            if (pool->timeout < 0) {
                rc = pthread_cond_wait(&pool->jobcv, &pool->mutex);
            } else {
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += pool->timeout;
                rc = pthread_cond_timedwait(&pool->jobcv, &pool->mutex, &ts);
            }
            --pool->idle;
        }

        if (pool->status & THR_POOL_DESTROY) break;

        if (pool->job_head != NULL) {
            /* get a job, then do it */
            job = pool->job_head;
            pool->job_head = job->next;
            if (job == pool->job_tail)
                pool->job_tail = NULL;

            self.next = pool->worker;
            pool->worker = &self;
            pthread_mutex_unlock(&pool->mutex);

            pthread_cleanup_push(job_cleanup, pool);
            /*
             * Call the specified job function
             */
            job->func(job->arg);
            pthread_cleanup_pop(1);
            free(job);
            pthread_mutex_lock(&pool->mutex);
            rc = 0;
            continue;
        }

        if (rc == ETIMEDOUT && pool->nthreads > pool->min) break;
        rc = 0;
    }
    pthread_cleanup_pop(1);
    return NULL;
}

int thr_pool_create(thr_pool_t *pool,
                    int min_threads,
                    int max_threads,
                    int timeout,
                    const pthread_attr_t *attr)
{
    if (min_threads > max_threads || max_threads < 1 || pool == NULL) {
        return EINVAL;
    }

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->jobcv, NULL);
    pthread_cond_init(&pool->waitcv, NULL);
    pthread_cond_init(&pool->busycv, NULL);
    pool->worker = NULL;
    pool->job_head = NULL;
    pool->job_tail = NULL;
    pool->status = THR_POOL_NEW;
    pool->timeout = timeout;
    pool->min = min_threads;
    pool->max = max_threads;
    pool->nthreads = 0;
    pool->idle = 0;
    
    clone_pthread_attr(&pool->attr, attr);

    return 0;
}

int thr_pool_add(thr_pool_t *pool,
                 void *(*func)(void *), void *arg)
{
    if (!pool || !func) return EINVAL;

    job_t *job = (job_t *) malloc(sizeof(job_t));
    if (!job) return ENOMEM;

    job->func = func;
    job->arg = arg;
    job->next = NULL;

    pthread_mutex_lock(&pool->mutex);
    if (pool->job_head == NULL) {
        pool->job_head = job;
        pool->job_tail = job;
    } else {
        pool->job_tail->next = job;
        pool->job_tail = job;
    }

    if (pool->job_head != NULL && pool->idle > 0) {
        pthread_cond_broadcast(&pool->jobcv);
    } else if (pool->nthreads < pool->max) {
        create_worker(pool);
    }
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

int thr_pool_wait(thr_pool_t *pool)
{
    if (pool == NULL) return EINVAL;
    pthread_mutex_lock(&pool->mutex);
    pool->status |= THR_POOL_WAIT;
    while (pool->job_head != NULL || pool->worker != NULL) {
        DEBUG("idle = %d,  nthreads = %d", pool->idle, pool->nthreads);
        pthread_cond_wait(&pool->waitcv, &pool->mutex);
        DEBUG("WAKE UP, idle = %d,  nthreads = %d", pool->idle, pool->nthreads);
    }
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

void thr_pool_destroy(thr_pool_t *pool) {
    if (pool == NULL) return;
    
    pthread_mutex_lock(&pool->mutex);
    pthread_cleanup_push(pthread_mutex_unlock, &pool->mutex);
    pool->status |= THR_POOL_DESTROY;
    
    /* Cancel all active thread */
    worker_t *cur_worker;
    while (pool->worker != NULL) {
        cur_worker = pool->worker;
        pool->worker = pool->worker->next;
        pthread_cancel(cur_worker->thread);
        DEBUG("CANCELED THREAD #%u", (unsigned int) cur_worker->thread);
    }

    /* Destroy the job queue */
    job_t *cur_job;
    pool->job_tail = NULL;
    while (pool->job_head != NULL) {
        cur_job = pool->job_head;
        pool->job_head = pool->job_head->next;
        free(cur_job);
    }

    /* wake up all idle thread */
    DEBUG("broadcast pool->jobcv");
    pthread_cond_broadcast(&pool->jobcv);

    /* Wait for the last worker thread cleanup done */
    while (pool->nthreads > 0) {
        pthread_cond_wait(&pool->busycv, &pool->mutex);
    }
    pthread_cleanup_pop(1);

    pthread_attr_destroy(&pool->attr);
    pthread_cond_destroy(&pool->jobcv);
    pthread_cond_destroy(&pool->waitcv);
}
