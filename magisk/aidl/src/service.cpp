/*
 * Entry point for the effect HAL service.
 *
 * Registers as android.hardware.audio.effect.IFactory/default, the name the
 * framework looks up. The stock implementation keeps running under
 * /vendor_original, started first by the module's init script, and everything
 * that is not ours is handed to it.
 *
 * A lazy service would be tidier, but the framework expects the effect factory
 * to be present when it enumerates at boot, so this stays resident.
 */
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android-base/logging.h>

#include "FactoryImpl.cpp"

using aidl::android::hardware::audio::effect::Rv4aFactory;

int main() {
    /* One extra thread beyond the caller: effect creation and queries are
       infrequent, and the audio itself never comes through binder - it goes
       through the message queues instead. */
    ABinderProcess_setThreadPoolMaxThreadCount(2);

    auto factory = ndk::SharedRefBase::make<Rv4aFactory>();
    const std::string name =
        std::string(Rv4aFactory::descriptor) + "/default";

    binder_status_t status =
        AServiceManager_addService(factory->asBinder().get(), name.c_str());
    if (status != STATUS_OK) {
        /* Refuse to linger as a half-registered service: exiting lets init
           restart us, and lets the boot watchdog see a clean failure. */
        LOG(FATAL) << "rv4a: could not register " << name << ", status " << status;
        return 1;
    }

    LOG(INFO) << "rv4a: effect factory registered as " << name;
    ABinderProcess_joinThreadPool();
    return 0;   // not reached
}
