#ifndef _THRPOOL_H
#define _THRPOOL_H

#include <pthread.h>

typedef struct thr_pool thr_pool_t;

thr_pool_t *thr_pool_create(unsigned int min_threads,
                            unsigned int max_threads,
                            unsigned int timeout,
                            const pthread_attr_t *attr);

int thr_pool_add(thr_pool_t *pool,
                 void *(*func)(void *), void *arg);

void thr_pool_wait(thr_pool_t *pool);

void thr_pool_destroy(thr_pool_t *pool);

#endif  /* _THRPOOL_H */
