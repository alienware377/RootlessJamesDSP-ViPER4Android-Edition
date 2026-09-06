// VST3 wrapper around the RootlessViPER4Android bass exciter.
//
// SingleComponentEffect puts the processor and controller in one class. The
// split exists so a host can run the UI apart from the audio engine, which
// matters for a plugin with a custom editor; this has no editor, so the split
// would only add boilerplate.
#include <vector>
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "base/source/fstreamer.h"

#include "DynamicBassDsp.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

static const FUID kDynamicBassUID(0x52563441, 0x44594E01, 0x616C6965, 0x6E773337);

enum ParamId : ParamID {
    kGain = 0, kX1, kX2, kY1, kY2, kSgx, kSgy,
    kNumParams
};

class DynamicBass : public SingleComponentEffect
{
public:
    static FUnknown* createInstance(void*) { return (IAudioProcessor*)new DynamicBass(); }

    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE
    {
        tresult r = SingleComponentEffect::initialize(context);
        if (r != kResultOk) return r;

        addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

        // Ranges and defaults match the Android UI, so a preset transfers.
        parameters.addParameter(STR16("Bass Gain"),  STR16("%"),  0, 0.33,
                                ParameterInfo::kCanAutomate, kGain);
        parameters.addParameter(STR16("X Lower"),    STR16("Hz"), 0, (1000.f - 20.f) / 7980.f,
                                ParameterInfo::kCanAutomate, kX1);
        parameters.addParameter(STR16("X Upper"),    STR16("Hz"), 0, (6200.f - 20.f) / 7980.f,
                                ParameterInfo::kCanAutomate, kX2);
        parameters.addParameter(STR16("Y Lower"),    STR16("Hz"), 0, (50.f - 20.f) / 7980.f,
                                ParameterInfo::kCanAutomate, kY1);
        parameters.addParameter(STR16("Y Upper"),    STR16("Hz"), 0, (90.f - 20.f) / 7980.f,
                                ParameterInfo::kCanAutomate, kY2);
        parameters.addParameter(STR16("Side Gain X"), STR16("%"), 0, 0.30,
                                ParameterInfo::kCanAutomate, kSgx);
        parameters.addParameter(STR16("Side Gain Y"), STR16("%"), 0, 0.10,
                                ParameterInfo::kCanAutomate, kSgy);
        return kResultOk;
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) SMTG_OVERRIDE
    {
        mDsp.setSampleRate((float)setup.sampleRate);
        applyParams();
        mDsp.reset();
        return SingleComponentEffect::setupProcessing(setup);
    }

    tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE
    {
        if (state) mDsp.reset();
        return SingleComponentEffect::setActive(state);
    }

    // Only 32-bit float: the engine this is ported from is float throughout,
    // and silently converting would make the plugin subtly unlike the app.
    tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) SMTG_OVERRIDE
    {
        return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
    }

    tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE
    {
        if (data.inputParameterChanges)
        {
            int32 count = data.inputParameterChanges->getParameterCount();
            for (int32 i = 0; i < count; i++)
            {
                if (auto* q = data.inputParameterChanges->getParameterData(i))
                {
                    ParamValue v; int32 sampleOffset;
                    // Take the last point in the block: parameters are applied
                    // per block here, not per sample.
                    if (q->getPoint(q->getPointCount() - 1, sampleOffset, v) == kResultTrue)
                    {
                        setParamNormalized(q->getParameterId(), v);
                        mDirty = true;
                    }
                }
            }
        }
        if (mDirty) { applyParams(); mDirty = false; }

        if (data.numSamples <= 0 || data.numInputs == 0 || data.numOutputs == 0)
            return kResultOk;

        float** in  = data.inputs[0].channelBuffers32;
        float** out = data.outputs[0].channelBuffers32;
        int32 chans = data.inputs[0].numChannels < data.outputs[0].numChannels
                          ? data.inputs[0].numChannels : data.outputs[0].numChannels;

        // This effect is inherently stereo - it sums and cross-couples the two
        // channels - so it needs both at once rather than a per-sample call.
        if (chans >= 2)
        {
            if (in[0] != out[0]) memcpy(out[0], in[0], sizeof(float) * data.numSamples);
            if (in[1] != out[1]) memcpy(out[1], in[1], sizeof(float) * data.numSamples);
            mDsp.processBlock(out[0], out[1], data.numSamples);
        }
        else if (chans == 1)
        {
            // Mono: run it as a dual-mono pair so the effect still applies
            static thread_local std::vector<float> scratch;
            scratch.assign(in[0], in[0] + data.numSamples);
            if (in[0] != out[0]) memcpy(out[0], in[0], sizeof(float) * data.numSamples);
            mDsp.processBlock(out[0], scratch.data(), data.numSamples);
        }

        // Anything beyond stereo passes through untouched rather than being
        // silenced, which is what a user dropping this on a wider bus expects.
        for (int32 c = chans; c < data.outputs[0].numChannels; c++)
            if (c < data.inputs[0].numChannels)
                memcpy(out[c], in[c], sizeof(float) * data.numSamples);

        return kResultOk;
    }

    tresult PLUGIN_API setState(IBStream* state) SMTG_OVERRIDE
    {
        IBStreamer s(state, kLittleEndian);
        for (ParamID p = 0; p < kNumParams; p++)
        {
            float v;
            if (!s.readFloat(v)) return kResultFalse;
            setParamNormalized(p, v);
        }
        applyParams();
        return kResultOk;
    }

    tresult PLUGIN_API getState(IBStream* state) SMTG_OVERRIDE
    {
        IBStreamer s(state, kLittleEndian);
        for (ParamID p = 0; p < kNumParams; p++)
            s.writeFloat((float)getParamNormalized(p));
        return kResultOk;
    }

private:
    void applyParams()
    {
        auto n  = [this](ParamID p) { return (float)getParamNormalized(p); };
        auto hz = [&](ParamID p) { return 20.0f + n(p) * 7980.0f; };
        mDsp.setParams(n(kGain) * 100.0f, hz(kX1), hz(kX2), hz(kY1), hz(kY2),
                       n(kSgx) * 100.0f, n(kSgy) * 100.0f);
    }

    DynamicBassDsp mDsp;
    bool mDirty = true;
};

BEGIN_FACTORY_DEF("alienware377",
                  "https://github.com/alienware377/RootlessViPER4Android",
                  "mailto:noreply@github.com")

    DEF_CLASS2(INLINE_UID_FROM_FUID(kDynamicBassUID),
               PClassInfo::kManyInstances,
               kVstAudioEffectClass,
               "RV4A Dynamic System",
               0 /* not distributable: one object is both processor and controller */,
               Vst::PlugType::kFx,
               "1.0.0",
               kVstVersionString,
               DynamicBass::createInstance)

END_FACTORY
