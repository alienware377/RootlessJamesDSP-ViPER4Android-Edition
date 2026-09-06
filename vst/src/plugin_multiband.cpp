// VST3 wrapper around the RV4A multiband distortion.
//
// The effect source is compiled unmodified from the app; only the band list
// differs in how it is fed. In the app, bands come from a graphical editor and
// there may be up to 24 of them. A host's generic parameter panel cannot
// express that, so here one band is exposed with a type, frequency, Q and
// slope. That covers what the effect is usually used for - pick a range,
// distort it - and the underlying code still builds a proper filter cascade
// from it, so the selection is as steep as it is in the app.
#include <vector>
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "base/source/fstreamer.h"

extern "C" {
#include "engine/jdsp_shim.h"
}

using namespace Steinberg;
using namespace Steinberg::Vst;

static const FUID kMultibandUID(0x52563441, 0x4D424401, 0x616C6965, 0x6E773337);

enum ParamId : ParamID {
    kBandType = 0, kBandFreq, kBandQ, kBandSlope,
    kRouting, kModel,
    kDrive, kBias, kShape, kBits, kDownsample, kTone, kBandGain, kMix,
    kChRate, kChDepth, kChFeedback, kChSpread, kChVoices, kChMix,
    kNumParams
};

class Multiband : public SingleComponentEffect
{
public:
    static FUnknown* createInstance(void*) { return (IAudioProcessor*)new Multiband(); }

    Multiband() { memset(&mLib, 0, sizeof(mLib)); mLib.fs = 48000; }

    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE
    {
        tresult r = SingleComponentEffect::initialize(context);
        if (r != kResultOk) return r;
        addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

        auto p = [&](const char16* n, const char16* u, int steps, double def, ParamID id) {
            parameters.addParameter(n, u, steps, def, ParameterInfo::kCanAutomate, id);
        };
        // Band selection
        p(STR16("Band Type"), nullptr, 3, 2.0 / 3.0, kBandType);   // LP, HP, BP, Peak
        p(STR16("Band Freq"), STR16("Hz"), 0, 0.25, kBandFreq);
        p(STR16("Band Q"),    nullptr, 0, 0.2, kBandQ);
        p(STR16("Band Slope"),STR16("dB/oct"), 3, 3.0 / 3.0, kBandSlope); // 12/24/36/48
        // Distortion
        p(STR16("Routing"),   nullptr, 1, 0.0, kRouting);          // split / parallel
        p(STR16("Model"),     nullptr, 7, 0.0, kModel);            // 8 models
        p(STR16("Drive"),     STR16("%"), 0, 0.35, kDrive);
        p(STR16("Bias"),      STR16("%"), 0, 0.5,  kBias);
        p(STR16("Shape"),     STR16("%"), 0, 0.5,  kShape);
        p(STR16("Bits"),      nullptr,    0, 1.0,  kBits);
        p(STR16("Downsample"),STR16("%"), 0, 0.0,  kDownsample);
        p(STR16("Tone"),      STR16("%"), 0, 0.5,  kTone);
        p(STR16("Band Gain"), STR16("%"), 0, 0.5,  kBandGain);
        p(STR16("Mix"),       STR16("%"), 0, 1.0,  kMix);
        // Chorus on the distorted band
        p(STR16("Chorus Rate"),     STR16("Hz"), 0, 0.1, kChRate);
        p(STR16("Chorus Depth"),    STR16("ms"), 0, 0.2, kChDepth);
        p(STR16("Chorus Feedback"), STR16("%"),  0, 0.0, kChFeedback);
        p(STR16("Chorus Spread"),   STR16("%"),  0, 0.5, kChSpread);
        p(STR16("Chorus Voices"),   nullptr,     3, 0.0, kChVoices);
        p(STR16("Chorus Mix"),      STR16("%"),  0, 0.0, kChMix);
        return kResultOk;
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) SMTG_OVERRIDE
    {
        mLib.fs = (int)setup.sampleRate;
        MultibandDistEnable(&mLib);
        applyParams();
        return SingleComponentEffect::setupProcessing(setup);
    }

    tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE
    {
        // Enable allocates the chorus buffers and clears filter state; disable
        // frees them, so this must follow the host rather than run once.
        if (state) { MultibandDistEnable(&mLib); applyParams(); }
        else       { MultibandDistDisable(&mLib); }
        return SingleComponentEffect::setActive(state);
    }

    tresult PLUGIN_API canProcessSampleSize(int32 s) SMTG_OVERRIDE
    { return s == kSample32 ? kResultTrue : kResultFalse; }

    tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE
    {
        if (data.inputParameterChanges)
        {
            int32 n = data.inputParameterChanges->getParameterCount();
            for (int32 i = 0; i < n; i++)
                if (auto* q = data.inputParameterChanges->getParameterData(i))
                {
                    ParamValue v; int32 off;
                    if (q->getPoint(q->getPointCount() - 1, off, v) == kResultTrue)
                    { setParamNormalized(q->getParameterId(), v); mDirty = true; }
                }
        }
        if (mDirty) { applyParams(); mDirty = false; }

        if (data.numSamples <= 0 || data.numInputs == 0 || data.numOutputs == 0)
            return kResultOk;

        float** in  = data.inputs[0].channelBuffers32;
        float** out = data.outputs[0].channelBuffers32;
        int32 chans = data.inputs[0].numChannels < data.outputs[0].numChannels
                          ? data.inputs[0].numChannels : data.outputs[0].numChannels;
        if (chans < 1) return kResultOk;

        // The effect works in place on two planar buffers, which is exactly
        // what the host hands us - copy in, process, leave it there.
        for (int32 c = 0; c < 2 && c < chans; c++)
            if (in[c] != out[c]) memcpy(out[c], in[c], sizeof(float) * data.numSamples);

        if (chans == 1)
        {
            mMonoScratch.assign(out[0], out[0] + data.numSamples);
            mLib.tmpBuffer[0] = out[0];
            mLib.tmpBuffer[1] = mMonoScratch.data();
        }
        else
        {
            mLib.tmpBuffer[0] = out[0];
            mLib.tmpBuffer[1] = out[1];
        }
        MultibandDistProcess(&mLib, (size_t)data.numSamples);
        return kResultOk;
    }

    tresult PLUGIN_API setState(IBStream* s) SMTG_OVERRIDE
    {
        IBStreamer r(s, kLittleEndian);
        for (ParamID p = 0; p < kNumParams; p++)
        { float v; if (!r.readFloat(v)) return kResultFalse; setParamNormalized(p, v); }
        applyParams();
        return kResultOk;
    }

    tresult PLUGIN_API getState(IBStream* s) SMTG_OVERRIDE
    {
        IBStreamer w(s, kLittleEndian);
        for (ParamID p = 0; p < kNumParams; p++) w.writeFloat((float)getParamNormalized(p));
        return kResultOk;
    }

private:
    void applyParams()
    {
        auto n = [this](ParamID p) { return (float)getParamNormalized(p); };
        auto step = [&](ParamID p, int count) { int v = (int)(n(p) * count + 0.5f);
                                                return v < 0 ? 0 : (v > count ? count : v); };

        // Frequency is exponential, so the dial resolves low frequencies as
        // finely as high ones rather than crowding everything below 1 kHz into
        // the first tenth of its travel.
        float freq = 20.0f * powf(1000.0f, n(kBandFreq));
        float q    = 0.1f + n(kBandQ) * 9.9f;
        static const int kTypes[4] = { MBD_FILTER_LOW_PASS, MBD_FILTER_HIGH_PASS,
                                       MBD_FILTER_BAND_PASS, MBD_FILTER_PEAK };
        float slope = 12.0f * (step(kBandSlope, 3) + 1);   // 12, 24, 36, 48 dB/oct

        // {freq, gain-or-slope, Q, type} - the gain slot carries the slope for
        // cutoff types, which is how the engine's own editor encodes it.
        float band[4] = { freq, slope, q, (float)kTypes[step(kBandType, 3)] };
        MultibandDistSetBands(&mLib, band, 1);

        MultibandDistSetParam(&mLib,
            step(kRouting, 1), step(kModel, MBD_MODEL_COUNT - 1),
            n(kDrive) * 100.0f, n(kBias) * 200.0f - 100.0f, n(kShape) * 200.0f - 100.0f,
            1.0f + n(kBits) * 15.0f, n(kDownsample) * 100.0f,
            n(kTone) * 200.0f - 100.0f, n(kBandGain) * 200.0f - 100.0f,
            0.05f + n(kChRate) * 9.95f, 0.5f + n(kChDepth) * 19.5f,
            n(kChFeedback) * 100.0f, n(kChSpread) * 100.0f,
            1 + step(kChVoices, 3), n(kChMix) * 100.0f,
            n(kMix) * 100.0f);
    }

    JamesDSPLib mLib;
    std::vector<float> mMonoScratch;
    bool mDirty = true;
};

BEGIN_FACTORY_DEF("alienware377",
                  "https://github.com/alienware377/RootlessViPER4Android",
                  "mailto:noreply@github.com")
    DEF_CLASS2(INLINE_UID_FROM_FUID(kMultibandUID),
               PClassInfo::kManyInstances, kVstAudioEffectClass,
               "RV4A Multiband Distortion", Vst::kDistributable,
               Vst::PlugType::kFxDistortion, "1.0.0", kVstVersionString,
               Multiband::createInstance)
END_FACTORY
