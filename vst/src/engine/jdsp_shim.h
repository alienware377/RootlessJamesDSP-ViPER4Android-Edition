// Minimal stand-in for the engine's header, so multibandDist.c compiles here
// unmodified. It touches only four fields of JamesDSPLib - fs, tmpBuffer, the
// effect's own state and its enable flag - so the rest of a 4000-line header is
// not needed, and keeping the effect source byte-identical means the plugin
// cannot drift from the app as the effect is developed.
#pragma once
#include <stddef.h>
#include "digitalFilters.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MBD_MAX_BANDS 24
#define MBD_MAX_CUTOFF_STAGES 8
#define MBD_CHORUS_BUFLEN 8192
#define MBD_CHORUS_VOICES 4
#define MBD_OS_MAX 8

enum MbdModel
{
	MBD_MODEL_SOFT = 0,
	MBD_MODEL_HARD,
	MBD_MODEL_TUBE,
	MBD_MODEL_OVERDRIVE,
	MBD_MODEL_FOLD,
	MBD_MODEL_FUZZ,
	MBD_MODEL_RECTIFY,
	MBD_MODEL_CRUSH,
	MBD_MODEL_COUNT
};
enum MbdRouting
{
	MBD_ROUTING_SPLIT = 0,
	MBD_ROUTING_PARALLEL
};
enum MbdFilterType
{
	MBD_FILTER_PEAKING = 0,
	MBD_FILTER_LOW_SHELF,
	MBD_FILTER_HIGH_SHELF,
	MBD_FILTER_LOW_PASS,
	MBD_FILTER_HIGH_PASS
};

typedef struct
{
	// Band-select filter: a biquad cascade built from the parametric band list
	// the editor produces, with per-channel state.
	int numBands;
	float b0[MBD_MAX_BANDS], b1[MBD_MAX_BANDS], b2[MBD_MAX_BANDS];
	float a1[MBD_MAX_BANDS], a2[MBD_MAX_BANDS];
	float z1[2][MBD_MAX_BANDS], z2[2][MBD_MAX_BANDS];
	// Distortion
	int routing, model, transparent;
	float drive, shaperBlend, bias, shape, bitStep, holdLen;
	float tilt, bandGain, mix;
	float toneA, toneZ[2];
	float dcR, dcX[2], dcY[2];
	float shHold[2], shPhase[2];
	int osFactor;
	samplerateTool smpUp[2], smpDown[2];
	// Chorus on the distorted band; buffers held only while enabled.
	float *chBufL, *chBufR;
	int chPos[2], chVoices;
	float chPhase, chInc, chBase, chDepth, chFeedback, chSpread, chMix;
	float fs;
} MultibandDist;

typedef struct {
    int fs;
    float *tmpBuffer[2];
    MultibandDist multibandDist;
    char multibandDistEnabled;
} JamesDSPLib;

#ifdef __cplusplus
extern "C" {
#endif
void MultibandDistSetBands(JamesDSPLib *jdsp, const float *bands, int count);
void MultibandDistSetParam(JamesDSPLib *jdsp, int routing, int model,
    float drivePct, float biasPct, float shapePct, float bits, float downsamplePct,
    float tonePct, float bandGainPct, float chorusRateHz, float chorusDepthMs,
    float chorusFeedbackPct, float chorusSpreadPct, int chorusVoices,
    float chorusMixPct, float mixPct);
void MultibandDistProcess(JamesDSPLib *jdsp, size_t n);
void MultibandDistEnable(JamesDSPLib *jdsp);
void MultibandDistDisable(JamesDSPLib *jdsp);
#ifdef __cplusplus
}
#endif
