//
// Created by cx9ps3 on 09.08.2023.
//
#include "ThreadPool.h"
#include <pthread.h>
#include <malloc.h>
#include <unistd.h>

struct ThreadPoolWork
{
    ThreadFunction function;
    void *arg;
    struct ThreadPoolWork *next;
};

typedef struct ThreadPoolWork ThreadPoolWork;

struct ThreadPool
{
    ThreadPoolWork *workFirst;
    ThreadPoolWork *workLast;

    pthread_mutex_t  workMutex;
    pthread_cond_t  workCondition;
    pthread_cond_t workingCondition;

    size_t workingCount;
    size_t threadsCount;
    bool isStop;

};
static ThreadPoolWork* createWork(ThreadFunction function,void *args)
{
    ThreadPoolWork* work;
    if(function == NULL)
    {
        return NULL;
    }

    work = malloc(sizeof(*work));
    work->function = function;
    work->arg = args;
    return work;
}

static void destroyWork(ThreadPoolWork *work)
{
    if(work == NULL)
    {
        return;
    }
    free(work);
}

static ThreadPoolWork* getWork(ThreadPool *pool)
{
    ThreadPoolWork* work;

    if(pool == NULL)
    {
        return NULL;
    }
    work = pool->workFirst ;
    if(work == NULL)
    {
        return NULL;
    }
    if(work->next == NULL)
    {
        pool->workFirst =  NULL;
        pool->workLast = NULL;
    }
    else
    {
        pool->workFirst = work->next;
    }

    return work;
}

static void *poolWorker(void *arg)
{
    ThreadPool *pool = arg;
    ThreadPoolWork *work;

    while (1)
    {
        pthread_mutex_lock(&(pool->workMutex));

        while (pool->workFirst == NULL && !pool->isStop)
        {
            pthread_cond_wait(&(pool->workCondition), &(pool->workMutex));

        }

        if (pool->isStop)
        {
            break;
        }

        work = getWork(pool);
        pool->workingCount++;
        pthread_mutex_unlock(&(pool->workMutex));

        if (work != NULL)
        {
            work->function(work->arg);
            destroyWork(work);
        }

        pthread_mutex_lock(&(pool->workMutex));
        pool->workingCount--;

        if (!pool->isStop && pool->workingCount == 0 && pool->workFirst == NULL)
        {
            pthread_cond_signal(&(pool->workingCondition));
        }

        pthread_mutex_unlock(&(pool->workMutex));
    }

    pool->threadsCount--;
    pthread_cond_signal(&(pool->workingCondition));
    pthread_mutex_unlock(&(pool->workMutex));
    return NULL;
}


ThreadPool* poolCreate(size_t numberOfThread)
{
    ThreadPool  *pool;
    pthread_t  thread;
    size_t i;

    if (numberOfThread == 0)
    {
        numberOfThread = (unsigned int)sysconf(_SC_NPROCESSORS_ONLN);

    }

    pool = calloc(1, sizeof(*pool));
    pool->threadsCount = numberOfThread;

    pthread_mutex_init(&(pool->workMutex), NULL);
    pthread_cond_init(&(pool->workingCondition), NULL);
    pthread_cond_init(&(pool->workCondition), NULL);

    pool->workFirst = NULL;
    pool->workLast  = NULL;

    for (i=0; i<numberOfThread; i++)
    {
        pthread_create(&thread, NULL, poolWorker, pool);
        pthread_detach(thread);
    }

    return pool;
}
void poolDestroy(ThreadPool *pool)
{
    ThreadPoolWork *work;
    ThreadPoolWork *work2;

    if (pool == NULL)
    {
        return;

    }

    pthread_mutex_lock(&(pool->workMutex));
    work = pool->workFirst;
    while (work != NULL)
    {
        work2 = work->next;
        destroyWork(work);
        work = work2;
    }

    pool->isStop = true;
    pthread_cond_broadcast(&(pool->workCondition));
    pthread_mutex_unlock(&(pool->workMutex));

    wait(pool);

    pthread_mutex_destroy(&(pool->workMutex));
    pthread_cond_destroy(&(pool->workCondition));
    pthread_cond_destroy(&(pool->workingCondition));

    free(pool);
}

bool addWork(ThreadPool *pool,ThreadFunction function,void *arg)
{
    ThreadPoolWork *work;

    if (pool == NULL)
    {
        return false;

    }

    work = createWork(function, arg);

    if (work == NULL)
    {
        return false;
    }

    pthread_mutex_lock(&(pool->workMutex));

    if (pool->workFirst == NULL)
    {
        pool->workFirst = work;
        pool->workLast  = pool->workFirst;
    }
    else
    {
        pool->workLast->next = work;
        pool->workLast = work;
    }

    pthread_cond_broadcast(&(pool->workCondition));
    pthread_mutex_unlock(&(pool->workMutex));

    return true;
}

void wait(ThreadPool *pool)
{
    if (pool == NULL)
    {
        return;
    }

    pthread_mutex_lock(&(pool->workMutex));
    while (1)
    {
        if ((!pool->isStop && pool->workingCount != 0) || (pool->isStop && pool->threadsCount != 0))
        {
            pthread_cond_wait(&(pool->workingCondition), &(pool->workMutex));
        }
        else
        {
            break;
        }
    }
    pthread_mutex_unlock(&(pool->workMutex));
}
