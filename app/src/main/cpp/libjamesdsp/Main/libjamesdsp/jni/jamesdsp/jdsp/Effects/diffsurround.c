// Differential surround: per-channel fractional delay (Haas widening),
// extracted from the ViperFX differential surround concept.
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../jdsp_header.h"

// 50ms is 9600 samples at 192kHz, so the old 8192 could not actually reach the
// top of the range there - it silently clamped to 42ms. 16384 covers the whole
// range at every sample rate the engine accepts, and the pair is now allocated
// on enable rather than sitting in the library struct: 128KB resident per
// instance, times a session for every app playing audio, is real memory to be
// holding for an effect that is switched off.
#define DSUR_BUFLEN 16384

void DiffSurroundSetParam(JamesDSPLib *jdsp, float delayLms, float delayRms)
{
	DiffSurround *ds = &jdsp->diffSurround;
	float fs = (float)jdsp->fs;
	if (fs < 8000.0f)
		fs = 48000.0f;
	float maxDelay = (float)(DSUR_BUFLEN - 4);
	float dl = delayLms * 0.001f * fs;
	float dr = delayRms * 0.001f * fs;
	if (dl < 0.0f) dl = 0.0f;
	if (dr < 0.0f) dr = 0.0f;
	if (dl > maxDelay) dl = maxDelay;
	if (dr > maxDelay) dr = maxDelay;
	ds->delayL = dl;
	ds->delayR = dr;
}

void DiffSurroundProcess(JamesDSPLib *jdsp, size_t n)
{
	DiffSurround *ds = &jdsp->diffSurround;
	size_t i;
	if (!jdsp->tmpBuffer[0] || !jdsp->tmpBuffer[1])
		return;
	if (!ds->bufL || !ds->bufR)
		return;
	for (i = 0; i < n; i++)
	{
		int w = ds->widx;
		ds->bufL[w] = jdsp->tmpBuffer[0][i];
		ds->bufR[w] = jdsp->tmpBuffer[1][i];

		float rpL = (float)w - ds->delayL;
		float rpR = (float)w - ds->delayR;
		if (rpL < 0.0f) rpL += DSUR_BUFLEN;
		if (rpR < 0.0f) rpR += DSUR_BUFLEN;
		int iL = (int)rpL;
		int iR = (int)rpR;
		float fL = rpL - (float)iL;
		float fR = rpR - (float)iR;
		int iL1 = iL + 1; if (iL1 >= DSUR_BUFLEN) iL1 = 0;
		int iR1 = iR + 1; if (iR1 >= DSUR_BUFLEN) iR1 = 0;

		jdsp->tmpBuffer[0][i] = ds->bufL[iL] + fL * (ds->bufL[iL1] - ds->bufL[iL]);
		jdsp->tmpBuffer[1][i] = ds->bufR[iR] + fR * (ds->bufR[iR1] - ds->bufR[iR]);

		ds->widx = (w + 1) & (DSUR_BUFLEN - 1);
	}
}

void DiffSurroundEnable(JamesDSPLib *jdsp)
{
	// Under the lock: Process may be running on the audio thread, and it must
	// not observe a half-built set of buffers.
	jdsp_lock(jdsp);
	DiffSurround *ds = &jdsp->diffSurround;
	if (!jdsp->diffSurroundEnabled)
	{
		if (!ds->bufL) ds->bufL = (float*)calloc(DSUR_BUFLEN, sizeof(float));
		if (!ds->bufR) ds->bufR = (float*)calloc(DSUR_BUFLEN, sizeof(float));
		if (!ds->bufL || !ds->bufR)
		{
			// Refuse to run half allocated rather than dereference a null on
			// the audio thread.
			// Inline rather than calling Disable: that would take the same
			// non-recursive lock this function is already holding.
			if (ds->bufL) { free(ds->bufL); ds->bufL = 0; }
			if (ds->bufR) { free(ds->bufR); ds->bufR = 0; }
			jdsp->diffSurroundEnabled = 0;
			jdsp_unlock(jdsp);
			return;
		}
		ds->widx = 0;
		// The delays were computed against whatever sample rate was current
		// when they were last set; re-clamp them here so a rate change cannot
		// leave a read pointer outside the ring.
		float fs = (float)jdsp->fs;
		if (fs < 8000.0f) fs = 48000.0f;
		float maxDelay = (float)(DSUR_BUFLEN - 4);
		if (ds->delayL > maxDelay) ds->delayL = maxDelay;
		if (ds->delayR > maxDelay) ds->delayR = maxDelay;
	}
	jdsp->diffSurroundEnabled = 1;
	jdsp_unlock(jdsp);
}

void DiffSurroundDisable(JamesDSPLib *jdsp)
{
	DiffSurround *ds = &jdsp->diffSurround;
	jdsp->diffSurroundEnabled = 0;
	// Clear the flag first so no further block enters, then take the lock,
	// which waits for any block already inside Process to leave. Freeing
	// without that wait is a use-after-free on the audio thread.
	jdsp_lock(jdsp);
	if (ds->bufL) { free(ds->bufL); ds->bufL = 0; }
	if (ds->bufR) { free(ds->bufR); ds->bufR = 0; }
	jdsp_unlock(jdsp);
}
