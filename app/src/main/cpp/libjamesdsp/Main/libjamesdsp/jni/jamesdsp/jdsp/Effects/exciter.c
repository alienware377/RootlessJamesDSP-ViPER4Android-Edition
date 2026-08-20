/*
 * Exciter: harmonics added per frequency range, with a choice of character.
 *
 * The existing bass exciter only works on the bottom and only one way. This
 * splits the spectrum four ways and lets each range be driven with its own
 * amount and its own flavour of distortion, which is what makes it useful for
 * air on top as well as weight underneath.
 *
 * Structured as a sum of deltas rather than a sum of bands:
 *
 *     out = x + sum over k of  amount[k] * (shape(band[k]) - band[k])
 *
 * so with every amount at zero the output is the input with zeroes added to it,
 * which is exact. Summing four reconstructed bands instead would be exact only
 * algebraically - in float it leaves a rounding floor - and this effect has to
 * be genuinely inaudible when it is doing nothing.
 *
 * The band split telescopes: three running lowpasses, the bands being their
 * differences and the remainder, so they always sum back to the input whatever
 * the corner frequencies are.
 *
 * Every shaper has unity slope at the origin, so `amount` is the only thing
 * that sets level and switching character changes flavour rather than loudness.
 * The asymmetric ones generate even harmonics, which is most of what makes
 * valve-ish distortion sound different from transistor-ish distortion - and
 * they also generate DC, so each band's delta runs through a DC blocker before
 * it is added back.
 */
#include <math.h>
#include <string.h>
#include "../jdsp_header.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void exciterDesignLowpass(ExciterStage *s, double f, float fs)
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

static float exciterRun(ExciterStage *s, int ch, float x)
{
	const float y = s->b0 * x + s->z1[ch];
	s->z1[ch] = s->b1 * x - s->a1 * y + s->z2[ch];
	s->z2[ch] = s->b2 * x - s->a2 * y;
	return y;
}

/*
 * The characters.
 *
 * Each is scaled so its slope at zero is one. Without that, "amount" would be
 * a volume control that happens to distort, and switching character would jump
 * the level - which is exactly the mistake the maximiser's character control
 * was found to be making.
 */
static float exciterShape(int character, float x)
{
	switch (character)
	{
	case EXCITER_RETRO:
		/* Odd harmonics, hard edge. Cubic soft clip: the classic transistor
		   sound, third harmonic and little else. */
		if (x > 1.0f) return 0.66666667f;
		if (x < -1.0f) return -0.66666667f;
		return x - (x * x * x) * (1.0f / 3.0f);

	case EXCITER_TAPE:
		/* Odd, but gentler and never quite reaching a limit - the slow
		   compression a tape gives rather than a clip. */
		return x / (1.0f + fabsf(x));

	case EXCITER_TUBE:
		/* Asymmetric: the positive half squashes sooner than the negative one,
		   which is where the second harmonic comes from. It has to branch on
		   the sign to do that. The first attempt here summed two tanh terms of
		   different steepness, which reads as asymmetric but is not - a sum of
		   odd functions is still odd, and it measured 157 dB down on the second
		   harmonic, which is to say none at all. Both halves keep unity slope
		   at the origin because d/dx of tanh(kx)/k is one there whatever k is. */
		if (x >= 0.0f) return tanhf(x * 1.35f) / 1.35f;
		return tanhf(x * 0.8f) / 0.8f;

	case EXCITER_TRIODE:
		/* More lopsided still, and the most even-harmonic of the set. */
		if (x >= 0.0f) return tanhf(x);
		return tanhf(x * 0.55f) / 0.55f;

	case EXCITER_WARM:
	default:
		/* Plain hyperbolic tangent: odd harmonics, softest knee of them all. */
		return tanhf(x);
	}
}

void ExciterSetParam(JamesDSPLib *jdsp, float freq1, float freq2, float freq3,
                     float amount1, float amount2, float amount3, float amount4,
                     int character, float drive, float mixPct)
{
	/* Held for the whole update. Process runs on the audio thread under this
	   same lock, so without it a block could be filtered with half the old
	   coefficients and half the new ones - which for a steep or resonant
	   setting is not a glitch but a burst. The single-threaded harness cannot
	   see this, and the one crash this project has shipped came from exactly
	   this class of problem. */
	jdsp_lock(jdsp);
	Exciter *e = &jdsp->exciter;
	const float fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;

	if (freq2 < freq1) freq2 = freq1;
	if (freq3 < freq2) freq3 = freq2;
	e->freq[0] = freq1; e->freq[1] = freq2; e->freq[2] = freq3;
	for (int i = 0; i < EXCITER_BANDS - 1; i++)
		exciterDesignLowpass(&e->split[i], e->freq[i], fs);

	const float amounts[EXCITER_BANDS] = { amount1, amount2, amount3, amount4 };
	for (int k = 0; k < EXCITER_BANDS; k++)
	{
		float a = amounts[k] * 0.01f;
		if (a < 0.0f) a = 0.0f;
		if (a > 1.0f) a = 1.0f;
		e->amount[k] = a;
	}

	if (character < 0 || character >= EXCITER_CHAR_COUNT) character = EXCITER_WARM;
	e->character = character;

	/* How hard the band is pushed into the shaper before being scaled back.
	   More drive means more harmonics for the same amount of added signal. */
	if (drive < 1.0f) drive = 1.0f;
	if (drive > 20.0f) drive = 20.0f;
	e->drive = drive;

	e->mix = mixPct * 0.01f;
	if (e->mix < 0.0f) e->mix = 0.0f;
	if (e->mix > 1.0f) e->mix = 1.0f;

	/* A DC blocker per band, because the asymmetric characters really do put
	   a step in the signal and four of them would add up. */
	e->dcCoef = 1.0f - (2.0f * (float)M_PI * 8.0f / fs);
	if (e->dcCoef > 0.9999f) e->dcCoef = 0.9999f;

	e->fs = fs;
	e->transparent = 1;
	for (int k = 0; k < EXCITER_BANDS; k++)
		if (e->amount[k] > 1e-6f) e->transparent = 0;
	jdsp_unlock(jdsp);
}

void ExciterProcess(JamesDSPLib *jdsp, size_t n)
{
	Exciter *e = &jdsp->exciter;
	if (e->mix <= 0.0f || e->transparent)
		return;

	float *left = jdsp->tmpBuffer[0];
	float *right = jdsp->tmpBuffer[1];
	const float drive = e->drive;
	const float inv = 1.0f / drive;

	for (size_t i = 0; i < n; i++)
	{
		const float dry[2] = { left[i], right[i] };
		float wet[2] = { dry[0], dry[1] };

		for (int ch = 0; ch < 2; ch++)
		{
			const float x = dry[ch];
			const float l1 = exciterRun(&e->split[0], ch, x);
			const float l2 = exciterRun(&e->split[1], ch, x);
			const float l3 = exciterRun(&e->split[2], ch, x);
			const float band[EXCITER_BANDS] = { l1, l2 - l1, l3 - l2, x - l3 };

			for (int k = 0; k < EXCITER_BANDS; k++)
			{
				if (e->amount[k] <= 0.0f)
					continue;
				/* Drive in, shape, scale back: the shaper's unity slope means
				   this returns the band itself when it is quiet, so the delta
				   below is genuinely just the harmonics. */
				const float shaped = exciterShape(e->character, band[k] * drive) * inv;
				float delta = (shaped - band[k]) * e->amount[k];

				/* Remove the step the asymmetric characters leave behind. */
				ExciterDc *dc = &e->dc[k][ch];
				const float out = delta - dc->x1 + e->dcCoef * dc->y1;
				dc->x1 = delta;
				dc->y1 = out;
				delta = out;

				wet[ch] += delta;
			}
		}

		if (e->mix >= 1.0f)
		{
			left[i] = wet[0];
			right[i] = wet[1];
		}
		else
		{
			left[i] = dry[0] + (wet[0] - dry[0]) * e->mix;
			right[i] = dry[1] + (wet[1] - dry[1]) * e->mix;
		}
	}
}

void ExciterEnable(JamesDSPLib *jdsp)
{
	if (jdsp->exciterEnabled)
		return;
	jdsp_lock(jdsp);
	Exciter *e = &jdsp->exciter;
	e->fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;
	for (int i = 0; i < EXCITER_BANDS - 1; i++)
	{
		e->split[i].z1[0] = e->split[i].z2[0] = 0.0f;
		e->split[i].z1[1] = e->split[i].z2[1] = 0.0f;
	}
	memset(e->dc, 0, sizeof(e->dc));
	jdsp->exciterEnabled = 1;
	jdsp_unlock(jdsp);
}

void ExciterDisable(JamesDSPLib *jdsp)
{
	jdsp_lock(jdsp);
	jdsp->exciterEnabled = 0;
	jdsp_unlock(jdsp);
}
