// Granular pitch shifter: dual-tap ring buffer with equal-power sine
// crossfade (classic harmonizer topology). Shifts pitch without tempo change.
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../jdsp_header.h"

#define PS_BUFLEN 8192

void PitchShiftSetParam(JamesDSPLib *jdsp, float semitones, float mixPct)
{
	PitchShift *p = &jdsp->pitchShift;
	float fs = (float)jdsp->fs;
	if (fs < 8000.0f) fs = 48000.0f;
	if (semitones > 12.0f) semitones = 12.0f;
	if (semitones < -12.0f) semitones = -12.0f;
	p->rate = powf(2.0f, semitones / 12.0f);
	p->mix = mixPct * 0.01f;
	if (p->mix < 0.0f) p->mix = 0.0f;
	if (p->mix > 1.0f) p->mix = 1.0f;
	int win = (int)(0.05f * fs);
	if (win > PS_BUFLEN - 200) win = PS_BUFLEN - 200;
	if (win < 256) win = 256;
	p->win = win;
	// No shift asked for and nothing to blend against: skip the stage rather
	// than run it. It is not a null - at 44.1kHz the half-window makes the
	// read interpolate between neighbouring samples, which is a two-tap
	// lowpass six decibels down at 15kHz, so leaving it running costs treble
	// and 25ms of latency to achieve nothing.
	p->bypass = (fabsf(semitones) < 1e-6f && p->mix >= 0.999f) ? 1 : 0;
}

static inline float psRead(const float *buf, float pos)
{
	int i0 = (int)pos;
	float fr = pos - (float)i0;
	int i1 = (i0 + 1) & (PS_BUFLEN - 1);
	return buf[i0 & (PS_BUFLEN - 1)] * (1.0f - fr) + buf[i1] * fr;
}

void PitchShiftProcess(JamesDSPLib *jdsp, size_t n)
{
	PitchShift *p = &jdsp->pitchShift;
	size_t i;
	if (!jdsp->tmpBuffer[0] || !jdsp->tmpBuffer[1])
		return;
	if (p->bypass)
		return;
	if (!p->buf[0] || !p->buf[1])
		return;
	float W = (float)p->win;
	float halfW = W * 0.5f;
	float piW = 3.14159265358979f / W;
	for (i = 0; i < n; i++)
	{
		int w = p->w;
		float l = jdsp->tmpBuffer[0][i];
		float r = jdsp->tmpBuffer[1][i];
		p->buf[0][w] = l;
		p->buf[1][w] = r;

		float d1 = p->phasor;
		float d2 = d1 + halfW;
		if (d2 >= W) d2 -= W;
		float g1 = sinf(piW * d1);
		float g2 = sinf(piW * d2);

		float p1 = (float)w - d1;
		float p2 = (float)w - d2;
		if (p1 < 0.0f) p1 += PS_BUFLEN;
		if (p2 < 0.0f) p2 += PS_BUFLEN;

		float wetL = g1 * psRead(p->buf[0], p1) + g2 * psRead(p->buf[0], p2);
		float wetR = g1 * psRead(p->buf[1], p1) + g2 * psRead(p->buf[1], p2);

		jdsp->tmpBuffer[0][i] = l + p->mix * (wetL - l);
		jdsp->tmpBuffer[1][i] = r + p->mix * (wetR - r);

		p->phasor += (1.0f - p->rate);
		while (p->phasor >= W) p->phasor -= W;
		while (p->phasor < 0.0f) p->phasor += W;
		p->w = (w + 1) & (PS_BUFLEN - 1);
	}
}

void PitchShiftEnable(JamesDSPLib *jdsp)
{
	// Under the lock: Process may be running on the audio thread, and it must
	// not observe a half-built set of buffers.
	jdsp_lock(jdsp);
	if (!jdsp->pitchShiftEnabled)
	{
		PitchShift *ps = &jdsp->pitchShift;
		for (int c = 0; c < 2; c++)
			if (!ps->buf[c])
				ps->buf[c] = (float*)calloc(PS_BUFLEN, sizeof(float));
		if (!ps->buf[0] || !ps->buf[1])
		{
			jdsp->pitchShiftEnabled = 0;
			jdsp_unlock(jdsp);
			return;
		}
		for (int c = 0; c < 2; c++)
			memset(ps->buf[c], 0, PS_BUFLEN * sizeof(float));
		ps->w = 0;
		ps->phasor = 0.0f;
	}
	jdsp->pitchShiftEnabled = 1;
	jdsp_unlock(jdsp);
}

void PitchShiftDisable(JamesDSPLib *jdsp)
{
	PitchShift *ps = &jdsp->pitchShift;
	jdsp->pitchShiftEnabled = 0;
	// Clear the flag first so no further block enters, then take the lock,
	// which waits for any block already inside Process to leave. Freeing
	// without that wait is a use-after-free on the audio thread.
	jdsp_lock(jdsp);
	for (int c = 0; c < 2; c++)
		if (ps->buf[c]) { free(ps->buf[c]); ps->buf[c] = 0; }
	jdsp_unlock(jdsp);
}
