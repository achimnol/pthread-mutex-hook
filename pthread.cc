/* A simple pthread_mutex hooking library to replace mutex with spinlocks.
 * (I know that pthread provides spinlocks, but this is to replace the
 * use of mutexes in existing binaries such as NVIDIA's CUDA runtime.)
 * Spinlock implementations are taken from Intel DPDK.
 * I've referred pthread 2.19 (shipped with Ubuntu 14.04) for its internal implementation.
 * The license follows DPDK's original BSD license.
 *
 * Architecture supported: x86-64 only
 *
 * How to build:
 *
 *   $ g++ -std=c++11 -shared -fPIC -o pthread.so pthread.cc
 *
 * How to use:
 *
 *   $ LD_PRELOAD=./pthread.so your-program your-arguments */

#include <cstdio>
#include <cerrno>
#include <bits/pthreadtypes.h>

#define PTHREAD_MUTEX_TYPE(m) \
        ((m)->__data.__kind & 127)
#define PTHREAD_MUTEX_RECURSIVE_NP 1

extern "C" {
pthread_t pthread_self(void);
}

static inline void
rte_spinlock_lock(pthread_mutex_t *sl)
{
    int lock_val = 1;
    asm volatile (
            "1:\n"
            "xchg %[locked], %[lv]\n"
            "test %[lv], %[lv]\n"
            "jz 3f\n"
            "2:\n"
            "pause\n"
            "cmpl $0, %[locked]\n"
            "jnz 2b\n"
            "jmp 1b\n"
            "3:\n"
            : [locked] "=m" (sl->__data.__lock), [lv] "=q" (lock_val)
            : "[lv]" (lock_val)
            : "memory");
}

static inline void
rte_spinlock_unlock (pthread_mutex_t *sl)
{
    int unlock_val = 0;
    asm volatile (
            "xchg %[locked], %[ulv]\n"
            : [locked] "=m" (sl->__data.__lock), [ulv] "=q" (unlock_val)
            : "[ulv]" (unlock_val)
            : "memory");
}

static inline int
rte_spinlock_trylock (pthread_mutex_t *sl)
{
    int lockval = 1;

    asm volatile (
            "xchg %[locked], %[lockval]"
            : [locked] "=m" (sl->__data.__lock), [lockval] "=q" (lockval)
            : "[lockval]" (lockval)
            : "memory");

    return (lockval == 0);
}

static inline void rte_spinlock_recursive_lock(pthread_mutex_t *slr)
{
    int id = pthread_self();

    if (slr->__data.__owner != id) {
        rte_spinlock_lock(slr);
        slr->__data.__owner = id;
    }
    slr->__data.__count++;
}

static inline void rte_spinlock_recursive_unlock(pthread_mutex_t *slr)
{
    if (--(slr->__data.__count) == 0) {
        slr->__data.__owner = 0;
        rte_spinlock_unlock(slr);
    }

}

/**
 * Try to take the recursive lock.
 *
 * @param slr
 *   A pointer to the recursive spinlock.
 * @return
 *   1 if the lock is successfully taken; 0 otherwise.
 */
static inline int rte_spinlock_recursive_trylock(pthread_mutex_t *slr)
{
    int id = pthread_self();

    if (slr->__data.__owner != id) {
        if (rte_spinlock_trylock(slr) == 0)
            return 0;
        slr->__data.__owner = id;
    }
    slr->__data.__count++;
    return 1;
}

extern "C" {

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    int type = PTHREAD_MUTEX_TYPE(mutex);
    if (type == PTHREAD_MUTEX_RECURSIVE_NP) {
        /* Recursive mutex. */
        rte_spinlock_recursive_lock(mutex);
    } else {
        /* Normal mutex. */
        rte_spinlock_lock(mutex);
    }
    return 0;
}
int __pthread_mutex_lock(pthread_mutex_t *mutex) {
    int type = PTHREAD_MUTEX_TYPE(mutex);
    if (type == PTHREAD_MUTEX_RECURSIVE_NP) {
        /* Recursive mutex. */
        rte_spinlock_recursive_lock(mutex);
    } else {
        /* Normal mutex. */
        rte_spinlock_lock(mutex);
    }
    return 0;
}
int __pthread_mutex_cond_lock(pthread_mutex_t *mutex) {
    int type = PTHREAD_MUTEX_TYPE(mutex);
    if (type == PTHREAD_MUTEX_RECURSIVE_NP) {
        /* Recursive mutex. */
        rte_spinlock_recursive_lock(mutex);
    } else {
        /* Normal mutex. */
        rte_spinlock_lock(mutex);
    }
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    int type = PTHREAD_MUTEX_TYPE(mutex);
    int ret;
    if (type == PTHREAD_MUTEX_RECURSIVE_NP) {
        /* Recursive mutex. */
        ret = rte_spinlock_recursive_trylock(mutex);
        if (ret == 0)
            return EBUSY;
    } else {
        /* Normal mutex. */
        ret = rte_spinlock_trylock(mutex);
        if (ret == 0)
            return EBUSY;
    }
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    int type = PTHREAD_MUTEX_TYPE(mutex);
    if (type == PTHREAD_MUTEX_RECURSIVE_NP) {
        /* Recursive mutex. */
        rte_spinlock_recursive_unlock(mutex);
    } else {
        /* Normal mutex. */
        rte_spinlock_unlock(mutex);
    }
    return 0;
}

}

// vim: ts=8 sts=4 sw=4 et
