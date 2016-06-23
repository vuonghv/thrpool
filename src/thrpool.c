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

#define THR_POOL_NEW 0
#define THR_POOL_WAIT (1<<0)
#define THR_POOL_DESTROY (1<<1)

typedef struct job job_t;
struct job {
    job_t *next;
    void *(*func)(void *);
    void *arg;
};

typedef struct worker worker_t;
struct worker {
    worker_t *next;
    pthread_t thread;
};

struct thr_pool {
    pthread_mutex_t mutex;   /* protects the pool data */
    pthread_cond_t wakecv;  /* signal wake up idle threads to do added jobs */
    pthread_cond_t waitcv;  /* Wait for all queued jobs to complete */
    worker_t *worker;   /* list of threads performing work */
    job_t *job_head;    /* head of FIFO job queue */
    job_t *job_tail;    /* tail of FIFO job queue */
    pthread_attr_t attr;    /* attributes of the worker threads */
    int status;
    unsigned int timeout; /* seconds before idle workers exit */
    unsigned int min;   /* minimum number of worker threads */
    unsigned int max;   /* maximum number of worker threads */
    unsigned int nthreads;  /* current number of worker threads */
    unsigned int idle;  /* number of idle workers */
};

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
    if (pool->nthreads < pool->min)
        create_worker(pool);
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
    if (pool->job_head == NULL &&
        pool->idle == pool->nthreads &&
        pool->status == THR_POOL_WAIT) {

        printf("WAKE UP BY #%u (idle: %u, nthreads: %u)",
               (unsigned int)self, pool->idle, pool->nthreads);
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
    ++pool->idle;   /* assume this thread is idle */
    pthread_cleanup_push(worker_cleanup, pool);
    while (1) {
        while (pool->job_head == NULL) {
            printf("THREAD #%u WAITING\n", (unsigned int)self.thread);
            pthread_cond_wait(&pool->wakecv, &pool->mutex);
        }

        printf("IDLE = %u\n", pool->idle);

        /* get a job, then do it */
        job = pool->job_head;
        pool->job_head = job->next;

        if (job == pool->job_tail)
            pool->job_tail = NULL;

        --pool->idle; /* the thread had a job to do */
        self.next = pool->worker;
        pool->worker = &self;
        pthread_mutex_unlock(&pool->mutex);

        printf("THREAD #%u START JOB\n", (unsigned int)self.thread);
        pthread_cleanup_push(job_cleanup, pool);
        job->func(job->arg);
        pthread_cleanup_pop(1);
        printf("THREAD #%u DONE  JOB\n", (unsigned int)self.thread);
        free(job);
        pthread_mutex_lock(&pool->mutex);
    }
    pthread_cleanup_pop(1);
}

thr_pool_t *thr_pool_create(unsigned int min_threads,
                            unsigned int max_threads,
                            unsigned int timeout,
                            const pthread_attr_t *attr)
{
    if (min_threads > max_threads || max_threads < 1) {
        errno = EINVAL;
        return NULL;
    }

    thr_pool_t *pool = (thr_pool_t *) malloc(sizeof(thr_pool_t));
    if (!pool) {
        errno = ENOMEM;
        return NULL;
    }

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->wakecv, NULL);
    pthread_cond_init(&pool->waitcv, NULL);
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

    return pool;
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
    pthread_mutex_unlock(&pool->mutex);

    pthread_mutex_lock(&pool->mutex);
    if (pool->job_head != NULL && pool->idle > 0) {
        printf("BROADCAST... IDLE = %u\n", pool->idle);
        pthread_cond_broadcast(&pool->wakecv);
    } else if (pool->nthreads < pool->max) {
        /* TODO: create a new thread to do added job */
        printf("CALLING create_worker... IDLE = %u\n", pool->idle);
        create_worker(pool);
    }
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

int thr_pool_wait(thr_pool_t *pool)
{
    if (pool == NULL) return EINVAL;
    pthread_mutex_lock(&pool->mutex);
    pool->status = THR_POOL_WAIT;
    while (pool->job_head != NULL && pool->idle != pool->nthreads) {
        pthread_cond_wait(&pool->waitcv, &pool->mutex);
    }
    printf("IDLE = %u, NTHREADS = %u\n", pool->idle, pool->nthreads);
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}
