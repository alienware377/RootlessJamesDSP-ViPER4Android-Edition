/*
 * Shared parameter dispatch.
 *
 * Both HALs receive the same ids from the app and must act on them identically:
 * the legacy one through EFFECT_CMD_SET_PARAM, the AIDL one through a vendor
 * extension carrying that very same legacy payload. One implementation means
 * the two cannot drift apart as effects are added.
 */
#pragma once

extern "C" {
#include "jdsp_header.h"
}

/* The legacy HAL defines these; the AIDL one logs through its own mechanism, so
   they fall away to nothing there rather than each HAL needing the other's. */
#ifndef LOGD
#define LOGD(...) ((void)0)
#endif

static inline void applyParam(JamesDSPLib *d, int32_t id, int16_t sv, bool on,
                              const float *fv, uint32_t fn)
{
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
    case 137: StereoEnhancementSetParam(d, (float)sv / 100.0f); break; /* width, sent x100 */
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

    case 26004:
        if (fn >= 5) FetCompSetParam(d, fv[0], fv[1], fv[2], fv[3], fv[4]);
        break;
    case 26104: if (on) FetCompEnable(d); else FetCompDisable(d); break;

    case 26005: if (fn >= 1) CureSetParam(d, (int)fv[0]); break;
    case 26105: if (on) CureEnable(d); else CureDisable(d); break;

    case 26009: if (fn >= 2) PitchShiftSetParam(d, fv[0], fv[1]); break;
    case 26109: if (on) PitchShiftEnable(d); else PitchShiftDisable(d); break;

    case 26010:
        if (fn >= 25)
            EchoDelaySetParam(d, fv[0], fv[1], fv[2], fv[3], fv[4] != 0.0f, (int)fv[5],
                              fv[6], fv[7], fv[8], fv[9], (int)fv[10], fv[11], fv[12],
                              fv[13], fv[14], fv[15], fv[16], fv[17], (int)fv[18],
                              fv[19], fv[20], fv[21], fv[22], fv[23], fv[24]);
        break;
    case 26110: if (on) EchoDelayEnable(d); else EchoDelayDisable(d); break;

    case 26011: ArbitraryResponseEqualizerSetPhaseMode(d, on ? 1 : 0); break;

    /* Mix, band count, then that many bands of DYNEQ_VALUES_PER_BAND. One
       array rather than two calls, so the bands and the mix cannot arrive out
       of step with each other. */
    case 26013:
        if (fn >= 2)
        {
            const int count = (int)(fv[1] + 0.5f);
            const uint32_t need = 2u + (uint32_t)count * DYNEQ_VALUES_PER_BAND;
            if (count >= 0 && fn >= need)
            {
                DynamicEqSetBands(d, fv + 2, count);
                DynamicEqSetParam(d, fv[0]);
            }
        }
        break;
    case 26113: if (on) DynamicEqEnable(d); else DynamicEqDisable(d); break;

    case 26014:
        if (fn >= 8)
            ImagingSetParam(d, fv[0], fv[1], fv[2], fv[3], fv[4], fv[5], fv[6], fv[7]);
        break;
    case 26114: if (on) ImagingEnable(d); else ImagingDisable(d); break;

    case 26015:
        if (fn >= 10)
            TransientSetParam(d, fv[0], fv[1], fv[2], fv[3], fv[4], fv[5],
                              fv[6], fv[7], fv[8], fv[9]);
        break;
    case 26115: if (on) TransientEnable(d); else TransientDisable(d); break;

    case 26016:
        if (fn >= 6)
            LowEndSetParam(d, fv[0], fv[1], fv[2], fv[3], fv[4], fv[5]);
        break;
    case 26116: if (on) LowEndEnable(d); else LowEndDisable(d); break;

    /* The order arrives as ints, so it is read from the raw payload rather
       than through the float view every other effect uses. */
    case 26012:
        if (fn >= 1) JamesDSPSetChainOrder(d, (const int *)fv, (int)fn);
        break;

    /* --- upstream ids this HAL used to drop ------------------------------
       These are not fork additions; they are the app's own EQ, dynamics and
       output stage. Leaving them unhandled meant the root build silently lost
       the equaliser, the compander and the limiter while reporting success. */
    case 1500:
        if (fn >= 3)
        {
            double threshold = (double)fv[0];
            double release = (double)fv[1];
            double postGain = (double)fv[2];
            /* Same clamps the engine's own handler applies: a threshold at or
               above 0 dB and a release below 0.15 ms both make the limiter
               misbehave rather than merely sound different. */
            if (threshold > -0.09) threshold = -0.09;
            if (release < 0.15) release = 0.15;
            if (postGain > 15.0) postGain = 15.0;
            if (postGain < -15.0) postGain = -15.0;
            JLimiterSetCoefficients(d, threshold, release);
            JamesDSPSetPostGain(d, postGain);
        }
        break;

    case 115:
        /* time constant, granularity, tf resolution, then 7 frequencies and
           7 gains. */
        if (fn >= 17)
        {
            double axis[14];
            for (int i = 0; i < 7; i++)
            {
                axis[i] = (double)fv[3 + i];
                axis[i + 7] = (double)fv[3 + 7 + i];
            }
            CompressorSetParam(d, fv[0], (int)(fv[1] + 0.5f), (int)(fv[2] + 0.5f), 0);
            CompressorSetGain(d, axis, axis + 7, 1);
        }
        break;

    case 116:
        /* filter type, interpolation mode, then 15 frequencies and 15 gains. */
        if (fn >= 32)
        {
            double axis[30];
            for (int i = 0; i < 15; i++)
            {
                axis[i] = (double)fv[2 + i];
                axis[i + 15] = (double)fv[2 + 15 + i];
            }
            MultimodalEqualizerAxisInterpolation(
                d, fv[1] < 0.0f ? 0 : 1, (int)(fv[0] + 0.5f), axis, axis + 15);
        }
        break;

    default:
        LOGD("param id %d ignored (no mapping)", id);
        /* Unknown ids are accepted rather than refused: returning an error
           here makes the audio server tear the effect down entirely. */
        break;
    }
}

