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
    pthread_cond_t busycv;  /* Wait for the last thread clean up */
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

/** @brief Initialize and create a thread pool.
 *
 *  This function initializes and create a thread pool before we can use it.
 *  On success, thr_pool_create() returns 0; on error, it returns
 *  an error number, and the contents of *pool are undefined.
 *
 *  @param[out] pool        The pointer to thr_pool_t object
 *  @param[in]  min_threads The minimum number of threads kept in the pool,
 *                          always available to perform work requests.
 *  @param[in]  max_threads The maximum number of threads that can be in
 *                          the pool, performing work requests.
 *  @param[in]  timeout     The number of seconds excess idle worker threads
 *                          linger before exiting. If timeout is less than 0,
 *                          idle threads will not exit.
 *  @param[in]  attr        Attributes of all worker threads.
 *                          It can be destroyed after calling thr_pool_create().
 *                          If attr is NULL, then all worker threads is created
 *                          with the default attributes.
 *
 *  @return                 If success, return 0; otherwise return error number
 */
int thr_pool_create(thr_pool_t *pool,
                    unsigned int min_threads,
                    unsigned int max_threads,
                    unsigned int timeout,
                    const pthread_attr_t *attr);

/** @brief Add a work request to the thread pool job queue.
 *
 *  If there are idle worker threads, awaken one to perform the job.
 *  Else if the maximum number of workers has not been reached,
 *  create a new worker thread to perform the job.
 *  Else just return after adding the job to the queue;
 *  an existing worker thread will perform the job when
 *  it finishes the job it is currently performing.
 *  The job is performed as if a new thread were created for it:
 *      pthread_create(NULL, attr, void *(*func)(void *), void *arg);
 *  On success, thr_pool_add() returns 0; otherwise returns an error number.
 *
 *  @param[in] pool The pointer to thr_pool_t object
 *  @param[in] func The function that will be excuted by a worker thread.
 *  @param[in] arg  The argument is passed to func(), i.e func(arg)
 *
 *  @return         On success return 0; otherwise return an error number.
 */
int thr_pool_add(thr_pool_t *pool,
                 void *(*func)(void *), void *arg);

/** @brief Wait for all queued jobs to complete.
 *
 *  @param[in] pool The pointer to thr_pool_t object
 *  @return On success, return 0; otherwise return error number.
 */
int thr_pool_wait(thr_pool_t *pool);

/** @brief Cancel all queued jobs and destroy the pool.
 *
 *  This function should be called after calling thr_pool_wait() to release
 *  all worker threads. Calling this function when the job queue is not empty
 *  can lead to inconsistent state for program.
 *
 *  @param[in] pool The pointer to thr_pool_t object
 */
void thr_pool_destroy(thr_pool_t *pool);

#endif  /* _THRPOOL_H */
