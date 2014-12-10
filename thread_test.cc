#include <cstdio>
#include <cstdint>
#include <cassert>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
uint64_t var;

void *worker(void *arg)
{
    for (unsigned i = 0; i < 1000; i++) {
        assert(0 == pthread_mutex_lock(&mutex));
        var ++;
        assert(0 == pthread_mutex_unlock(&mutex));
    }
    return nullptr;
}

int main()
{
    struct timespec tv_begin, tv_end;
    pthread_t threads[4];
    clock_gettime(CLOCK_MONOTONIC_RAW, &tv_begin);
    var = 0;
    for (unsigned repeat = 0; repeat < 10; repeat ++) {
        printf("Repetition %u\n", repeat + 1);
        for (unsigned t = 0; t < 4; t++) {
            assert(0 == pthread_create(&threads[t], nullptr, worker, nullptr));
        }
        for (unsigned t = 0; t < 4; t++) {
            assert(0 == pthread_join(threads[t], nullptr));
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &tv_end);
    printf("Value: %lu\n", var);
    printf("Elapsed time: %.6f micro-seconds.\n", (tv_end.tv_sec * 1e6 + tv_end.tv_nsec / 1e3 - (tv_begin.tv_sec * 1e6 + tv_begin.tv_nsec / 1e3)));
}

// vim: ts=8 sts=4 sw=4 et
