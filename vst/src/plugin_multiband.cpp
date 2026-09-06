// RV4A Multiband Distortion — VST3.
//
// The effect itself is the app's own source, compiled unmodified behind a shim.
#include "Rv4aPluginBase.h"
#include "public.sdk/source/main/pluginfactory.h"

extern "C" {
#include "engine/jdsp_shim.h"
}

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace rv4a;

static const FUID kProcUID (0x52563441, 0x4D424401, 0x616C6965, 0x6E773337);
static const FUID kCtrlUID (0x52563441, 0x4D424402, 0x616C6965, 0x6E773337);

enum {
    kBandType = 0, kBandFreq, kBandQ, kBandSlope,
    kRouting, kModel,
    kDrive, kBias, kShape, kBits, kDownsample, kTone, kBandGain, kMix,
    kChRate, kChDepth, kChFeedback, kChSpread, kChVoices, kChMix,
    kNumParams
};

class Processor : public ProcessorBase<kNumParams>
{
public:
    Processor() { setControllerClass(kCtrlUID); memset(&mLib, 0, sizeof(mLib)); mLib.fs = 48000; }
    static FUnknown* createInstance(void*) { return (IAudioProcessor*)new Processor(); }

    tresult PLUGIN_API initialize(FUnknown* c) SMTG_OVERRIDE
    {
        // Same defaults the controller declares, so what is heard before
        // touching anything matches what the knobs say.
        mParams[kBandType] = 2.f / 4.f;  mParams[kBandFreq] = 0.25f;
        mParams[kBandQ] = 0.2f;          mParams[kBandSlope] = 1.0f;
        mParams[kDrive] = 0.35f;         mParams[kBias] = 0.5f;
        mParams[kShape] = 0.5f;          mParams[kBits] = 1.0f;
        mParams[kTone] = 0.5f;           mParams[kBandGain] = 0.5f;
        mParams[kMix] = 1.0f;            mParams[kChRate] = 0.1f;
        mParams[kChDepth] = 0.2f;        mParams[kChSpread] = 0.5f;
        return ProcessorBase::initialize(c);
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& s) SMTG_OVERRIDE
    {
        mLib.fs = (int)s.sampleRate;
        MultibandDistEnable(&mLib);
        onParamsChanged();
        return AudioEffect::setupProcessing(s);
    }

    tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE
    {
        // Enable allocates the chorus buffers and Disable frees them, so this
        // has to follow the host rather than run once.
        if (state) { MultibandDistEnable(&mLib); onParamsChanged(); }
        else       { MultibandDistDisable(&mLib); }
        return AudioEffect::setActive(state);
    }

    tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE
    {
        readParamChanges(data);
        if (data.numSamples <= 0 || data.numInputs == 0 || data.numOutputs == 0)
            return kResultOk;
        float** in = data.inputs[0].channelBuffers32;
        float** out = data.outputs[0].channelBuffers32;
        int32 ch = data.inputs[0].numChannels < data.outputs[0].numChannels
                       ? data.inputs[0].numChannels : data.outputs[0].numChannels;
        if (ch < 1) return kResultOk;

        for (int32 c = 0; c < 2 && c < ch; c++)
            if (in[c] != out[c]) memcpy(out[c], in[c], sizeof(float) * data.numSamples);

        if (ch == 1)
        {
            mScratch.assign(out[0], out[0] + data.numSamples);
            mLib.tmpBuffer[0] = out[0];
            mLib.tmpBuffer[1] = mScratch.data();
        }
        else { mLib.tmpBuffer[0] = out[0]; mLib.tmpBuffer[1] = out[1]; }
        MultibandDistProcess(&mLib, (size_t)data.numSamples);
        return kResultOk;
    }

protected:
    void onParamsChanged() SMTG_OVERRIDE
    {
        auto n = [this](int id) { return p(id); };
        auto step = [&](int p, int count) { int v = (int)(n(p) * count + 0.5f);
                                                return v < 0 ? 0 : (v > count ? count : v); };

        // Frequency is exponential, so the dial resolves low frequencies as
        // finely as high ones rather than crowding everything below 1 kHz into
        // the first tenth of its travel.
        float freq = 20.0f * powf(1000.0f, n(kBandFreq));
        float q    = 0.1f + n(kBandQ) * 9.9f;
        // The engine's actual filter set. There is no bandpass type: a band is
        // selected by combining a lowpass and a highpass, or by peaking, which
        // is how the app's editor does it too.
        static const int kTypes[5] = { MBD_FILTER_LOW_PASS, MBD_FILTER_HIGH_PASS,
                                       MBD_FILTER_PEAKING, MBD_FILTER_LOW_SHELF,
                                       MBD_FILTER_HIGH_SHELF };
        float slope = 12.0f * (step(kBandSlope, 3) + 1);   // 12, 24, 36, 48 dB/oct

        // {freq, gain-or-slope, Q, type} - the gain slot carries the slope for
        // cutoff types, which is how the engine's own editor encodes it.
        float band[4] = { freq, slope, q, (float)kTypes[step(kBandType, 4)] };
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

private:
    JamesDSPLib mLib;
    std::vector<float> mScratch;
};

class Controller : public ControllerBase
{
public:
    static FUnknown* createInstance(void*) { return (IEditController*)new Controller(); }

    tresult PLUGIN_API initialize(FUnknown* c) SMTG_OVERRIDE
    {
        tresult r = ControllerBase::initialize(c);
        if (r != kResultOk) return r;
        mUidesc = "RV4AMultibandDistortion.uidesc";
        auto p = [&](const char16* n, const char16* u, int steps, double def, ParamID id) {
            parameters.addParameter(n, u, steps, def, ParameterInfo::kCanAutomate, id);
        };
        // Band selection
        p(STR16("Band Type"), nullptr, 4, 2.0 / 4.0, kBandType);   // LP, HP, peak, shelves
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
};

BEGIN_FACTORY_DEF("alienware377",
                  "https://github.com/alienware377/RootlessViPER4Android",
                  "mailto:noreply@github.com")
    DEF_CLASS2(INLINE_UID_FROM_FUID(kProcUID), PClassInfo::kManyInstances,
               kVstAudioEffectClass, "RV4A Multiband Distortion", Vst::kDistributable,
               Vst::PlugType::kFxDistortion, "1.0.0", kVstVersionString, Processor::createInstance)
    DEF_CLASS2(INLINE_UID_FROM_FUID(kCtrlUID), PClassInfo::kManyInstances,
               kVstComponentControllerClass, "RV4A Multiband Distortion Controller", 0,
               "", "1.0.0", kVstVersionString, Controller::createInstance)
END_FACTORY
