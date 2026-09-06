// RV4A Bass Exciter — VST3.
#include "Rv4aPluginBase.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "DynamicBassDsp.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace rv4a;

static const FUID kProcUID (0x52563441, 0x44594E01, 0x616C6965, 0x6E773337);
static const FUID kCtrlUID (0x52563441, 0x44594E02, 0x616C6965, 0x6E773337);

enum { kGain = 0, kX1, kX2, kY1, kY2, kSgx, kSgy, kNumParams };

class Processor : public ProcessorBase<kNumParams>
{
public:
    Processor() { setControllerClass(kCtrlUID); }
    static FUnknown* createInstance(void*) { return (IAudioProcessor*)new Processor(); }

    tresult PLUGIN_API initialize(FUnknown* c) SMTG_OVERRIDE
    {
        // Defaults must match the controller's, or the plugin sounds different
        // before the user touches anything than after they nudge one knob.
        mParams[kGain] = 0.33f;
        mParams[kX1] = (1000.f - 20.f) / 7980.f;
        mParams[kX2] = (6200.f - 20.f) / 7980.f;
        mParams[kY1] = (50.f - 20.f) / 7980.f;
        mParams[kY2] = (90.f - 20.f) / 7980.f;
        mParams[kSgx] = 0.30f; mParams[kSgy] = 0.10f;
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
        // Inherently stereo: it sums and cross-couples the channels, so it
        // needs both at once rather than a sample at a time.
        if (ch >= 2)
        {
            if (in[0] != out[0]) memcpy(out[0], in[0], sizeof(float) * data.numSamples);
            if (in[1] != out[1]) memcpy(out[1], in[1], sizeof(float) * data.numSamples);
            mDsp.processBlock(out[0], out[1], data.numSamples);
        }
        else if (ch == 1)
        {
            mScratch.assign(in[0], in[0] + data.numSamples);
            if (in[0] != out[0]) memcpy(out[0], in[0], sizeof(float) * data.numSamples);
            mDsp.processBlock(out[0], mScratch.data(), data.numSamples);
        }
        return kResultOk;
    }

protected:
    void onParamsChanged() SMTG_OVERRIDE
    {
        auto hz = [this](int id) { return 20.f + p(id) * 7980.f; };
        mDsp.setParams(p(kGain) * 100.f, hz(kX1), hz(kX2), hz(kY1), hz(kY2),
                       p(kSgx) * 100.f, p(kSgy) * 100.f);
    }

private:
    DynamicBassDsp mDsp;
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
        parameters.addParameter(STR16("Bass Gain"), STR16("%"), 0, 0.33,
                                ParameterInfo::kCanAutomate, kGain);
        parameters.addParameter(STR16("X Lower"), STR16("Hz"), 0, (1000.f - 20.f) / 7980.f,
                                ParameterInfo::kCanAutomate, kX1);
        parameters.addParameter(STR16("X Upper"), STR16("Hz"), 0, (6200.f - 20.f) / 7980.f,
                                ParameterInfo::kCanAutomate, kX2);
        parameters.addParameter(STR16("Y Lower"), STR16("Hz"), 0, (50.f - 20.f) / 7980.f,
                                ParameterInfo::kCanAutomate, kY1);
        parameters.addParameter(STR16("Y Upper"), STR16("Hz"), 0, (90.f - 20.f) / 7980.f,
                                ParameterInfo::kCanAutomate, kY2);
        parameters.addParameter(STR16("Side Gain X"), STR16("%"), 0, 0.30,
                                ParameterInfo::kCanAutomate, kSgx);
        parameters.addParameter(STR16("Side Gain Y"), STR16("%"), 0, 0.10,
                                ParameterInfo::kCanAutomate, kSgy);
        return kResultOk;
    }
};

BEGIN_FACTORY_DEF("alienware377",
                  "https://github.com/alienware377/RootlessViPER4Android",
                  "mailto:noreply@github.com")
    DEF_CLASS2(INLINE_UID_FROM_FUID(kProcUID), PClassInfo::kManyInstances,
               kVstAudioEffectClass, "RV4A Dynamic System", Vst::kDistributable,
               Vst::PlugType::kFx, "1.0.0", kVstVersionString, Processor::createInstance)
    DEF_CLASS2(INLINE_UID_FROM_FUID(kCtrlUID), PClassInfo::kManyInstances,
               kVstComponentControllerClass, "RV4A Dynamic System Controller", 0,
               "", "1.0.0", kVstVersionString, Controller::createInstance)
END_FACTORY
