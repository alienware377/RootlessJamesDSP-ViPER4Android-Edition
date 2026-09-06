/*
 * The engine guards buffer teardown with a lock, because Disable frees the
 * chorus buffers while the audio thread may still be inside Process. That
 * hazard is identical in a plugin, so this is a real mutex rather than an empty
 * stub - stubbing it out would compile and then crash under exactly the race
 * the lock exists to prevent.
 */
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
static CRITICAL_SECTION gCs;
static volatile LONG gReady = 0;

void jdsp_lock(void *jdsp)
{
    (void)jdsp;
    if (InterlockedCompareExchange(&gReady, 1, 0) == 0)
        InitializeCriticalSection(&gCs);
    while (gReady != 1) { /* another thread is mid-initialise */ }
    EnterCriticalSection(&gCs);
}

void jdsp_unlock(void *jdsp)
{
    (void)jdsp;
    LeaveCriticalSection(&gCs);
}
#else
#include <pthread.h>
static pthread_mutex_t gMutex = PTHREAD_MUTEX_INITIALIZER;
void jdsp_lock(void *jdsp)   { (void)jdsp; pthread_mutex_lock(&gMutex); }
void jdsp_unlock(void *jdsp) { (void)jdsp; pthread_mutex_unlock(&gMutex); }
#endif
