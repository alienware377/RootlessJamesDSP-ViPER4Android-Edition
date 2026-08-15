/*
 * Link stub for the platform half of libbinder_ndk.
 *
 * The NDK ships libbinder_ndk for apps, which deliberately omits the service
 * registration entry points - an app has no business publishing a system
 * service. A HAL does exactly that, so those symbols are missing at link time
 * even though they are present in libbinder_ndk.so on device.
 *
 * This builds a stub carrying the same soname. The linker is satisfied by it,
 * the executable records a dependency on libbinder_ndk.so, and at runtime the
 * real platform library is what actually loads. Nothing here ever executes;
 * the bodies exist only to give the symbols an address.
 */
#include <stdint.h>

typedef struct AIBinder AIBinder;
typedef int32_t binder_status_t;

binder_status_t AServiceManager_addService(AIBinder* binder, const char* instance) {
    (void)binder; (void)instance; return -1;
}
AIBinder* AServiceManager_checkService(const char* instance) {
    (void)instance; return 0;
}
AIBinder* AServiceManager_waitForService(const char* instance) {
    (void)instance; return 0;
}
void ABinderProcess_setThreadPoolMaxThreadCount(uint32_t numThreads) { (void)numThreads; }
void ABinderProcess_joinThreadPool(void) {}
void ABinderProcess_startThreadPool(void) {}
