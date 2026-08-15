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

#include "EffectParams.h"

#include <android/log.h>

#define LOG_TAG "RV4A-HAL"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

/* Buffers the audio server hands us vary by device and route; the engine sizes
   its internals from the block size given at init, so process in chunks no
   larger than this rather than trusting whatever arrives. */
#define RV4A_BLOCK 4096

extern "C" {
#include "jdsp_header.h"
// Defined in jdspController.c but not exposed by the header, so declare it here
// rather than editing the vendored engine.
void JamesDSPProcess(JamesDSPLib *jdsp, size_t n);
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
    bool configured;
    uint32_t sampleRate;
    uint32_t channels;
    uint8_t outAccessMode;   /* write or accumulate */
    uint64_t blocks;         /* processed block counter, for log throttling */
};

static int32_t rv4a_process(effect_handle_t self, audio_buffer_t *in, audio_buffer_t *out)
{
    rv4a_context *c = reinterpret_cast<rv4a_context *>(self);
    if (!c || !in || !out || !in->raw || !out->raw)
    {
        LOGE("process: null buffer (self=%p in=%p out=%p)", self, in, out);
        return -EINVAL;
    }
    if (!c->active)
        return -ENODATA;              /* convention: nothing written */
    if (!c->configured)
    {
        LOGW("process: called before SET_CONFIG, passing through");
        return -ENODATA;
    }

    /* Never read or write past the shorter of the two buffers. */
    size_t frames = in->frameCount < out->frameCount ? in->frameCount : out->frameCount;
    if (frames == 0)
        return 0;

    static thread_local float *left = nullptr, *right = nullptr;
    static thread_local size_t capacity = 0;
    if (capacity < RV4A_BLOCK)
    {
        free(left); free(right);
        left = (float *)malloc(RV4A_BLOCK * sizeof(float));
        right = (float *)malloc(RV4A_BLOCK * sizeof(float));
        if (!left || !right)
        {
            free(left); free(right);
            left = right = nullptr; capacity = 0;
            LOGE("process: out of memory for scratch buffers");
            return -ENOMEM;
        }
        capacity = RV4A_BLOCK;
        LOGI("process: scratch buffers allocated (%d frames)", RV4A_BLOCK);
    }

    const bool accumulate = (c->outAccessMode == EFFECT_BUFFER_ACCESS_ACCUMULATE);

    for (size_t off = 0; off < frames; off += RV4A_BLOCK)
    {
        const size_t n = (frames - off) > RV4A_BLOCK ? RV4A_BLOCK : (frames - off);

        for (size_t i = 0; i < n; i++)
        {
            left[i]  = in->f32[(off + i) * 2];
            right[i] = in->f32[(off + i) * 2 + 1];
        }

        c->dsp.tmpBuffer[0] = left;
        c->dsp.tmpBuffer[1] = right;
        JamesDSPProcess(&c->dsp, n);

        for (size_t i = 0; i < n; i++)
        {
            const size_t o = (off + i) * 2;
            if (accumulate)
            {
                out->f32[o]     += left[i];
                out->f32[o + 1] += right[i];
            }
            else
            {
                out->f32[o]     = left[i];
                out->f32[o + 1] = right[i];
            }
        }
    }

    /* One line every few thousand blocks: enough to confirm audio is flowing
       without flooding logcat on the audio thread. */
    if ((c->blocks++ % 2000) == 0)
        LOGD("process: alive, %zu frames, rate %u, %s", frames, c->sampleRate,
             accumulate ? "accumulate" : "write");
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
        LOGI("INIT");
        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;

    case EFFECT_CMD_SET_CONFIG:
    {
        if (!pCmdData || cmdSize != sizeof(effect_config_t))
        {
            LOGE("SET_CONFIG: bad payload (size %u, expected %zu)",
                 cmdSize, sizeof(effect_config_t));
            return -EINVAL;
        }
        effect_config_t *cfg = (effect_config_t *)pCmdData;

        LOGI("SET_CONFIG: in rate=%u fmt=0x%x chans=0x%x | out rate=%u fmt=0x%x chans=0x%x access=%d",
             cfg->inputCfg.samplingRate, cfg->inputCfg.format, cfg->inputCfg.channels,
             cfg->outputCfg.samplingRate, cfg->outputCfg.format, cfg->outputCfg.channels,
             cfg->outputCfg.accessMode);

        /* The engine works on planar float stereo. Anything else would be
           misread as floats and produce noise, so refuse it loudly instead. */
        if (cfg->inputCfg.format != AUDIO_FORMAT_PCM_FLOAT ||
            cfg->outputCfg.format != AUDIO_FORMAT_PCM_FLOAT)
        {
            LOGE("SET_CONFIG: refusing non-float format (in 0x%x out 0x%x)",
                 cfg->inputCfg.format, cfg->outputCfg.format);
            return -EINVAL;
        }
        if (cfg->inputCfg.channels != AUDIO_CHANNEL_OUT_STEREO ||
            cfg->outputCfg.channels != AUDIO_CHANNEL_OUT_STEREO)
        {
            LOGE("SET_CONFIG: refusing non-stereo (in 0x%x out 0x%x)",
                 cfg->inputCfg.channels, cfg->outputCfg.channels);
            return -EINVAL;
        }

        c->sampleRate = cfg->inputCfg.samplingRate;
        c->channels = 2;
        c->outAccessMode = cfg->outputCfg.accessMode;

        JamesDSPInit(&c->dsp, RV4A_BLOCK, c->sampleRate);
        if (!JamesDSPGetMutexStatus(&c->dsp))
            LOGW("SET_CONFIG: engine reports no mutex; concurrent access unsafe");
        c->configured = true;
        LOGI("SET_CONFIG: engine initialised at %u Hz, block %d", c->sampleRate, RV4A_BLOCK);

        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;
    }

    case EFFECT_CMD_GET_CONFIG:
        /* Some servers query this before enabling; failing it can stop the
           effect being used at all. */
        if (pReplyData && replySize && *replySize >= sizeof(effect_config_t))
            return 0;
        return -EINVAL;

    case EFFECT_CMD_RESET:
    case EFFECT_CMD_SET_DEVICE:
    case EFFECT_CMD_SET_INPUT_DEVICE:
    case EFFECT_CMD_SET_VOLUME:
    case EFFECT_CMD_SET_AUDIO_MODE:
        /* Benign notifications. Refusing them makes the audio server treat the
           effect as broken and drop it, which is worse than ignoring them. */
        LOGD("command 0x%x acknowledged", cmdCode);
        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;

    case EFFECT_CMD_ENABLE:
        c->active = true;
        LOGI("ENABLE (configured=%d, rate=%u)", c->configured, c->sampleRate);
        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;

    case EFFECT_CMD_DISABLE:
        c->active = false;
        LOGI("DISABLE");
        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;

    case EFFECT_CMD_SET_PARAM:
    {
        /* AudioEffect packs the id and value into effect_param_t: the id is a
           4-byte int, the value follows it aligned to 4 bytes, and vsize says
           how wide it is (2 for the short overload the app mostly uses). */
        if (!pCmdData || cmdSize < sizeof(effect_param_t))
            return -EINVAL;
        effect_param_t *p = (effect_param_t *)pCmdData;
        if (p->psize != sizeof(int32_t))
            return -EINVAL;

        const int32_t id = *(int32_t *)p->data;
        const void *val = p->data + ((p->psize + 3) & ~3);
        const int16_t sv = (p->vsize >= sizeof(int16_t)) ? *(const int16_t *)val : 0;
        const bool on = sv != 0;

        /* Fork effects arrive as one float array per effect rather than an id
           per value, so hand the payload through as well. */
        LOGD("SET_PARAM id=%d vsize=%u short=%d", id, p->vsize, sv);
        applyParam(&c->dsp, id, sv, on, (const float *)val, p->vsize / sizeof(float));

        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;
    }

    default:
        LOGD("command 0x%x not handled", cmdCode);
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

/* The library is built with hidden visibility so the engine's internals stay
 * private, which means the four symbols the audio server looks up must be
 * marked visible explicitly - without this the module loads but exports
 * nothing, and the effect silently never appears. */
#define HAL_EXPORT __attribute__((visibility("default")))

extern "C" {

HAL_EXPORT int32_t EffectCreate(const effect_uuid_t *uuid, int32_t sessionId, int32_t ioId,
                     effect_handle_t *pHandle)
{
    if (!pHandle || !uuid)
        return -EINVAL;
    if (memcmp(uuid, &rv4a_descriptor.uuid, sizeof(effect_uuid_t)) != 0)
        return -EINVAL;

    rv4a_context *c = (rv4a_context *)calloc(1, sizeof(rv4a_context));
    if (!c)
    {
        LOGE("EffectCreate: out of memory");
        return -ENOMEM;
    }
    c->itfe = &rv4a_interface;
    c->active = false;
    c->configured = false;
    c->outAccessMode = EFFECT_BUFFER_ACCESS_WRITE;

    /* Shared tables, allocated once for the process rather than per effect. */
    static bool globalReady = false;
    if (!globalReady)
    {
        JamesDSPGlobalMemoryAllocation();
        globalReady = true;
        LOGI("global engine tables allocated");
    }
    JamesDSPInit(&c->dsp, RV4A_BLOCK, 48000);
    LOGI("EffectCreate: session=%d io=%d handle=%p", sessionId, ioId, (void *)c);
    *pHandle = (effect_handle_t)c;
    return 0;
}

HAL_EXPORT int32_t EffectRelease(effect_handle_t handle)
{
    rv4a_context *c = reinterpret_cast<rv4a_context *>(handle);
    if (!c)
        return -EINVAL;
    LOGI("EffectRelease: handle=%p after %llu blocks", (void *)c,
         (unsigned long long)c->blocks);
    JamesDSPFree(&c->dsp);
    free(c);
    return 0;
}

HAL_EXPORT int32_t EffectGetDescriptor(const effect_uuid_t *uuid, effect_descriptor_t *pDescriptor)
{
    if (!pDescriptor || !uuid)
        return -EINVAL;
    memcpy(pDescriptor, &rv4a_descriptor, sizeof(effect_descriptor_t));
    return 0;
}

HAL_EXPORT audio_effect_library_t AUDIO_EFFECT_LIBRARY_INFO_SYM =
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
