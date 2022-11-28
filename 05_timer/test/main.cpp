#include <assert.h>
#include <atomic>
#include <cstdio>

#include "timer.h"

std::atomic<int> counter(0);
pthread_mutex_t cond_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_cb3_done = PTHREAD_COND_INITIALIZER;

void callback1(void *data) {
    counter += 1;
    pthread_t *thread = (pthread_t *)data;
    printf("Thread[%lu] callback1\n", *thread);
}

void callback2(void *data) {
    counter += 1;
    pthread_t *thread = (pthread_t *)data;
    printf("Thread[%lu] callback2\n", *thread);
}

void callback3(void *data) {
    counter += 1;
    pthread_t *thread = (pthread_t *)data;
    printf("Thread[%lu] callback3\n", *thread);
    pthread_cond_signal(&cond_cb3_done);
}

void *thread_entry(void *data) {
    MxTimer timer;
    timer.AddCallback({callback1, data, 2500});
    timer.AddCallback({callback2, data, 2000});
    timer.AddCallback({callback3, data, 3000});
    pthread_mutex_lock(&cond_lock);
    pthread_cond_wait(&cond_cb3_done, &cond_lock);
    pthread_mutex_unlock(&cond_lock);
    printf("Thread[%lu] stoped\n", *(unsigned long *)(data));

    return NULL;
}

/**
 * Test timer with multiple threads.
 */
int main(int argc, char **argv) {

    pthread_t t1, t2;
    pthread_create(&t1, NULL, thread_entry, &t1);
    pthread_create(&t2, NULL, thread_entry, &t2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("counter: %d\n", counter.load());

    return 0;
}