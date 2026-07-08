#include "threadpool.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct Job {
    JobFunc func;
    void* data;
    struct Job* next;
} Job;

struct ThreadPool {
    pthread_t* threads;
    int num_threads;

    Job* queue_head;
    Job* queue_tail;
    size_t pending;

    pthread_mutex_t queue_mutex;
    pthread_cond_t  queue_cond;

    pthread_mutex_t ui_mutex;

    int stop;
    int any_failed;
};

static ThreadPool* g_pool = NULL;

void ui_lock(void) {
    if (g_pool)
        pthread_mutex_lock(&g_pool->ui_mutex);
}

void ui_unlock(void) {
    if (g_pool)
        pthread_mutex_unlock(&g_pool->ui_mutex);
}

static Job* job_dequeue(ThreadPool* tp) {
    if (!tp->queue_head)
        return NULL;
    Job* j = tp->queue_head;
    tp->queue_head = j->next;
    if (!tp->queue_head)
        tp->queue_tail = NULL;
    return j;
}

static void* worker_thread(void* arg) {
    ThreadPool* tp = (ThreadPool*)arg;

    while (1) {
        pthread_mutex_lock(&tp->queue_mutex);

        while (!tp->stop && !tp->queue_head)
            pthread_cond_wait(&tp->queue_cond, &tp->queue_mutex);

        if (tp->stop && !tp->queue_head) {
            pthread_mutex_unlock(&tp->queue_mutex);
            break;
        }

        Job* job = job_dequeue(tp);
        tp->pending--;

        pthread_mutex_unlock(&tp->queue_mutex);

        if (job) {
            int rc = job->func(job->data);
            if (rc != 0) {
                pthread_mutex_lock(&tp->queue_mutex);
                tp->any_failed = 1;
                pthread_mutex_unlock(&tp->queue_mutex);
            }
            free(job);
        }
    }

    return NULL;
}

ThreadPool* tp_create(int num_threads) {
    ThreadPool* tp = calloc(1, sizeof(ThreadPool));
    if (!tp) return NULL;

    tp->num_threads = num_threads > 0 ? num_threads : 1;

    pthread_mutex_init(&tp->queue_mutex, NULL);
    pthread_cond_init(&tp->queue_cond, NULL);
    pthread_mutex_init(&tp->ui_mutex, NULL);

    tp->threads = malloc(sizeof(pthread_t) * tp->num_threads);
    if (!tp->threads) {
        free(tp);
        return NULL;
    }

    g_pool = tp;

    for (int i = 0; i < tp->num_threads; i++)
        pthread_create(&tp->threads[i], NULL, worker_thread, tp);

    return tp;
}

void tp_enqueue(ThreadPool* tp, JobFunc func, void* data) {
    Job* job = malloc(sizeof(Job));
    if (!job) return;

    job->func = func;
    job->data = data;
    job->next = NULL;

    pthread_mutex_lock(&tp->queue_mutex);

    if (tp->queue_tail)
        tp->queue_tail->next = job;
    else
        tp->queue_head = job;
    tp->queue_tail = job;
    tp->pending++;

    pthread_cond_signal(&tp->queue_cond);
    pthread_mutex_unlock(&tp->queue_mutex);
}

int tp_wait(ThreadPool* tp) {
    pthread_mutex_lock(&tp->queue_mutex);
    while (tp->pending > 0) {
        pthread_cond_wait(&tp->queue_cond, &tp->queue_mutex);
    }

    int failed = tp->any_failed;
    tp->stop = 1;
    pthread_cond_broadcast(&tp->queue_cond);
    pthread_mutex_unlock(&tp->queue_mutex);

    for (int i = 0; i < tp->num_threads; i++)
        pthread_join(tp->threads[i], NULL);

    g_pool = NULL;

    return failed ? -1 : 0;
}

void tp_destroy(ThreadPool* tp) {
    if (!tp) return;

    pthread_mutex_destroy(&tp->queue_mutex);
    pthread_cond_destroy(&tp->queue_cond);
    pthread_mutex_destroy(&tp->ui_mutex);
    free(tp->threads);

    Job* j = tp->queue_head;
    while (j) {
        Job* next = j->next;
        free(j);
        j = next;
    }

    free(tp);
}
