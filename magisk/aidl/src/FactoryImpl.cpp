/*
 * A proxying effect factory.
 *
 * On devices that ship audio_effects_config.xml, an effect declares itself in
 * that file and the stock service loads it. Some devices - Pixels among them -
 * ship no such file, so there is nothing to declare ourselves in and no way to
 * be loaded. The only remaining way in is to *be* the factory the framework
 * talks to, and keep the vendor's implementation behind us.
 *
 * The rule this follows: never make the device worse than it was. Everything
 * that is not ours is delegated verbatim, so all stock effects keep working. If
 * the vendor handle cannot be obtained at all, we still answer with our own
 * effect rather than failing the call, because returning an error from
 * queryEffects would leave the framework believing the device has no effect
 * HAL whatsoever.
 *
 * Instance naming: the module's init script starts the vendor binary under
 * `android.hardware.audio.effect.IFactory/vendor_original` and starts this
 * service as `/default`. That keeps both alive and makes the delegation target
 * explicit, rather than trying to grab a handle to a service we are in the
 * middle of replacing.
 */
#include <aidl/android/hardware/audio/effect/BnFactory.h>
#include <android/binder_manager.h>
#include <android-base/logging.h>

#include "EffectImpl.h"

namespace aidl::android::hardware::audio::effect {

using ::aidl::android::media::audio::common::AudioUuid;

static const AudioUuid kOurUuid = {
    static_cast<int32_t>(0xf27317f4), 0xc984, 0x4de6, 0x9a90, {0x54, 0x57, 0x59, 0x49, 0x5b, 0xf2}};

/* Where the module's init script leaves the stock implementation. */
static constexpr const char* kVendorInstance =
    "android.hardware.audio.effect.IFactory/vendor_original";

class Rv4aFactory : public BnFactory {
  public:
    ndk::ScopedAStatus queryEffects(const std::optional<AudioUuid>& type,
                                    const std::optional<AudioUuid>& implementation,
                                    const std::optional<AudioUuid>& proxy,
                                    std::vector<Descriptor>* out) override {
        out->clear();

        /* Vendor first, so stock effects keep their usual ordering. A failure
           here is logged and tolerated rather than propagated: a device with
           our effect alone is poor, but a device the framework thinks has no
           effect HAL is broken. */
        if (auto vendor = vendorFactory()) {
            std::vector<Descriptor> theirs;
            auto status = vendor->queryEffects(type, implementation, proxy, &theirs);
            if (status.isOk()) {
                out->insert(out->end(), theirs.begin(), theirs.end());
            } else {
                LOG(WARNING) << "rv4a: vendor queryEffects failed, continuing with ours only";
            }
        }

        /* Only advertise ourselves when the query could match us; the framework
           filters by type and implementation and expects us to honour that. */
        if ((!implementation || *implementation == kOurUuid) && !proxy) {
            out->push_back(ourDescriptor());
        }
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus queryProcessing(const std::optional<Processing::Type>& type,
                                       std::vector<Processing>* out) override {
        /* Passed straight through. This describes which streams and sources
           effects attach to, which is the vendor's business entirely - we add
           an effect, we do not change the routing rules. */
        if (auto vendor = vendorFactory()) {
            return vendor->queryProcessing(type, out);
        }
        out->clear();
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus createEffect(const AudioUuid& uuid,
                                    std::shared_ptr<IEffect>* out) override {
        if (uuid == kOurUuid) {
            *out = ndk::SharedRefBase::make<Rv4aEffect>();
            LOG(INFO) << "rv4a: created our effect";
            return ndk::ScopedAStatus::ok();
        }
        if (auto vendor = vendorFactory()) {
            return vendor->createEffect(uuid, out);
        }
        LOG(ERROR) << "rv4a: no vendor factory to create a non-ours effect";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    ndk::ScopedAStatus destroyEffect(const std::shared_ptr<IEffect>& handle) override {
        /* Ours self-destruct when the last reference goes; anything else has to
           go back to whoever made it. Telling the two apart by descriptor is
           more reliable than tracking handles we did not create. */
        Descriptor desc;
        if (handle && handle->getDescriptor(&desc).isOk() &&
            desc.common.id.uuid == kOurUuid) {
            return ndk::ScopedAStatus::ok();
        }
        if (auto vendor = vendorFactory()) {
            return vendor->destroyEffect(handle);
        }
        return ndk::ScopedAStatus::ok();
    }

  private:
    Descriptor ourDescriptor() const {
        Descriptor d;
        d.common.id.type = {static_cast<int32_t>(0xf98765f4), 0xc321, 0x5de6, 0x9a45,
                            {0x12, 0x34, 0x59, 0x49, 0x5a, 0xb2}};
        d.common.id.uuid = kOurUuid;
        d.common.name = "RootlessViPER4Android";
        d.common.implementor = "alienware377";
        d.common.flags.type = Flags::Type::INSERT;
        d.common.flags.insert = Flags::Insert::FIRST;
        return d;
    }

    /* Resolved once and cached. Deliberately not waitForService: if the stock
       service is absent or slow, we answer with what we have rather than
       blocking a framework call for however long it takes. */
    std::shared_ptr<IFactory> vendorFactory() {
        std::call_once(mOnce, [this] {
            auto binder = ndk::SpAIBinder(AServiceManager_checkService(kVendorInstance));
            mVendor = IFactory::fromBinder(binder);
            LOG(INFO) << "rv4a: vendor factory "
                      << (mVendor ? "resolved" : "not found");
        });
        return mVendor;
    }

    std::once_flag mOnce;
    std::shared_ptr<IFactory> mVendor;
};

}  // namespace aidl::android::hardware::audio::effect
