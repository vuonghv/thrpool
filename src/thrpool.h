#ifndef _THRPOOL_H
#define _THRPOOL_H

#include <pthread.h>

#define THR_POOL_NEW 0
#define THR_POOL_WAIT (1<<0)
#define THR_POOL_DESTROY (1<<1)

typedef struct job {
    struct job *next;
    void *(*func)(void *);
    void *arg;
} job_t;

typedef struct worker {
    struct worker *next;
    pthread_t thread;
} worker_t;

typedef struct thr_pool {
    pthread_mutex_t mutex;   /* protects the pool data */
    pthread_cond_t jobcv;  /* signal wake up idle threads to do added jobs */
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
} thr_pool_t;

thr_pool_t *thr_pool_create(unsigned int min_threads,
                            unsigned int max_threads,
                            unsigned int timeout,
                            const pthread_attr_t *attr);

int thr_pool_add(thr_pool_t *pool,
                 void *(*func)(void *), void *arg);

/*
 * Wait for all queued jobs to complete.
 */
int thr_pool_wait(thr_pool_t *pool);

void thr_pool_destroy(thr_pool_t *pool);

#endif  /* _THRPOOL_H */
