// Shared scaffolding for the RV4A plugins.
//
// These were originally one object implementing both the processor and the
// controller (SingleComponentEffect). That is legal, the SDK validator passes
// it, and it crashed FL Studio's scanner: the host asks a component for its
// controller's class id and dereferences the answer without checking, so a
// plugin that has no separate controller takes the host down with a null read.
//
// So this is the conventional split instead - the layout every commercial
// plugin ships and the one hosts are actually tested against. The processor
// owns the audio and a plain array of parameter values; the controller owns
// their descriptions and talks to the UI. They meet only through the state
// stream and the parameter changes the host delivers each block.
#pragma once
#include <vector>
#include <cstring>
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "base/source/fstreamer.h"
#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/plugin-bindings/vst3editor.h"

namespace rv4a {

using namespace Steinberg;
using namespace Steinberg::Vst;

// Processor side. Holds parameter values itself rather than asking a
// controller, because the two may not even be in the same process.
template <int NumParams>
class ProcessorBase : public AudioEffect
{
public:
    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE
    {
        tresult r = AudioEffect::initialize(context);
        if (r != kResultOk) return r;
        addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
        return kResultOk;
    }

    tresult PLUGIN_API canProcessSampleSize(int32 s) SMTG_OVERRIDE
    { return s == kSample32 ? kResultTrue : kResultFalse; }

    tresult PLUGIN_API setState(IBStream* state) SMTG_OVERRIDE
    {
        IBStreamer s(state, kLittleEndian);
        for (int i = 0; i < NumParams; i++)
        {
            float v;
            if (!s.readFloat(v)) return kResultFalse;
            mParams[i] = v;
        }
        onParamsChanged();
        return kResultOk;
    }

    tresult PLUGIN_API getState(IBStream* state) SMTG_OVERRIDE
    {
        IBStreamer s(state, kLittleEndian);
        for (int i = 0; i < NumParams; i++) s.writeFloat(mParams[i]);
        return kResultOk;
    }

protected:
    // Reads the parameter changes for this block. Takes the final point of
    // each: parameters are applied per block here, not per sample.
    void readParamChanges(ProcessData& data)
    {
        if (!data.inputParameterChanges) return;
        int32 n = data.inputParameterChanges->getParameterCount();
        bool changed = false;
        for (int32 i = 0; i < n; i++)
            if (auto* q = data.inputParameterChanges->getParameterData(i))
            {
                ParamValue v; int32 off;
                if (q->getPoint(q->getPointCount() - 1, off, v) == kResultTrue)
                {
                    ParamID id = q->getParameterId();
                    if (id < NumParams) { mParams[id] = (float)v; changed = true; }
                }
            }
        if (changed) onParamsChanged();
    }

    virtual void onParamsChanged() {}

    float p(int id) const { return mParams[id]; }
    float mParams[NumParams] = {};
};

// Controller side: the parameter descriptions, and nothing else.
class ControllerBase : public EditControllerEx1
{
public:
    // Without this a host has no editor to open. FL Studio then hides the
    // parameters behind its own "browse parameters" list rather than drawing
    // anything, which reads as a plugin with no interface at all.
    IPlugView* PLUGIN_API createView(FIDString name) SMTG_OVERRIDE
    {
        if (name && strcmp(name, ViewType::kEditor) == 0 && mUidesc)
            return new VSTGUI::VST3Editor(this, "view", mUidesc);
        return nullptr;
    }

protected:
    const char* mUidesc = nullptr;   // set by each plugin's controller

public:
    // The host hands the processor's saved state here too, so the UI opens
    // showing what the audio engine is actually doing.
    tresult PLUGIN_API setComponentState(IBStream* state) SMTG_OVERRIDE
    {
        if (!state) return kResultFalse;
        IBStreamer s(state, kLittleEndian);
        for (int32 i = 0; i < parameters.getParameterCount(); i++)
        {
            float v;
            if (!s.readFloat(v)) break;
            setParamNormalized(i, v);
        }
        return kResultOk;
    }
};

}  // namespace rv4a
