/*
 * AIDL effect implementation around this fork's engine.
 *
 * The legacy module hands the audio server a library exporting EffectCreate;
 * the audio server then calls process() with a buffer. Under AIDL none of that
 * applies. The framework asks a binder service for an IEffect, calls open(),
 * and from then on audio travels through three fast message queues that *we*
 * create and serve from our own thread:
 *
 *   inputDataMQ   framework -> us, interleaved floats
 *   outputDataMQ  us -> framework, same layout
 *   statusMQ      us -> framework, one Status per processed chunk saying how
 *                 much was consumed and produced
 *
 * So the processing loop is ours to drive, which is the main structural
 * difference from the legacy wrapper. Everything below the interface - engine
 * init from the negotiated config, block-bounded processing, and the parameter
 * mapping onto the engine's setters - carries over from
 * app/src/main/cpp/hal/JamesDspHalEffect.cpp unchanged in spirit.
 */
#pragma once
#include <aidl/android/hardware/audio/effect/BnEffect.h>
#include <fmq/AidlMessageQueue.h>
#include <android/binder_manager.h>
#include <android-base/logging.h>

#include <atomic>
#include <thread>
#include <vector>

#include <aidl/android/hardware/audio/effect/DefaultExtension.h>
#include <hardware/audio_effect.h>

#include "EffectParams.h"   // the dispatch the legacy HAL uses, shared

extern "C" {
#include "jdsp_header.h"
void JamesDSPProcess(JamesDSPLib *jdsp, size_t n);
}

namespace aidl::android::hardware::audio::effect {

using ::android::AidlMessageQueue;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::aidl::android::media::audio::common::AudioUuid;

/* Same identity the app already looks for, so nothing changes app-side. */
static const AudioUuid kEffectUuid = {
    // timeLow is int32_t, and these UUIDs have the high bit set, so the value
    // has to be written as the signed pattern rather than narrowed implicitly.
    static_cast<int32_t>(0xf27317f4), 0xc984, 0x4de6, 0x9a90, {0x54, 0x57, 0x59, 0x49, 0x5b, 0xf2}};
static const AudioUuid kEffectType = {
    static_cast<int32_t>(0xf98765f4), 0xc321, 0x5de6, 0x9a45, {0x12, 0x34, 0x59, 0x49, 0x5a, 0xb2}};

/* The engine sizes its internals from the block given at init, so never hand
   it more than this in one call regardless of what arrives. */
static constexpr int kBlock = 4096;

class Rv4aEffect : public BnEffect {
  public:
    Rv4aEffect() {
        JamesDSPGlobalMemoryAllocation();
        JamesDSPInit(&mDsp, kBlock, 48000);
    }

    ~Rv4aEffect() override {
        stopWorker();
        JamesDSPFree(&mDsp);
    }

    ndk::ScopedAStatus open(const Parameter::Common& common,
                            const std::optional<Parameter::Specific>&,
                            OpenEffectReturn* ret) override {
        std::lock_guard lock(mMutex);
        if (mState != State::INIT) return ok();   // already open

        mSampleRate = common.input.base.sampleRate;
        mChannels = 2;
        JamesDSPInit(&mDsp, kBlock, mSampleRate);

        /* Sized from the negotiated frame count rather than a guess: too small
           and the framework stalls waiting for room, too large just wastes
           shared memory. */
        const size_t frames = common.input.frameCount > 0
                                  ? common.input.frameCount : kBlock;
        const size_t samples = frames * mChannels;

        mStatusMQ = std::make_shared<StatusMQ>(1, true /* configure event flag */);
        mInputMQ = std::make_shared<DataMQ>(samples, true);
        mOutputMQ = std::make_shared<DataMQ>(samples, true);
        if (!mStatusMQ->isValid() || !mInputMQ->isValid() || !mOutputMQ->isValid()) {
            LOG(ERROR) << "rv4a: failed to create message queues";
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        }

        ret->statusMQ = mStatusMQ->dupeDesc();
        ret->inputDataMQ = mInputMQ->dupeDesc();
        ret->outputDataMQ = mOutputMQ->dupeDesc();

        mState = State::IDLE;
        LOG(INFO) << "rv4a: open at " << mSampleRate << " Hz, " << frames << " frames";
        return ok();
    }

    ndk::ScopedAStatus close() override {
        stopWorker();
        std::lock_guard lock(mMutex);
        mStatusMQ.reset(); mInputMQ.reset(); mOutputMQ.reset();
        mState = State::INIT;
        return ok();
    }

    ndk::ScopedAStatus getDescriptor(Descriptor* desc) override {
        desc->common.id.type = kEffectType;
        desc->common.id.uuid = kEffectUuid;
        desc->common.name = "RootlessViPER4Android";
        desc->common.implementor = "alienware377";
        desc->common.flags.type = Flags::Type::INSERT;
        desc->common.flags.insert = Flags::Insert::FIRST;
        return ok();
    }

    ndk::ScopedAStatus command(CommandId id) override {
        switch (id) {
            case CommandId::START:
                startWorker();
                break;
            case CommandId::STOP:
            case CommandId::RESET:
                stopWorker();
                break;
            default:
                break;
        }
        return ok();
    }

    ndk::ScopedAStatus getState(State* state) override {
        *state = mState;
        return ok();
    }

    ndk::ScopedAStatus setParameter(const Parameter& param) override {
        /* Our own effects travel as a vendor extension rather than one of the
           standard union arms, since none of them describe what this engine
           does. The payload is the same id-and-values shape the legacy module
           already understands, so both paths stay in step. */
        applyVendorParameter(param);
        return ok();
    }

    ndk::ScopedAStatus getParameter(const Parameter::Id&, Parameter*) override {
        return ok();
    }

    ndk::ScopedAStatus reopen(OpenEffectReturn* ret) override {
        std::lock_guard lock(mMutex);
        if (!mStatusMQ) return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        ret->statusMQ = mStatusMQ->dupeDesc();
        ret->inputDataMQ = mInputMQ->dupeDesc();
        ret->outputDataMQ = mOutputMQ->dupeDesc();
        return ok();
    }

  private:
    using StatusMQ = AidlMessageQueue<IEffect::Status, SynchronizedReadWrite>;
    using DataMQ = AidlMessageQueue<float, SynchronizedReadWrite>;

    static ndk::ScopedAStatus ok() { return ndk::ScopedAStatus::ok(); }

    void startWorker() {
        if (mRunning.exchange(true)) return;
        mState = State::PROCESSING;
        mWorker = std::thread([this] { workerLoop(); });
    }

    void stopWorker() {
        if (!mRunning.exchange(false)) return;
        if (mWorker.joinable()) mWorker.join();
        if (mState == State::PROCESSING) mState = State::IDLE;
    }

    /*
     * Reads whatever the framework has queued, processes it in engine-sized
     * chunks, and reports back. Deinterleaving is needed because the queues
     * carry interleaved frames while the engine works on planar channels.
     */
    void workerLoop() {
        std::vector<float> in, left(kBlock), right(kBlock);

        while (mRunning.load()) {
            const size_t avail = mInputMQ ? mInputMQ->availableToRead() : 0;
            if (avail == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            in.resize(avail);
            if (!mInputMQ->read(in.data(), avail)) continue;

            const size_t frames = avail / mChannels;
            for (size_t off = 0; off < frames; off += kBlock) {
                const size_t n = std::min<size_t>(kBlock, frames - off);
                for (size_t i = 0; i < n; i++) {
                    left[i] = in[(off + i) * 2];
                    right[i] = in[(off + i) * 2 + 1];
                }
                mDsp.tmpBuffer[0] = left.data();
                mDsp.tmpBuffer[1] = right.data();
                JamesDSPProcess(&mDsp, n);
                for (size_t i = 0; i < n; i++) {
                    in[(off + i) * 2] = left[i];
                    in[(off + i) * 2 + 1] = right[i];
                }
            }

            const bool wrote = mOutputMQ->write(in.data(), avail);
            IEffect::Status st{wrote ? STATUS_OK : STATUS_INVALID_OPERATION,
                               static_cast<int>(avail),
                               static_cast<int>(wrote ? avail : 0)};
            mStatusMQ->write(&st, 1);
        }
    }

    /*
     * The framework does not stop speaking the old language just because the
     * HAL is new: an app calling AudioEffect.setParameter has its legacy
     * effect_param_t wrapped into a VendorExtension and delivered here. So the
     * app needs no AIDL of its own, and the payload is byte-for-byte what the
     * legacy HAL already parses - which is why the dispatch is shared rather
     * than written twice.
     */
    void applyVendorParameter(const Parameter& param) {
        if (param.getTag() != Parameter::specific) return;
        const auto& specific = param.get<Parameter::specific>();
        if (specific.getTag() != Parameter::Specific::vendorEffect) return;

        DefaultExtension payload;
        if (specific.get<Parameter::Specific::vendorEffect>()
                    .extension.getParcelable(&payload) != STATUS_OK) {
            LOG(WARNING) << "rv4a: vendor parameter carried no default extension";
            return;
        }

        /* Same layout as the legacy path: a four-byte id, then the value
           aligned to four bytes, with vsize giving its width. */
        const auto& bytes = payload.bytes;
        if (bytes.size() < sizeof(effect_param_t)) return;
        auto* p = reinterpret_cast<const effect_param_t*>(bytes.data());
        if (p->psize != sizeof(int32_t)) return;

        const int32_t id = *reinterpret_cast<const int32_t*>(p->data);
        const void* val = p->data + ((p->psize + 3) & ~3);
        const int16_t sv = (p->vsize >= sizeof(int16_t))
                               ? *reinterpret_cast<const int16_t*>(val) : 0;

        LOG(DEBUG) << "rv4a: parameter id " << id << " vsize " << p->vsize;
        applyParam(&mDsp, id, sv, sv != 0,
                   reinterpret_cast<const float*>(val), p->vsize / sizeof(float));
    }

    JamesDSPLib mDsp{};
    std::mutex mMutex;
    std::atomic<bool> mRunning{false};
    std::thread mWorker;
    State mState{State::INIT};
    int mSampleRate{48000};
    int mChannels{2};
    std::shared_ptr<StatusMQ> mStatusMQ;
    std::shared_ptr<DataMQ> mInputMQ, mOutputMQ;
};

}  // namespace aidl::android::hardware::audio::effect
