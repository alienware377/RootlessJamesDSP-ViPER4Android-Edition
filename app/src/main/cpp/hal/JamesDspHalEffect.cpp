/*
 * System-wide audio effect wrapper around this fork's DSP engine.
 *
 * Root mode drives a system effect through Android's AudioEffect API rather
 * than the engine bundled in the APK, which is why the fork's own effects do
 * nothing there when the stock JamesDSP module is installed. This library
 * exposes the same descriptor the app looks for, so a Magisk module shipping
 * it makes root mode run *this* engine - extra reverb rooms, the ViPER classic
 * limiter, the rebuilt delay and the retuned speaker optimisation included.
 *
 * The descriptor UUID matches EFFECT_JAMESDSP in JamesDspRemoteEngine, and the
 * type UUID matches EFFECT_TYPE_CUSTOM, so no app-side change is needed.
 */
#include <string.h>
#include <stdlib.h>
#include <hardware/audio_effect.h>

extern "C" {
#include "jdsp_header.h"
}

static effect_descriptor_t rv4a_descriptor =
{
    { 0xf98765f4, 0xc321, 0x5de6, 0x9a45, { 0x12, 0x34, 0x59, 0x49, 0x5a, 0xb2 } },
    { 0xf27317f4, 0xc984, 0x4de6, 0x9a90, { 0x54, 0x57, 0x59, 0x49, 0x5b, 0xf2 } },
    EFFECT_CONTROL_API_VERSION,
    EFFECT_FLAG_TYPE_INSERT | EFFECT_FLAG_INSERT_FIRST,
    10,
    1,
    "RootlessViPER4Android engine",
    "alienware377"
};

struct rv4a_context
{
    const struct effect_interface_s *itfe;
    JamesDSPLib dsp;
    bool active;
    uint32_t frameCount;
};

static int32_t rv4a_process(effect_handle_t self, audio_buffer_t *in, audio_buffer_t *out)
{
    rv4a_context *c = reinterpret_cast<rv4a_context *>(self);
    if (!c || !in || !out || !in->raw || !out->raw)
        return -EINVAL;
    if (!c->active)
        return -ENODATA;

    /* The HAL hands us interleaved stereo floats; the engine works on planar
       buffers, so deinterleave, process, and interleave back. */
    const size_t frames = in->frameCount;
    if (frames == 0)
        return 0;

    static thread_local float *left = nullptr, *right = nullptr;
    static thread_local size_t capacity = 0;
    if (frames > capacity)
    {
        free(left); free(right);
        left = (float *)malloc(frames * sizeof(float));
        right = (float *)malloc(frames * sizeof(float));
        capacity = left && right ? frames : 0;
        if (!capacity) return -ENOMEM;
    }

    for (size_t i = 0; i < frames; i++)
    {
        left[i] = in->f32[i * 2];
        right[i] = in->f32[i * 2 + 1];
    }

    c->dsp.tmpBuffer[0] = left;
    c->dsp.tmpBuffer[1] = right;
    JamesDSPProcess(&c->dsp, frames);

    for (size_t i = 0; i < frames; i++)
    {
        out->f32[i * 2] = left[i];
        out->f32[i * 2 + 1] = right[i];
    }
    return 0;
}

static int32_t rv4a_command(effect_handle_t self, uint32_t cmdCode, uint32_t cmdSize,
                            void *pCmdData, uint32_t *replySize, void *pReplyData)
{
    rv4a_context *c = reinterpret_cast<rv4a_context *>(self);
    if (!c)
        return -EINVAL;

    switch (cmdCode)
    {
    case EFFECT_CMD_INIT:
        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;

    case EFFECT_CMD_SET_CONFIG:
    {
        if (!pCmdData || cmdSize != sizeof(effect_config_t))
            return -EINVAL;
        effect_config_t *cfg = (effect_config_t *)pCmdData;
        JamesDSPInit(&c->dsp, 128, cfg->inputCfg.samplingRate);
        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;
    }

    case EFFECT_CMD_ENABLE:
        c->active = true;
        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;

    case EFFECT_CMD_DISABLE:
        c->active = false;
        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;

    case EFFECT_CMD_SET_PARAM:
        /* Parameter dispatch shares the id space used by the JNI wrapper.
           Wired up per effect in the follow-up work; acknowledged here so the
           audio server doesn't treat unknown ids as a failure and drop us. */
        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;

    default:
        return -EINVAL;
    }
}

static int32_t rv4a_getDescriptor(effect_handle_t self, effect_descriptor_t *pDescriptor)
{
    if (!pDescriptor)
        return -EINVAL;
    memcpy(pDescriptor, &rv4a_descriptor, sizeof(effect_descriptor_t));
    return 0;
}

static const struct effect_interface_s rv4a_interface =
{
    rv4a_process,
    rv4a_command,
    rv4a_getDescriptor,
    NULL
};

extern "C" {

int32_t EffectCreate(const effect_uuid_t *uuid, int32_t sessionId, int32_t ioId,
                     effect_handle_t *pHandle)
{
    (void)sessionId; (void)ioId;
    if (!pHandle || !uuid)
        return -EINVAL;
    if (memcmp(uuid, &rv4a_descriptor.uuid, sizeof(effect_uuid_t)) != 0)
        return -EINVAL;

    rv4a_context *c = (rv4a_context *)calloc(1, sizeof(rv4a_context));
    if (!c)
        return -ENOMEM;
    c->itfe = &rv4a_interface;
    c->active = false;
    JamesDSPGlobalMemoryAllocation();
    JamesDSPInit(&c->dsp, 128, 48000);
    *pHandle = (effect_handle_t)c;
    return 0;
}

int32_t EffectRelease(effect_handle_t handle)
{
    rv4a_context *c = reinterpret_cast<rv4a_context *>(handle);
    if (!c)
        return -EINVAL;
    JamesDSPFree(&c->dsp);
    free(c);
    return 0;
}

int32_t EffectGetDescriptor(const effect_uuid_t *uuid, effect_descriptor_t *pDescriptor)
{
    if (!pDescriptor || !uuid)
        return -EINVAL;
    memcpy(pDescriptor, &rv4a_descriptor, sizeof(effect_descriptor_t));
    return 0;
}

audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM =
{
    .tag = AUDIO_EFFECT_LIBRARY_TAG,
    .version = EFFECT_LIBRARY_API_VERSION,
    .name = "RootlessViPER4Android Effect Library",
    .implementor = "alienware377",
    .create_effect = EffectCreate,
    .release_effect = EffectRelease,
    .get_descriptor = EffectGetDescriptor,
};

}
