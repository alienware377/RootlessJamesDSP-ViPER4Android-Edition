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
        LOGD("param id %d ignored (no mapping)", id);
        /* Unknown ids are accepted rather than refused: returning an error
           here makes the audio server tear the effect down entirely. */
        break;
    }
}

