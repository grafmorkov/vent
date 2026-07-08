#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stddef.h>

typedef int (*JobFunc)(void* data);

typedef struct ThreadPool ThreadPool;

ThreadPool* tp_create(int num_threads);
void tp_destroy(ThreadPool* tp);
void tp_enqueue(ThreadPool* tp, JobFunc func, void* data);
int  tp_wait(ThreadPool* tp);

void ui_lock(void);
void ui_unlock(void);

#endif
