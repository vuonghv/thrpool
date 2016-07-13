#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "thrpool.h"
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

static void * worker_thread(void *arg);
static int create_worker(void *arg);

/*
 * Copy all attributes from src_attr to dst_attr.
 * If src_attr is NULL, initialize dst_attr with default values.
 */
static void clone_attributes(pthread_attr_t *dst_attr,
                             const pthread_attr_t *src_attr)
{
    pthread_attr_init(dst_attr);
    if (src_attr == NULL) return;

    size_t size;
    void *addr;
    pthread_attr_getstack(src_attr, &addr, &size);
    pthread_attr_setstack(dst_attr, addr, size);

    pthread_attr_getguardsize(src_attr, &size);
    pthread_attr_setguardsize(dst_attr, size);

    struct sched_param param;
    pthread_attr_getschedparam(src_attr, &param);
    pthread_attr_setschedparam(dst_attr, &param);

    int value;
    pthread_attr_getdetachstate(src_attr, &value);
    pthread_attr_setdetachstate(dst_attr, value);

    pthread_attr_getschedpolicy(src_attr, &value);
    pthread_attr_setschedpolicy(dst_attr, value);

    pthread_attr_getinheritsched(src_attr, &value);
    pthread_attr_setinheritsched(dst_attr, value);

    pthread_attr_getscope(src_attr, &value);
    pthread_attr_setdetachstate(dst_attr, value);

    cpu_set_t cpuset;
    pthread_attr_getaffinity_np(src_attr, sizeof(cpu_set_t), &cpuset);
    pthread_attr_setaffinity_np(dst_attr, sizeof(cpu_set_t), &cpuset);
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
#ifdef THR_POOL_DEBUG
    printf("CLEANUP THREAD #%u\n", (unsigned int)pthread_self());
#endif
    pthread_mutex_unlock(&pool->mutex);
}

static void job_cleanup(void *arg)
{
    if (arg == NULL) return;

    thr_pool_t *pool = (thr_pool_t *)arg;
    pthread_t self = pthread_self();

    pthread_mutex_lock(&pool->mutex);
    ++pool->idle;

    /* TODO: Need to refactor this snippet code */
    worker_t *pre_worker = pool->worker;
    worker_t *cur_worker = pool->worker;
    while (cur_worker != NULL) {
        if (pthread_equal(cur_worker->thread, self)) {
            if (pre_worker == pool->worker)
                pool->worker = cur_worker->next;
            else
                pre_worker->next = cur_worker->next;

            break;
        }
        pre_worker = cur_worker;
        cur_worker = cur_worker->next;
    }

    /* If run out of job and all threads is idle */
#ifdef THR_POOL_DEBUG
    printf("%s: #%u  status = %02X,  idle = %d,  nthreads = %d\n",
           __func__, (unsigned int) pthread_self(),
           pool->status, pool->idle, pool->nthreads);
#endif
    if (pool->status & THR_POOL_WAIT &&
        pool->job_head == NULL &&
        pool->idle == pool->nthreads) {

        pool->status &= ~THR_POOL_WAIT;
        pthread_cond_broadcast(&pool->waitcv);
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
    worker_t self;
    self.thread = pthread_self();

    pthread_mutex_lock(&pool->mutex);
    pthread_cleanup_push(worker_cleanup, pool);
    ++pool->idle;   /* assume this thread is idle */
#ifdef THR_POOL_DEBUG
    printf("%s: #%u  idle = %d\n",
           __func__, (unsigned int)pthread_self(), pool->idle);
#endif
    while (1) {
        /*
         * we don't know what the previous job do with cancelability state.
         * So we need to reset the cancelability state
         */
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

        while (pool->job_head == NULL && !(pool->status & THR_POOL_DESTROY)) {
#ifdef THR_POOL_DEBUG
            printf("#%u waiting\n", (unsigned int)self.thread);
#endif
            pthread_cond_wait(&pool->jobcv, &pool->mutex);
#ifdef THR_POOL_DEBUG
            printf("#%u wake up\n", (unsigned int)self.thread);
#endif
        }

        if (pool->status & THR_POOL_DESTROY) break;

        /* get a job, then do it */
        job = pool->job_head;
        pool->job_head = job->next;

        if (job == pool->job_tail)
            pool->job_tail = NULL;

        --pool->idle; /* the thread had a job to do */
        self.next = pool->worker;
        pool->worker = &self;
        pthread_mutex_unlock(&pool->mutex);

        pthread_cleanup_push(job_cleanup, pool);
        job->func(job->arg);
        pthread_cleanup_pop(1);
        free(job);
        pthread_mutex_lock(&pool->mutex);
    }
    pthread_cleanup_pop(1);
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
    
    clone_attributes(&pool->attr, attr);

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
    while (pool->job_head != NULL || pool->idle != pool->nthreads) {
#ifdef THR_POOL_DEBUG
        printf("%s: idle = %d,  nthreads = %d\n",
               __func__, pool->idle, pool->nthreads);
#endif
        pthread_cond_wait(&pool->waitcv, &pool->mutex);
    }
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

void thr_pool_destroy(thr_pool_t *pool) {
    if (pool == NULL) return;
    
    pthread_mutex_lock(&pool->mutex);
    pthread_cleanup_push(pthread_mutex_unlock, &pool->mutex);
    pool->status |= THR_POOL_DESTROY;
#ifdef THR_POOL_DEBUG
    printf("%s: status = 0x%02X,  nthreads = %d\n",
            __func__, pool->status, pool->nthreads);
#endif
    
    /* Cancel all active thread */
    worker_t *cur_worker;
    while (pool->worker != NULL) {
        cur_worker = pool->worker;
        pool->worker = pool->worker->next;
        pthread_cancel(cur_worker->thread);
#ifdef THR_POOL_DEBUG
        printf("CANCELED THREAD #%u\n", (unsigned int)cur_worker->thread);
#endif
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
#ifdef THR_POOL_DEBUG
    printf("%s: broadcast pool->jobcv\n", __func__);
#endif
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
