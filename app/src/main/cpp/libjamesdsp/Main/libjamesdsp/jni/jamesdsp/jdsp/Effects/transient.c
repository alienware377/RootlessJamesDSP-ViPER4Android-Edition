/*
 * Impact: attack and sustain shaping, per frequency range.
 *
 * A compressor asks "how loud is it"; this asks "is it getting louder right
 * now". Two envelope followers chase the same signal at different speeds, and
 * the gap between them says which part of a note you are in. During an onset
 * the fast one has already risen while the slow one is still climbing, so their
 * difference is positive - that is the attack. During the decay a slow-release
 * follower is still up while a fast-release one has dropped, and that gap is
 * the sustain. Multiply each by how much the user wants and you can put the
 * snap back into a compressed drum, or take the room out of a boomy one,
 * without touching steady tones at all - a held note has no gap between its
 * envelopes, so it comes through with no gain change whatsoever.
 *
 * Three ranges, so the punch of a kick can be lifted without also sharpening
 * the cymbals. The split is telescoping - two running lowpasses, the bands
 * being their differences and the remainder - so the three always sum back to
 * the input. The bands are not perfectly isolated (each carries a phase-shifted
 * residue of its neighbour), which for a folded-to-mono test would matter and
 * did; here it only means a band's shaping bleeds gently into the next, which
 * is how nearly every multiband processor behaves and is musically harmless.
 *
 * The detector listens to the mono sum of each band. Detecting per channel
 * would let the two sides be shaped by different amounts, which smears the
 * stereo image on exactly the transients this is meant to sharpen.
 */
#include <math.h>
#include <string.h>
#include "../jdsp_header.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Fixed detector speeds. These are what make the effect an attack/sustain
   shaper rather than a compressor, so they are not user-facing: exposing them
   only offers ways to stop it working. */
#define TRANS_ATK_FAST_MS    1.0f
#define TRANS_ATK_SLOW_MS   22.0f
#define TRANS_ATK_REL_MS    45.0f
#define TRANS_SUS_ATT_MS     6.0f
#define TRANS_SUS_FAST_MS   70.0f
#define TRANS_SUS_SLOW_MS  650.0f
/* A rectified sine ripples, and the fast follower rides that ripple slightly
   higher than the slow one does, so a held note shows a small permanent gap
   that is not a transient at all. Measured at just under 1 dB on a 110 Hz tone.
   Ignoring the first 2.5 dB of any gap removes it with margin to spare: a real onset opens a gap of
   ten or more. Without this the effect behaves like a very slow compressor on
   sustained material, which is precisely what it is meant not to be. */
#define TRANS_DEADZONE_DB    2.5f

static float transCoef(float ms, float fs)
{
	if (ms <= 0.0f) return 0.0f;
	return expf(-1.0f / ((ms * 0.001f) * fs));
}

static void transDesignLowpass(TransientStage *s, double f, float fs)
{
	if (f < 20.0) f = 20.0;
	if (f > (double)fs * 0.45) f = (double)fs * 0.45;
	const double w0 = 2.0 * M_PI * f / (double)fs;
	const double cw = cos(w0);
	const double alpha = sin(w0) / (2.0 * 0.70710678);
	const double a0 = 1.0 + alpha;
	s->b0 = (float)(((1.0 - cw) * 0.5) / a0);
	s->b1 = (float)((1.0 - cw) / a0);
	s->b2 = s->b0;
	s->a1 = (float)((-2.0 * cw) / a0);
	s->a2 = (float)((1.0 - alpha) / a0);
}

static float transRun(TransientStage *s, int ch, float x)
{
	const float y = s->b0 * x + s->z1[ch];
	s->z1[ch] = s->b1 * x - s->a1 * y + s->z2[ch];
	s->z2[ch] = s->b2 * x - s->a2 * y;
	return y;
}

/* One-pole follower: rise on attack, fall on release. */
static float transFollow(float env, float mag, float attC, float relC)
{
	const float c = mag > env ? attC : relC;
	return mag + c * (env - mag);
}

void TransientSetParam(JamesDSPLib *jdsp, float freqLow, float freqHigh,
                       float attackLow, float sustainLow,
                       float attackMid, float sustainMid,
                       float attackHigh, float sustainHigh,
                       float rangeDb, float mixPct)
{
	/* Held for the whole update. Process runs on the audio thread under this
	   same lock, so without it a block could be filtered with half the old
	   coefficients and half the new ones - which for a steep or resonant
	   setting is not a glitch but a burst. The single-threaded harness cannot
	   see this, and the one crash this project has shipped came from exactly
	   this class of problem. */
	jdsp_lock(jdsp);
	Transient *t = &jdsp->transient;
	const float fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;

	if (freqHigh < freqLow) freqHigh = freqLow;
	t->freqLow = freqLow;
	t->freqHigh = freqHigh;
	transDesignLowpass(&t->split[0], freqLow, fs);
	transDesignLowpass(&t->split[1], freqHigh, fs);

	/* The controls arrive as percentages either side of zero. */
	t->band[0].attack = attackLow * 0.01f;
	t->band[0].sustain = sustainLow * 0.01f;
	t->band[1].attack = attackMid * 0.01f;
	t->band[1].sustain = sustainMid * 0.01f;
	t->band[2].attack = attackHigh * 0.01f;
	t->band[2].sustain = sustainHigh * 0.01f;

	t->rangeDb = rangeDb < 0.0f ? 0.0f : (rangeDb > 24.0f ? 24.0f : rangeDb);

	const float atkFast = transCoef(TRANS_ATK_FAST_MS, fs);
	const float atkSlow = transCoef(TRANS_ATK_SLOW_MS, fs);
	const float atkRel = transCoef(TRANS_ATK_REL_MS, fs);
	const float susAtt = transCoef(TRANS_SUS_ATT_MS, fs);
	const float susFast = transCoef(TRANS_SUS_FAST_MS, fs);
	const float susSlow = transCoef(TRANS_SUS_SLOW_MS, fs);
	for (int k = 0; k < TRANSIENT_BANDS; k++)
	{
		TransientBand *b = &t->band[k];
		b->atkFastC = atkFast; b->atkSlowC = atkSlow; b->atkRelC = atkRel;
		b->susAttC = susAtt; b->susFastC = susFast; b->susSlowC = susSlow;
	}

	t->mix = mixPct * 0.01f;
	if (t->mix < 0.0f) t->mix = 0.0f;
	if (t->mix > 1.0f) t->mix = 1.0f;
	t->fs = fs;

	/* Nothing asked for means nothing done - and it has to be a real early
	   return, because splitting into three bands and adding them back does not
	   reproduce the input in float arithmetic even when no gain is applied. */
	t->transparent = t->rangeDb <= 0.0f;
	if (!t->transparent)
	{
		t->transparent = 1;
		for (int k = 0; k < TRANSIENT_BANDS; k++)
			if (fabsf(t->band[k].attack) > 1e-6f || fabsf(t->band[k].sustain) > 1e-6f)
				t->transparent = 0;
	}
	jdsp_unlock(jdsp);
}

void TransientProcess(JamesDSPLib *jdsp, size_t n)
{
	Transient *t = &jdsp->transient;
	if (t->mix <= 0.0f || t->transparent)
		return;

	float *left = jdsp->tmpBuffer[0];
	float *right = jdsp->tmpBuffer[1];

	for (size_t i = 0; i < n; i++)
	{
		const float dryL = left[i], dryR = right[i];

		const float lo_l = transRun(&t->split[0], 0, dryL);
		const float hi_l = transRun(&t->split[1], 0, dryL);
		const float lo_r = transRun(&t->split[0], 1, dryR);
		const float hi_r = transRun(&t->split[1], 1, dryR);

		const float bandL[TRANSIENT_BANDS] = { lo_l, hi_l - lo_l, dryL - hi_l };
		const float bandR[TRANSIENT_BANDS] = { lo_r, hi_r - lo_r, dryR - hi_r };

		float wetL = 0.0f, wetR = 0.0f;
		for (int k = 0; k < TRANSIENT_BANDS; k++)
		{
			TransientBand *b = &t->band[k];
			const float mono = fabsf((bandL[k] + bandR[k]) * 0.5f);

			b->envAtkFast = transFollow(b->envAtkFast, mono, b->atkFastC, b->atkRelC);
			b->envAtkSlow = transFollow(b->envAtkSlow, mono, b->atkSlowC, b->atkRelC);
			b->envSusFast = transFollow(b->envSusFast, mono, b->susAttC, b->susFastC);
			b->envSusSlow = transFollow(b->envSusSlow, mono, b->susAttC, b->susSlowC);

			float gainDb = 0.0f;
			if (b->attack != 0.0f)
			{
				/* Positive only while the fast follower is ahead, which is the
				   definition of an onset. A held tone has both followers at the
				   same level and contributes nothing. */
				const float fast = b->envAtkFast > 1e-7f ? b->envAtkFast : 1e-7f;
				const float slow = b->envAtkSlow > 1e-7f ? b->envAtkSlow : 1e-7f;
				const float d = 20.0f * log10f(fast / slow) - TRANS_DEADZONE_DB;
				if (d > 0.0f) gainDb += b->attack * d;
			}
			if (b->sustain != 0.0f)
			{
				/* And positive only while the slow-release follower is still up
				   after the fast one has dropped - the tail of a note. */
				const float fast = b->envSusFast > 1e-7f ? b->envSusFast : 1e-7f;
				const float slow = b->envSusSlow > 1e-7f ? b->envSusSlow : 1e-7f;
				const float d = 20.0f * log10f(slow / fast) - TRANS_DEADZONE_DB;
				if (d > 0.0f) gainDb += b->sustain * d;
			}

			if (gainDb > t->rangeDb) gainDb = t->rangeDb;
			if (gainDb < -t->rangeDb) gainDb = -t->rangeDb;
			b->gainDb = gainDb;

			const float g = gainDb == 0.0f ? 1.0f : powf(10.0f, gainDb * 0.05f);
			wetL += bandL[k] * g;
			wetR += bandR[k] * g;
		}

		if (t->mix >= 1.0f)
		{
			left[i] = wetL;
			right[i] = wetR;
		}
		else
		{
			left[i] = dryL + (wetL - dryL) * t->mix;
			right[i] = dryR + (wetR - dryR) * t->mix;
		}
	}
}

void TransientEnable(JamesDSPLib *jdsp)
{
	if (jdsp->transientEnabled)
		return;
	jdsp_lock(jdsp);
	Transient *t = &jdsp->transient;
	t->fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;
	for (int i = 0; i < TRANSIENT_BANDS - 1; i++)
	{
		t->split[i].z1[0] = t->split[i].z2[0] = 0.0f;
		t->split[i].z1[1] = t->split[i].z2[1] = 0.0f;
	}
	for (int k = 0; k < TRANSIENT_BANDS; k++)
	{
		TransientBand *b = &t->band[k];
		b->envAtkFast = b->envAtkSlow = 0.0f;
		b->envSusFast = b->envSusSlow = 0.0f;
		b->gainDb = 0.0f;
	}
	jdsp->transientEnabled = 1;
	jdsp_unlock(jdsp);
}

void TransientDisable(JamesDSPLib *jdsp)
{
	jdsp_lock(jdsp);
	jdsp->transientEnabled = 0;
	jdsp_unlock(jdsp);
}
