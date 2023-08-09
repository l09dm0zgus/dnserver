//
// Created by cx9ps3 on 09.08.2023.
//
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stdbool.h>
#include <stddef.h>

struct ThreadPool;

typedef struct ThreadPool ThreadPool;

typedef void (*ThreadFunction)(void *arg);
ThreadPool* poolCreate(size_t numberOfThread);

void poolDestroy(ThreadPool *pool);

bool addWork(ThreadPool *pool,ThreadFunction function,void *arg);

void wait(ThreadPool *pool);


#endif //THREAD_POOL_H
