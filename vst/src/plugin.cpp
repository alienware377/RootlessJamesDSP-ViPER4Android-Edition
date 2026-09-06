// VST3 wrapper around the RootlessViPER4Android bass exciter.
//
// SingleComponentEffect puts the processor and controller in one class. The
// split exists so a host can run the UI apart from the audio engine, which
// matters for a plugin with a custom editor; this has no editor, so the split
// would only add boilerplate.
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "base/source/fstreamer.h"

#include "BassExciterDsp.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

static const FUID kBassExciterUID(0x52563441, 0x42455801, 0x616C6965, 0x6E773337);

enum ParamId : ParamID {
    kCutoff = 0, kIntensity, kMix,
    kBand2On, kCutoff2, kIntensity2, kMix2,
    kNumParams
};

class BassExciter : public SingleComponentEffect
{
public:
    static FUnknown* createInstance(void*) { return (IAudioProcessor*)new BassExciter(); }

    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE
    {
        tresult r = SingleComponentEffect::initialize(context);
        if (r != kResultOk) return r;

        addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

        // Ranges match the Android UI so a setting means the same in both.
        parameters.addParameter(STR16("Cutoff"),    STR16("Hz"), 0, (100.f - 40.f) / 160.f,
                                ParameterInfo::kCanAutomate, kCutoff);
        parameters.addParameter(STR16("Intensity"), STR16("%"),  0, 0.40,
                                ParameterInfo::kCanAutomate, kIntensity);
        parameters.addParameter(STR16("Mix"),       STR16("%"),  0, 0.50,
                                ParameterInfo::kCanAutomate, kMix);
        parameters.addParameter(STR16("Band 2"),    nullptr,     1, 0.0,
                                ParameterInfo::kCanAutomate, kBand2On);
        parameters.addParameter(STR16("Cutoff 2"),  STR16("Hz"), 0, (60.f - 30.f) / 170.f,
                                ParameterInfo::kCanAutomate, kCutoff2);
        parameters.addParameter(STR16("Intensity 2"), STR16("%"), 0, 0.40,
                                ParameterInfo::kCanAutomate, kIntensity2);
        parameters.addParameter(STR16("Mix 2"),     STR16("%"),  0, 0.40,
                                ParameterInfo::kCanAutomate, kMix2);
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

        for (int32 c = 0; c < chans; c++)
            for (int32 i = 0; i < data.numSamples; i++)
                out[c][i] = mDsp.processSample(in[c][i], c & 1);

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
        auto n = [this](ParamID p) { return (float)getParamNormalized(p); };
        mDsp.setBand1(40.0f + n(kCutoff) * 160.0f, n(kIntensity) * 100.0f, n(kMix) * 100.0f);
        mDsp.setBand2(n(kBand2On) >= 0.5f,
                      30.0f + n(kCutoff2) * 170.0f, n(kIntensity2) * 100.0f, n(kMix2) * 100.0f);
    }

    BassExciterDsp mDsp;
    bool mDirty = true;
};

BEGIN_FACTORY_DEF("alienware377",
                  "https://github.com/alienware377/RootlessViPER4Android",
                  "mailto:noreply@github.com")

    DEF_CLASS2(INLINE_UID_FROM_FUID(kBassExciterUID),
               PClassInfo::kManyInstances,
               kVstAudioEffectClass,
               "RV4A Bass Exciter",
               Vst::kDistributable,
               Vst::PlugType::kFxInstrument,
               "1.0.0",
               kVstVersionString,
               BassExciter::createInstance)

END_FACTORY
