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

/*
 * Maps the ids the app sends over AudioEffect onto the engine's own setters.
 *
 * The ids come from JamesDspRemoteEngine: the 12xx range switches an effect on
 * or off, while the lower ids carry that effect's value. Values arrive as
 * shorts, scaled the way the app scales them - vacuum tube level is sent
 * multiplied by a thousand, for instance, so it is divided back here.
 *
 * Effects the app currently stubs out in root mode (the fork's own additions)
 * have no ids to receive yet; they are handled once the app side sends them.
 */
static void applyParam(rv4a_context *c, int32_t id, int16_t sv, bool on,
                       const float *fv, uint32_t fn)
{
    JamesDSPLib *d = &c->dsp;
    switch (id)
    {
    /* --- enable flags --------------------------------------------------- */
    case 1200: if (on) CompressorEnable(d, 1); else CompressorEnable(d, 0); break;
    case 1201: if (on) BassBoostEnable(d); else BassBoostDisable(d); break;
    case 1202: if (on) MultimodalEqualizerEnable(d, 1); else MultimodalEqualizerEnable(d, 0); break;
    case 1203: if (on) ReverbEnable(d); else ReverbDisable(d); break;
    case 1204: if (on) StereoEnhancementEnable(d); else StereoEnhancementDisable(d); break;
    case 1205: if (on) Convolver1DEnable(d); else Convolver1DDisable(d); break;
    case 1206: if (on) VacuumTubeEnable(d); else VacuumTubeDisable(d); break;
    case 1208: if (on) CrossfeedEnable(d, 1); else CrossfeedEnable(d, 0); break;
    case 1210: if (on) ArbitraryResponseEqualizerEnable(d, 1); else ArbitraryResponseEqualizerDisable(d); break;
    case 1212: DDCEnable(d, on ? 1 : 0); break;
    case 1213: if (on) LiveProgEnable(d); else LiveProgDisable(d); break;

    /* --- values --------------------------------------------------------- */
    case 112: BassBoostSetParam(d, (float)sv); break;                 /* max gain, dB */
    case 128: Reverb_SetParam(d, sv); break;                          /* preset index */
    case 137: StereoEnhancementSetParam(d, (float)sv); break;         /* width */
    case 150: VacuumTubeSetGain(d, (double)sv / 1000.0); break;       /* sent x1000 */
    case 188: CrossfeedChangeMode(d, sv); break;                      /* mode index */


    /* --- effects this fork adds ----------------------------------------
       Values come as a float array in the order the app packs them; enable
       flags sit one hundred above the value id. Anything with too few values
       is ignored rather than half-applied. */
    case 26000: if (fn >= 2) ViperClaritySetParam(d, (int)fv[0], fv[1]); break;
    case 26100: if (on) ViperClarityEnable(d); else ViperClarityDisable(d); break;

    case 26001: if (fn >= 2) FieldSurroundSetParam(d, fv[0], fv[1]); break;
    case 26101: if (on) FieldSurroundEnable(d); else FieldSurroundDisable(d); break;

    case 26002: if (fn >= 2) AgcSetParam(d, fv[0], fv[1]); break;
    case 26102: if (on) AgcEnable(d); else AgcDisable(d); break;

    case 26003: if (fn >= 2) HpSurroundSetParam(d, fv[0], fv[1]); break;
    case 26103: if (on) HpSurroundEnable(d); else HpSurroundDisable(d); break;

    case 26006: if (fn >= 3) ViperBassSetParam(d, (int)fv[0], fv[1], fv[2]); break;
    case 26106: if (on) ViperBassEnable(d); else ViperBassDisable(d); break;

    case 26007:
        if (fn >= 12)
            VReverbSetParam(d, (int)fv[0], fv[1], fv[2], fv[3], fv[4], fv[5],
                            fv[6], fv[7], fv[8], fv[9], fv[10], fv[11]);
        break;
    case 26107: if (on) VReverbEnable(d); else VReverbDisable(d); break;

    case 26008: if (fn >= 1) SpeakerOptSetParam(d, fv[0]); break;
    case 26108: if (on) SpeakerOptEnable(d); else SpeakerOptDisable(d); break;

    case 26011: ArbitraryResponseEqualizerSetPhaseMode(d, on ? 1 : 0); break;

    default:
        /* Unknown ids are accepted rather than refused: returning an error
           here makes the audio server tear the effect down entirely. */
        break;
    }
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
        applyParam(c, id, sv, on, (const float *)val, p->vsize / sizeof(float));

        if (pReplyData && replySize && *replySize == sizeof(int))
            *(int *)pReplyData = 0;
        return 0;
    }

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

/* The library is built with hidden visibility so the engine's internals stay
 * private, which means the four symbols the audio server looks up must be
 * marked visible explicitly - without this the module loads but exports
 * nothing, and the effect silently never appears. */
#define HAL_EXPORT __attribute__((visibility("default")))

extern "C" {

HAL_EXPORT int32_t EffectCreate(const effect_uuid_t *uuid, int32_t sessionId, int32_t ioId,
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

HAL_EXPORT int32_t EffectRelease(effect_handle_t handle)
{
    rv4a_context *c = reinterpret_cast<rv4a_context *>(handle);
    if (!c)
        return -EINVAL;
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
