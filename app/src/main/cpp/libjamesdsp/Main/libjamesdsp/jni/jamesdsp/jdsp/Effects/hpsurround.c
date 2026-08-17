// Headphone Surround+ : binaural externalization for headphones.
// Delayed, head-shadow-filtered crossfeed (ITD/ILD cues) + multi-tap early
// reflections with feedback + mid/side widening. Inspired by ViPER VHS.
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../jdsp_header.h"

#define HPS_BUFLEN 8192

void HpSurroundSetParam(JamesDSPLib *jdsp, float strengthPct, float roomPct)
{
	HpSurround *h = &jdsp->hpSurround;
	float fs = (float)jdsp->fs;
	if (fs < 8000.0f)
		fs = 48000.0f;
	float s = strengthPct * 0.01f;
	float r = roomPct * 0.01f;
	if (s < 0.0f) s = 0.0f; if (s > 1.0f) s = 1.0f;
	if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;

	// Crossfeed: much stronger than a subtle blend - this is what pulls the
	// image out of the middle of your head.
	h->cross = 0.95f * s;
	h->room = 0.75f * r;
	// Stereo widening rises with strength (1.0 = untouched, 2.2 = very wide)
	h->width = 1.0f + 1.2f * s;
	// Feedback gives the reflections a short decaying tail (room sense)
	h->fb = 0.42f * r;

	// Interaural time difference ~0.3 ms, plus early reflection taps
	h->dCross = (int)(0.00030f * fs);
	h->dR1 = (int)(0.0090f * fs);
	h->dR2 = (int)(0.0170f * fs);
	h->dR3 = (int)(0.0290f * fs);
	if (h->dR3 > HPS_BUFLEN - 8) h->dR3 = HPS_BUFLEN - 8;
	if (h->dR2 > HPS_BUFLEN - 8) h->dR2 = HPS_BUFLEN - 8;
	if (h->dR1 > HPS_BUFLEN - 8) h->dR1 = HPS_BUFLEN - 8;
	if (h->dCross < 1) h->dCross = 1;

	// Head-shadow lowpass on the crossfeed path (~1100 Hz): the far ear hears
	// mostly low frequencies. Higher than a plain crossfeed so the effect is
	// clearly audible rather than subliminal.
	h->lpCoef = 1.0f - expf(-2.0f * 3.14159265f * 1100.0f / fs);
	// Damping lowpass in the reflection feedback path
	h->dampCoef = 1.0f - expf(-2.0f * 3.14159265f * 3500.0f / fs);
	// Output trim so heavy settings don't clip
	h->norm = 1.0f / (1.0f + 0.30f * h->cross + 0.28f * h->room);
}

void HpSurroundProcess(JamesDSPLib *jdsp, size_t n)
{
	HpSurround *h = &jdsp->hpSurround;
	size_t i;
	if (!jdsp->tmpBuffer[0] || !jdsp->tmpBuffer[1])
		return;
	if (!h->bufL || !h->bufR)
		return;
	for (i = 0; i < n; i++)
	{
		int w = h->widx;
		float l = jdsp->tmpBuffer[0][i];
		float r = jdsp->tmpBuffer[1][i];

		// Write input plus damped reflection feedback into the delay line
		h->bufL[w] = l + h->fbzL;
		h->bufR[w] = r + h->fbzR;

		int pc = w - h->dCross; if (pc < 0) pc += HPS_BUFLEN;
		int p1 = w - h->dR1; if (p1 < 0) p1 += HPS_BUFLEN;
		int p2 = w - h->dR2; if (p2 < 0) p2 += HPS_BUFLEN;
		int p3 = w - h->dR3; if (p3 < 0) p3 += HPS_BUFLEN;

		// Head-shadowed opposite-ear signal (ITD + ILD)
		h->lpzL += h->lpCoef * (h->bufR[pc] - h->lpzL);
		h->lpzR += h->lpCoef * (h->bufL[pc] - h->lpzR);

		// Early reflections, cross-mixed for envelopment
		float roomL = h->bufL[p1] * 0.62f + h->bufR[p2] * 0.48f + h->bufL[p3] * 0.34f;
		float roomR = h->bufR[p1] * 0.62f + h->bufL[p2] * 0.48f + h->bufR[p3] * 0.34f;

		// Damped feedback for a short decaying tail
		h->fbzL += h->dampCoef * (roomL * h->fb - h->fbzL);
		h->fbzR += h->dampCoef * (roomR * h->fb - h->fbzR);

		float outL = l + h->cross * h->lpzL + h->room * roomL;
		float outR = r + h->cross * h->lpzR + h->room * roomR;

		// Mid/side widening on top of the binaural cues
		float mid = (outL + outR) * 0.5f;
		float side = (outL - outR) * 0.5f * h->width;
		outL = mid + side;
		outR = mid - side;

		jdsp->tmpBuffer[0][i] = outL * h->norm;
		jdsp->tmpBuffer[1][i] = outR * h->norm;

		h->widx = (w + 1) & (HPS_BUFLEN - 1);
	}
}

void HpSurroundEnable(JamesDSPLib *jdsp)
{
	// Under the lock: Process may be running on the audio thread, and it must
	// not observe a half-built set of buffers.
	jdsp_lock(jdsp);
	if (!jdsp->hpSurroundEnabled)
	{
		HpSurround *hs = &jdsp->hpSurround;
		if (!hs->bufL) hs->bufL = (float*)calloc(HPS_BUFLEN, sizeof(float));
		if (!hs->bufR) hs->bufR = (float*)calloc(HPS_BUFLEN, sizeof(float));
		if (!hs->bufL || !hs->bufR)
		{
			jdsp->hpSurroundEnabled = 0;
			jdsp_unlock(jdsp);
			return;
		}
		memset(hs->bufL, 0, HPS_BUFLEN * sizeof(float));
		memset(hs->bufR, 0, HPS_BUFLEN * sizeof(float));
		jdsp->hpSurround.lpzL = jdsp->hpSurround.lpzR = 0.0f;
		jdsp->hpSurround.fbzL = jdsp->hpSurround.fbzR = 0.0f;
		jdsp->hpSurround.widx = 0;
	}
	jdsp->hpSurroundEnabled = 1;
	jdsp_unlock(jdsp);
}

void HpSurroundDisable(JamesDSPLib *jdsp)
{
	HpSurround *hs = &jdsp->hpSurround;
	jdsp->hpSurroundEnabled = 0;
	// Clear the flag first so no further block enters, then take the lock,
	// which waits for any block already inside Process to leave. Freeing
	// without that wait is a use-after-free on the audio thread.
	jdsp_lock(jdsp);
	if (hs->bufL) { free(hs->bufL); hs->bufL = 0; }
	if (hs->bufR) { free(hs->bufR); hs->bufR = 0; }
	jdsp_unlock(jdsp);
}
