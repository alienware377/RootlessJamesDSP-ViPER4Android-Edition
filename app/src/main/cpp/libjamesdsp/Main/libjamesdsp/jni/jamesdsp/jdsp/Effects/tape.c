/*
 * Vintage tape: the ways a tape machine fails, offered as colour.
 *
 * Four of them, because between them they account for most of what people mean
 * by "sounds like tape":
 *
 *   Wow       Slow speed variation, well under a cycle a second - an eccentric
 *             reel or a tired capstan. Heard as pitch drifting rather than
 *             wobbling.
 *   Flutter   The same thing an order of magnitude faster, from the tape
 *             fighting the guides. Heard as a shimmer on sustained notes.
 *   Saturation Magnetic tape runs out of headroom gently rather than clipping,
 *             rounding peaks and adding odd harmonics as it goes.
 *   Bias      Set low, a machine gets brighter and distorts more; set high it
 *             goes dull. One control spanning both, flat in the middle.
 *
 * Plus the head bump: the low resonance a tape head's geometry produces, which
 * is why tape masters often sound weightier around 60 Hz than the mix did.
 *
 * Wow and flutter are one modulated delay line, since they are the same defect
 * at different speeds. Reading at a moving offset is what produces real pitch
 * change - a phase-modulated copy would only sound like it. The read position
 * is interpolated between samples, because stepping it in whole samples would
 * click on every move.
 */
#include <math.h>
#include <string.h>
#include "../jdsp_header.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Base read offset, in samples at 48k, scaled with the rate. The line has to
   sit far enough back that the modulation never runs off either end. */
#define TAPE_BASE_MS 12.0f

static void tapeDesignShelf(TapeStage *s, double f, float fs, float gainDb, int high)
{
	if (fabsf(gainDb) < 1e-6f)
	{
		s->b0 = 1.0f; s->b1 = 0.0f; s->b2 = 0.0f; s->a1 = 0.0f; s->a2 = 0.0f;
		return;
	}
	const double A = pow(10.0, (double)gainDb / 40.0);
	const double w0 = 2.0 * M_PI * f / (double)fs;
	const double cw = cos(w0), sw = sin(w0);
	const double alpha = sw / (2.0 * 0.70710678);
	const double sq = 2.0 * sqrt(A) * alpha;
	double b0, b1, b2, a0, a1, a2;
	if (high)
	{
		b0 = A * ((A + 1.0) + (A - 1.0) * cw + sq);
		b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
		b2 = A * ((A + 1.0) + (A - 1.0) * cw - sq);
		a0 = (A + 1.0) - (A - 1.0) * cw + sq;
		a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
		a2 = (A + 1.0) - (A - 1.0) * cw - sq;
	}
	else
	{
		b0 = A * ((A + 1.0) - (A - 1.0) * cw + sq);
		b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
		b2 = A * ((A + 1.0) - (A - 1.0) * cw - sq);
		a0 = (A + 1.0) + (A - 1.0) * cw + sq;
		a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
		a2 = (A + 1.0) + (A - 1.0) * cw - sq;
	}
	s->b0 = (float)(b0 / a0); s->b1 = (float)(b1 / a0); s->b2 = (float)(b2 / a0);
	s->a1 = (float)(a1 / a0); s->a2 = (float)(a2 / a0);
}

static void tapeDesignPeak(TapeStage *s, double f, float fs, float gainDb, double q)
{
	if (fabsf(gainDb) < 1e-6f)
	{
		s->b0 = 1.0f; s->b1 = 0.0f; s->b2 = 0.0f; s->a1 = 0.0f; s->a2 = 0.0f;
		return;
	}
	const double A = pow(10.0, (double)gainDb / 40.0);
	const double w0 = 2.0 * M_PI * f / (double)fs;
	const double alpha = sin(w0) / (2.0 * q);
	const double a0 = 1.0 + alpha / A;
	s->b0 = (float)((1.0 + alpha * A) / a0);
	s->b1 = (float)((-2.0 * cos(w0)) / a0);
	s->b2 = (float)((1.0 - alpha * A) / a0);
	s->a1 = (float)((-2.0 * cos(w0)) / a0);
	s->a2 = (float)((1.0 - alpha / A) / a0);
}

static float tapeRun(TapeStage *s, int ch, float x)
{
	const float y = s->b0 * x + s->z1[ch];
	s->z1[ch] = s->b1 * x - s->a1 * y + s->z2[ch];
	s->z2[ch] = s->b2 * x - s->a2 * y;
	return y;
}

/* Almost everything here is rate-dependent - the modulation depths are in
   samples, the oscillator increments in radians per sample, the read offset in
   samples and both filters in normalised frequency. Kept apart from storing the
   settings so a rate change can redo it. See LowEndRefresh for the reasoning. */
static void tapeDesign(Tape *t, float fs)
{
	/* Depths in samples. Real machines wobble by a fraction of a percent; these
	   go further so the control is usable as an effect rather than only as a
	   restoration of an old fault. */
	t->wowDepth = (t->wowPct * 0.01f) * 0.0035f * fs;
	t->flutterDepth = (t->flutterPct * 0.01f) * 0.00035f * fs;
	t->wowInc = 2.0f * (float)M_PI * 0.7f / fs;
	t->flutterInc = 2.0f * (float)M_PI * 9.0f / fs;

	/* Which way round this goes is worth writing down, because the intuitive
	   guess is backwards: under-biasing a machine gives MORE top end, with more
	   distortion alongside it, and over-biasing is what dulls the sound. So a
	   negative setting lifts the shelf. Measured at 12 kHz, -100 reads +5.95 dB
	   and +100 reads -5.95 dB. */
	tapeDesignShelf(&t->biasShelf, 4500.0, fs, -(t->bias * 0.01f) * 6.0f, 1);
	tapeDesignPeak(&t->headBump, 60.0, fs, t->headBumpDb, 1.1);

	t->base = TAPE_BASE_MS * 0.001f * fs;
	if (t->base < 4.0f) t->base = 4.0f;
	if (t->base > (float)(TAPE_LINE - 4)) t->base = (float)(TAPE_LINE - 4);
	t->fs = fs;
}

/* Unlocked on purpose - the caller already holds it. Note this leaves the delay
   line and the oscillator phases alone: the read offset moves to the new rate's
   equivalent of twelve milliseconds, which is a small step rather than the click
   that clearing the line would give. */
void TapeRefresh(JamesDSPLib *jdsp)
{
	tapeDesign(&jdsp->tape, jdsp->fs > 0.0f ? jdsp->fs : 48000.0f);
}

void TapeSetParam(JamesDSPLib *jdsp, float wowPct, float flutterPct,
                  float saturationPct, float biasPct, float headBumpDb,
                  float mixPct)
{
	/* Held for the whole update. Process runs on the audio thread under this
	   same lock, so without it a block could be filtered with half the old
	   coefficients and half the new ones - which for a steep or resonant
	   setting is not a glitch but a burst. The single-threaded harness cannot
	   see this, and the one crash this project has shipped came from exactly
	   this class of problem. */
	jdsp_lock(jdsp);
	Tape *t = &jdsp->tape;
	const float fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;

	if (wowPct < 0.0f) wowPct = 0.0f;
	if (wowPct > 100.0f) wowPct = 100.0f;
	if (flutterPct < 0.0f) flutterPct = 0.0f;
	if (flutterPct > 100.0f) flutterPct = 100.0f;
	t->wowPct = wowPct;
	t->flutterPct = flutterPct;

	if (saturationPct < 0.0f) saturationPct = 0.0f;
	if (saturationPct > 100.0f) saturationPct = 100.0f;
	t->saturation = saturationPct * 0.01f;
	/* Drive rises with the control so more of it means more harmonics, not
	   just more of the same shaped signal. */
	t->drive = 1.0f + t->saturation * 7.0f;

	if (biasPct < -100.0f) biasPct = -100.0f;
	if (biasPct > 100.0f) biasPct = 100.0f;
	t->bias = biasPct;

	if (headBumpDb < 0.0f) headBumpDb = 0.0f;
	if (headBumpDb > 9.0f) headBumpDb = 9.0f;
	t->headBumpDb = headBumpDb;

	tapeDesign(t, fs);

	t->mix = mixPct * 0.01f;
	if (t->mix < 0.0f) t->mix = 0.0f;
	if (t->mix > 1.0f) t->mix = 1.0f;

	/* Nothing asked for means nothing done - and it has to be a real early
	   return, because the delay line alone would shift the whole signal twelve
	   milliseconds even with no modulation on it. */
	t->transparent = wowPct <= 0.0f && flutterPct <= 0.0f &&
		saturationPct <= 0.0f && fabsf(biasPct) < 1e-6f && headBumpDb <= 0.0f;
	jdsp_unlock(jdsp);
}

void TapeProcess(JamesDSPLib *jdsp, size_t n)
{
	Tape *t = &jdsp->tape;
	if (t->mix <= 0.0f || t->transparent)
		return;

	float *left = jdsp->tmpBuffer[0];
	float *right = jdsp->tmpBuffer[1];
	const float inv = 1.0f / t->drive;

	for (size_t i = 0; i < n; i++)
	{
		const float dry[2] = { left[i], right[i] };

		/* One pair of oscillators drives both channels, so the two sides drift
		   together. Independent ones would swing the image about, which is a
		   chorus rather than a worn machine. */
		t->wowPhase += t->wowInc;
		if (t->wowPhase > 2.0f * (float)M_PI) t->wowPhase -= 2.0f * (float)M_PI;
		t->flutterPhase += t->flutterInc;
		if (t->flutterPhase > 2.0f * (float)M_PI) t->flutterPhase -= 2.0f * (float)M_PI;

		const float offset = t->base
			+ t->wowDepth * sinf(t->wowPhase)
			+ t->flutterDepth * sinf(t->flutterPhase);

		for (int ch = 0; ch < 2; ch++)
		{
			t->line[ch][t->widx] = dry[ch];

			/* Read at a fractional distance behind the write head. */
			float readPos = (float)t->widx - offset;
			while (readPos < 0.0f) readPos += (float)TAPE_LINE;
			const int i0 = (int)readPos;
			const float frac = readPos - (float)i0;
			const int i1 = (i0 + 1) & (TAPE_LINE - 1);
			float y = t->line[ch][i0] * (1.0f - frac) + t->line[ch][i1] * frac;

			if (t->saturation > 0.0f)
			{
				/* Runs out of headroom gently and never quite limits, which is
				   what separates tape from a clipper. Unity slope at the origin
				   so the control adds harmonics rather than level. */
				const float xd = y * t->drive;
				const float sat = (xd / (1.0f + fabsf(xd))) * inv;
				y = y * (1.0f - t->saturation) + sat * t->saturation;
			}

			y = tapeRun(&t->biasShelf, ch, y);
			y = tapeRun(&t->headBump, ch, y);

			if (t->mix >= 1.0f) (ch ? right : left)[i] = y;
			else (ch ? right : left)[i] = dry[ch] + (y - dry[ch]) * t->mix;
		}

		t->widx = (t->widx + 1) & (TAPE_LINE - 1);
	}
}

void TapeEnable(JamesDSPLib *jdsp)
{
	if (jdsp->tapeEnabled)
		return;
	jdsp_lock(jdsp);
	Tape *t = &jdsp->tape;
	t->fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;
	memset(t->line, 0, sizeof(t->line));
	t->widx = 0;
	t->wowPhase = 0.0f;
	t->flutterPhase = 0.0f;
	t->biasShelf.z1[0] = t->biasShelf.z2[0] = 0.0f;
	t->biasShelf.z1[1] = t->biasShelf.z2[1] = 0.0f;
	t->headBump.z1[0] = t->headBump.z2[0] = 0.0f;
	t->headBump.z1[1] = t->headBump.z2[1] = 0.0f;
	jdsp->tapeEnabled = 1;
	jdsp_unlock(jdsp);
}

void TapeDisable(JamesDSPLib *jdsp)
{
	jdsp_lock(jdsp);
	jdsp->tapeEnabled = 0;
	jdsp_unlock(jdsp);
}
