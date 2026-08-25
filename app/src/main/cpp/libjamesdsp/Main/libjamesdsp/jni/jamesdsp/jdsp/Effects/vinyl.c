/*
 * Vintage vinyl: the noises a record makes, as separate controls.
 *
 * Vintage tape next door models a machine that is working, badly. This models
 * a disc that is worn, and the two are different failures - tape drifts in
 * pitch and runs out of headroom, vinyl adds things that were never in the
 * recording. So they are separate cards, and both can run at once.
 *
 * Everything here is a NOISE SOURCE mixed alongside the music rather than a
 * filter applied to it, with one exception (wear). That has a consequence
 * worth stating plainly: at every control zero this stage is the exact
 * identity, and it early-returns rather than adding silence, because adding
 * float zeroes to a signal is only approximately harmless.
 *
 *   Surface     The continuous bed a record sits on. Pink-ish noise, band
 *               limited - a groove cannot reproduce much above 10 kHz or
 *               below the turntable's own rumble.
 *   Crackle     Sparse, small, dull impulses. What worn vinyl does constantly.
 *               Density and size are separate: a lot of small crackle is a
 *               tired record, a little large crackle is a damaged one.
 *   Pops        Rare and much louder, from a scratch or a speck of grit.
 *               Given their own decay because a pop rings the stylus rather
 *               than just displacing it.
 *   Clicks      Very short and bright - the click of a dust particle rather
 *               than the thump of a scratch. Almost a single sample, so it
 *               reads as an edge instead of an event.
 *   Sizzle      Dense high-frequency frying, the sound of a record played wet
 *               or worn smooth. Effectively crackle at a rate high enough to
 *               stop being individual events.
 *   Hiss        Flat, quiet, always there. The noise floor of the cutting
 *               chain rather than the disc.
 *   Prickle     Sharp, thin, upper-mid ticks - the "prickly" quality of a
 *               dirty stylus. Bandpassed rather than bright, which is what
 *               separates it from clicks.
 *   Rumble      Below everything: the turntable's motor and bearing, felt
 *               more than heard. Lowpassed noise, deliberately very low.
 *   Wear        The one control that touches the music: high end lost to a
 *               worn groove. A gentle shelf, not a brick wall.
 *
 * All the noise sources share one generator and one filter set per source
 * rather than each rolling its own, so the bed sounds like one record rather
 * than nine unrelated noises layered up.
 *
 * `Follow` deserves its own note. Real vinyl noise is constant, and constant
 * is what you want while music plays - but a silent passage full of crackle is
 * obviously synthetic, and worse, it never stops when the music does. So the
 * noise can be made to track the programme level. At zero the bed is constant
 * and honest; turned up it fades with the music, which is a lie but a useful
 * one for a phone that pauses.
 */
#include <math.h>
#include <string.h>
#include "../jdsp_header.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* One stream of uniform noise, shared by every source. */
static inline float vinylWhite(Vinyl *v)
{
	v->rng = v->rng * 1664525u + 1013904223u;
	return (float)((int)(v->rng >> 8) & 0xFFFF) * (1.0f / 32768.0f) - 1.0f;
}

/* Uniform in [0,1), for deciding whether an event happens this sample. */
static inline float vinylChance(Vinyl *v)
{
	v->rng = v->rng * 1664525u + 1013904223u;
	return (float)(v->rng >> 8) * (1.0f / 16777216.0f);
}

static void vinylDesignLowpass(VinylStage *s, double f, float fs)
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

static void vinylDesignHighpass(VinylStage *s, double f, float fs)
{
	if (f < 5.0) f = 5.0;
	if (f > (double)fs * 0.45) f = (double)fs * 0.45;
	const double w0 = 2.0 * M_PI * f / (double)fs;
	const double cw = cos(w0);
	const double alpha = sin(w0) / (2.0 * 0.70710678);
	const double a0 = 1.0 + alpha;
	s->b0 = (float)(((1.0 + cw) * 0.5) / a0);
	s->b1 = (float)((-(1.0 + cw)) / a0);
	s->b2 = s->b0;
	s->a1 = (float)((-2.0 * cw) / a0);
	s->a2 = (float)((1.0 - alpha) / a0);
}

static void vinylDesignBandpass(VinylStage *s, double f, float fs, double q)
{
	if (f < 20.0) f = 20.0;
	if (f > (double)fs * 0.45) f = (double)fs * 0.45;
	const double w0 = 2.0 * M_PI * f / (double)fs;
	const double alpha = sin(w0) / (2.0 * q);
	const double a0 = 1.0 + alpha;
	s->b0 = (float)(alpha / a0);
	s->b1 = 0.0f;
	s->b2 = (float)(-alpha / a0);
	s->a1 = (float)((-2.0 * cos(w0)) / a0);
	s->a2 = (float)((1.0 - alpha) / a0);
}

static void vinylDesignHighShelf(VinylStage *s, double f, float fs, float gainDb)
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
	const double a0 = (A + 1.0) - (A - 1.0) * cw + sq;
	s->b0 = (float)((A * ((A + 1.0) + (A - 1.0) * cw + sq)) / a0);
	s->b1 = (float)((-2.0 * A * ((A - 1.0) + (A + 1.0) * cw)) / a0);
	s->b2 = (float)((A * ((A + 1.0) + (A - 1.0) * cw - sq)) / a0);
	s->a1 = (float)((2.0 * ((A - 1.0) - (A + 1.0) * cw)) / a0);
	s->a2 = (float)(((A + 1.0) - (A - 1.0) * cw - sq) / a0);
}

static float vinylRun(VinylStage *s, int ch, float x)
{
	const float y = s->b0 * x + s->z1[ch];
	s->z1[ch] = s->b1 * x - s->a1 * y + s->z2[ch];
	s->z2[ch] = s->b2 * x - s->a2 * y;
	return y;
}

/* Rate-dependent design, kept apart so a sample rate change can redo it from
   the stored settings. Same reasoning as LowEndRefresh. */
static void vinylDesign(Vinyl *v, float fs)
{
	vinylDesignLowpass(&v->surfaceLp, 9000.0, fs);
	vinylDesignHighpass(&v->surfaceHp, 300.0, fs);
	vinylDesignLowpass(&v->crackleLp, 4500.0, fs);
	vinylDesignBandpass(&v->prickleBp, 4200.0, fs, 2.2);
	vinylDesignLowpass(&v->rumbleLp, 45.0, fs);
	vinylDesignLowpass(&v->hissLp, 12000.0, fs);
	vinylDesignHighShelf(&v->wearShelf, 5000.0, fs, -v->wearDb);

	/* Event decays, in samples. A pop rings for a few milliseconds; a click is
	   over almost immediately, which is what makes it read as an edge. */
	v->popDecay = expf(-1.0f / (0.0040f * fs));
	v->crackleDecay = expf(-1.0f / (0.0012f * fs));
	v->clickDecay = expf(-1.0f / (0.00012f * fs));
	v->prickleDecay = expf(-1.0f / (0.0008f * fs));

	/* Programme follower: quick to open so the bed arrives with the music,
	   slow to close so it does not pump between notes. */
	v->followAtt = expf(-1.0f / (0.010f * fs));
	v->followRel = expf(-1.0f / (0.400f * fs));
	v->fs = fs;
}

void VinylRefresh(JamesDSPLib *jdsp)
{
	vinylDesign(&jdsp->vinyl, jdsp->fs > 0.0f ? jdsp->fs : 48000.0f);
}

static float vinylPct(float v)
{
	if (v < 0.0f) return 0.0f;
	if (v > 100.0f) return 100.0f;
	return v * 0.01f;
}

void VinylSetParam(JamesDSPLib *jdsp, float surfacePct, float cracklePct,
                   float crackleSizePct, float popsPct, float clicksPct,
                   float sizzlePct, float hissPct, float pricklePct,
                   float rumblePct, float wearDb, float followPct, float mixPct)
{
	/* Held for the whole update: Process runs on the audio thread under this
	   same lock, and half-updated filter coefficients are a burst rather than a
	   glitch. See the note in tape.c. */
	jdsp_lock(jdsp);
	Vinyl *v = &jdsp->vinyl;
	const float fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;

	v->surface = vinylPct(surfacePct);
	v->crackle = vinylPct(cracklePct);
	v->crackleSize = vinylPct(crackleSizePct);
	v->pops = vinylPct(popsPct);
	v->clicks = vinylPct(clicksPct);
	v->sizzle = vinylPct(sizzlePct);
	v->hiss = vinylPct(hissPct);
	v->prickle = vinylPct(pricklePct);
	v->rumble = vinylPct(rumblePct);
	v->follow = vinylPct(followPct);

	if (wearDb < 0.0f) wearDb = 0.0f;
	if (wearDb > 24.0f) wearDb = 24.0f;
	v->wearDb = wearDb;

	v->mix = vinylPct(mixPct);

	/* Event probabilities per sample. Scaled by the rate so a record crackles
	   at the same speed however fast it is being sampled. */
	v->crackleRate = v->crackle * 900.0f / fs;
	v->popRate = v->pops * 3.0f / fs;
	v->clickRate = v->clicks * 60.0f / fs;
	v->sizzleRate = v->sizzle * 7000.0f / fs;
	v->prickleRate = v->prickle * 220.0f / fs;

	vinylDesign(v, fs);

	if (v->rng == 0u) v->rng = 22222u;

	/* Nothing asked for means nothing done, and it has to be a real early
	   return: mixing in a bed of zeroes is not bit-identical once a mix control
	   has multiplied it. */
	v->transparent = v->surface <= 0.0f && v->crackle <= 0.0f &&
		v->pops <= 0.0f && v->clicks <= 0.0f && v->sizzle <= 0.0f &&
		v->hiss <= 0.0f && v->prickle <= 0.0f && v->rumble <= 0.0f &&
		v->wearDb <= 0.0f;
	jdsp_unlock(jdsp);
}

void VinylProcess(JamesDSPLib *jdsp, size_t n)
{
	Vinyl *v = &jdsp->vinyl;
	if (v->mix <= 0.0f || v->transparent)
		return;

	float *left = jdsp->tmpBuffer[0];
	float *right = jdsp->tmpBuffer[1];

	for (size_t i = 0; i < n; i++)
	{
		const float dry[2] = { left[i], right[i] };

		/* One follower on the summed programme, so both channels duck together
		   and the bed does not wander across the image. */
		float gate = 1.0f;
		if (v->follow > 0.0f)
		{
			const float mag = fabsf(dry[0] + dry[1]) * 0.5f;
			const float c = mag > v->followEnv ? v->followAtt : v->followRel;
			v->followEnv = mag + c * (v->followEnv - mag);
			/* Fully up at -40 dB and above; the control sets how much of that
			   tracking is applied rather than switching it on and off. */
			float lvl = v->followEnv * 100.0f;
			if (lvl > 1.0f) lvl = 1.0f;
			gate = 1.0f - v->follow + v->follow * lvl;
		}

		/* --- events -------------------------------------------------------
		   Each is an impulse dropped into its own decaying store. Retriggering
		   overwrites rather than adds, so a dense setting cannot pile up into
		   a continuous tone. */
		if (v->crackleRate > 0.0f && vinylChance(v) < v->crackleRate)
			v->crackleEnv = vinylWhite(v) * (0.05f + v->crackleSize * 0.55f);
		if (v->popRate > 0.0f && vinylChance(v) < v->popRate)
			v->popEnv = vinylWhite(v) * 0.9f;
		if (v->clickRate > 0.0f && vinylChance(v) < v->clickRate)
			v->clickEnv = vinylWhite(v) * 0.6f;
		if (v->sizzleRate > 0.0f && vinylChance(v) < v->sizzleRate)
			v->sizzleEnv = vinylWhite(v) * 0.18f;
		if (v->prickleRate > 0.0f && vinylChance(v) < v->prickleRate)
			v->prickleEnv = vinylWhite(v) * 0.5f;

		v->crackleEnv *= v->crackleDecay;
		v->popEnv *= v->popDecay;
		v->clickEnv *= v->clickDecay;
		v->sizzleEnv *= v->clickDecay;
		v->prickleEnv *= v->prickleDecay;

		for (int ch = 0; ch < 2; ch++)
		{
			float noise = 0.0f;

			if (v->surface > 0.0f)
			{
				/* Band limited both ways: a groove reproduces neither the top
				   nor the very bottom, and unfiltered white noise over music
				   sounds like a broken tweeter rather than a record. */
				float s = vinylWhite(v);
				s = vinylRun(&v->surfaceLp, ch, s);
				s = vinylRun(&v->surfaceHp, ch, s);
				noise += s * v->surface * 0.06f;
			}
			if (v->hiss > 0.0f)
				noise += vinylRun(&v->hissLp, ch, vinylWhite(v)) * v->hiss * 0.03f;
			if (v->rumble > 0.0f)
				noise += vinylRun(&v->rumbleLp, ch, vinylWhite(v)) * v->rumble * 0.5f;

			/* The dull events share one lowpass so they sit in the same place
			   as each other; clicks and prickle bypass it, which is what makes
			   them sound like a different kind of fault. */
			float dull = v->crackleEnv + v->popEnv + v->sizzleEnv;
			if (dull != 0.0f)
				noise += vinylRun(&v->crackleLp, ch, dull);
			noise += v->clickEnv;
			if (v->prickleEnv != 0.0f)
				noise += vinylRun(&v->prickleBp, ch, v->prickleEnv);

			/* Wear is the only part that touches the music itself. */
			float wet = dry[ch];
			if (v->wearDb > 0.0f)
				wet = vinylRun(&v->wearShelf, ch, wet);
			wet += noise * gate;

			if (v->mix >= 1.0f) (ch ? right : left)[i] = wet;
			else (ch ? right : left)[i] = dry[ch] + (wet - dry[ch]) * v->mix;
		}
	}
}

void VinylEnable(JamesDSPLib *jdsp)
{
	jdsp_lock(jdsp);
	if (jdsp->vinylEnabled)
	{
		jdsp_unlock(jdsp);
		return;
	}
	Vinyl *v = &jdsp->vinyl;
	v->fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;
	VinylStage *all[7] = { &v->surfaceLp, &v->surfaceHp, &v->crackleLp,
		&v->prickleBp, &v->rumbleLp, &v->hissLp, &v->wearShelf };
	for (int i = 0; i < 7; i++)
	{
		all[i]->z1[0] = all[i]->z2[0] = 0.0f;
		all[i]->z1[1] = all[i]->z2[1] = 0.0f;
	}
	v->crackleEnv = v->popEnv = v->clickEnv = v->sizzleEnv = v->prickleEnv = 0.0f;
	v->followEnv = 0.0f;
	if (v->rng == 0u) v->rng = 22222u;
	jdsp->vinylEnabled = 1;
	jdsp_unlock(jdsp);
}

void VinylDisable(JamesDSPLib *jdsp)
{
	jdsp_lock(jdsp);
	jdsp->vinylEnabled = 0;
	jdsp_unlock(jdsp);
}
