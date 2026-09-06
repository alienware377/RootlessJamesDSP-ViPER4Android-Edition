// RV4A Bass Exciter — VST3.
#include "Rv4aPluginBase.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "BassExciterDsp.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace rv4a;

static const FUID kProcUID (0x52563441, 0x42455801, 0x616C6965, 0x6E773337);
static const FUID kCtrlUID (0x52563441, 0x42455802, 0x616C6965, 0x6E773337);

enum { kCutoff = 0, kIntensity, kMix, kBand2On, kCutoff2, kIntensity2, kMix2, kNumParams };

class Processor : public ProcessorBase<kNumParams>
{
public:
    Processor() { setControllerClass(kCtrlUID); }
    static FUnknown* createInstance(void*) { return (IAudioProcessor*)new Processor(); }

    tresult PLUGIN_API initialize(FUnknown* c) SMTG_OVERRIDE
    {
        // Defaults must match the controller's, or the plugin sounds different
        // before the user touches anything than after they nudge one knob.
        mParams[kCutoff] = (100.f - 40.f) / 160.f;
        mParams[kIntensity] = 0.40f; mParams[kMix] = 0.50f;
        mParams[kBand2On] = 0.f;
        mParams[kCutoff2] = (60.f - 30.f) / 170.f;
        mParams[kIntensity2] = 0.40f; mParams[kMix2] = 0.40f;
        return ProcessorBase::initialize(c);
    }

    tresult PLUGIN_API setupProcessing(ProcessSetup& s) SMTG_OVERRIDE
    {
        mDsp.setSampleRate((float)s.sampleRate);
        onParamsChanged();
        mDsp.reset();
        return AudioEffect::setupProcessing(s);
    }

    tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE
    {
        if (state) mDsp.reset();
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
        for (int32 c = 0; c < ch; c++)
            for (int32 i = 0; i < data.numSamples; i++)
                out[c][i] = mDsp.processSample(in[c][i], c & 1);
        return kResultOk;
    }

protected:
    void onParamsChanged() SMTG_OVERRIDE
    {
        mDsp.setBand1(40.f + p(kCutoff) * 160.f, p(kIntensity) * 100.f, p(kMix) * 100.f);
        mDsp.setBand2(p(kBand2On) >= 0.5f, 30.f + p(kCutoff2) * 170.f,
                      p(kIntensity2) * 100.f, p(kMix2) * 100.f);
    }

private:
    BassExciterDsp mDsp;
};

class Controller : public ControllerBase
{
public:
    static FUnknown* createInstance(void*) { return (IEditController*)new Controller(); }

    tresult PLUGIN_API initialize(FUnknown* c) SMTG_OVERRIDE
    {
        tresult r = ControllerBase::initialize(c);
        if (r != kResultOk) return r;
        parameters.addParameter(STR16("Cutoff"), STR16("Hz"), 0, (100.f - 40.f) / 160.f,
                                ParameterInfo::kCanAutomate, kCutoff);
        parameters.addParameter(STR16("Intensity"), STR16("%"), 0, 0.40,
                                ParameterInfo::kCanAutomate, kIntensity);
        parameters.addParameter(STR16("Mix"), STR16("%"), 0, 0.50,
                                ParameterInfo::kCanAutomate, kMix);
        parameters.addParameter(STR16("Band 2"), nullptr, 1, 0.0,
                                ParameterInfo::kCanAutomate, kBand2On);
        parameters.addParameter(STR16("Cutoff 2"), STR16("Hz"), 0, (60.f - 30.f) / 170.f,
                                ParameterInfo::kCanAutomate, kCutoff2);
        parameters.addParameter(STR16("Intensity 2"), STR16("%"), 0, 0.40,
                                ParameterInfo::kCanAutomate, kIntensity2);
        parameters.addParameter(STR16("Mix 2"), STR16("%"), 0, 0.40,
                                ParameterInfo::kCanAutomate, kMix2);
        return kResultOk;
    }
};

BEGIN_FACTORY_DEF("alienware377",
                  "https://github.com/alienware377/RootlessViPER4Android",
                  "mailto:noreply@github.com")
    DEF_CLASS2(INLINE_UID_FROM_FUID(kProcUID), PClassInfo::kManyInstances,
               kVstAudioEffectClass, "RV4A Bass Exciter", Vst::kDistributable,
               Vst::PlugType::kFx, "1.0.0", kVstVersionString, Processor::createInstance)
    DEF_CLASS2(INLINE_UID_FROM_FUID(kCtrlUID), PClassInfo::kManyInstances,
               kVstComponentControllerClass, "RV4A Bass Exciter Controller", 0,
               "", "1.0.0", kVstVersionString, Controller::createInstance)
END_FACTORY
