/*
 * Low end: clean it up, give it weight, take the mud out.
 *
 * Deliberately three things this app could not already do, rather than a fourth
 * copy of things it could. Folding the bass to mono lives in the stereo imager,
 * putting punch back into a kick lives in Impact, and riding a boomy note lives
 * in the dynamic EQ - none of that is repeated here.
 *
 *   Subsonic  A steep highpass below the range anyone can hear. Turntable
 *             rumble, footfalls and mic handling sit down there costing real
 *             headroom and pushing the limiter around, all for content no
 *             speaker will reproduce. Fourth order, because a gentle slope
 *             leaves half of it behind.
 *   Weight    One knob for body. A low shelf, unlike the bass boost, which is
 *             a dynamic processor, and unlike the equaliser, which is a general
 *             tool - this is just "more" or "less".
 *   Mud       A wide dip where recordings pile up and turn thick. Broad on
 *             purpose: narrow enough to be surgical would need the EQ.
 *
 * All three are plain biquads on the full signal, so at neutral settings each
 * is the exact identity and the stage is skipped outright.
 */
#include <math.h>
#include <string.h>
#include "../jdsp_header.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void lowEndIdentity(LowEndStage *s)
{
	s->b0 = 1.0f; s->b1 = 0.0f; s->b2 = 0.0f;
	s->a1 = 0.0f; s->a2 = 0.0f;
}

static float lowEndRun(LowEndStage *s, int ch, float x)
{
	const float y = s->b0 * x + s->z1[ch];
	s->z1[ch] = s->b1 * x - s->a1 * y + s->z2[ch];
	s->z2[ch] = s->b2 * x - s->a2 * y;
	return y;
}

static double lowEndClamp(double f, float fs)
{
	if (f < 10.0) f = 10.0;
	if (f > (double)fs * 0.45) f = (double)fs * 0.45;
	return f;
}

/* Butterworth highpass section. Two of these in cascade with the Q values
   below make a fourth-order slope. */
static void lowEndDesignHighpass(LowEndStage *s, double f, float fs, double q)
{
	const double w0 = 2.0 * M_PI * lowEndClamp(f, fs) / (double)fs;
	const double cw = cos(w0);
	const double alpha = sin(w0) / (2.0 * q);
	const double a0 = 1.0 + alpha;
	s->b0 = (float)(((1.0 + cw) * 0.5) / a0);
	s->b1 = (float)((-(1.0 + cw)) / a0);
	s->b2 = s->b0;
	s->a1 = (float)((-2.0 * cw) / a0);
	s->a2 = (float)((1.0 - alpha) / a0);
}

static void lowEndDesignLowShelf(LowEndStage *s, double f, float fs, float gainDb)
{
	if (fabsf(gainDb) < 1e-6f) { lowEndIdentity(s); return; }
	const double A = pow(10.0, (double)gainDb / 40.0);
	const double w0 = 2.0 * M_PI * lowEndClamp(f, fs) / (double)fs;
	const double cw = cos(w0), sw = sin(w0);
	const double alpha = sw / (2.0 * 0.70710678);
	const double sq = 2.0 * sqrt(A) * alpha;
	const double a0 = (A + 1.0) + (A - 1.0) * cw + sq;
	s->b0 = (float)((A * ((A + 1.0) - (A - 1.0) * cw + sq)) / a0);
	s->b1 = (float)((2.0 * A * ((A - 1.0) - (A + 1.0) * cw)) / a0);
	s->b2 = (float)((A * ((A + 1.0) - (A - 1.0) * cw - sq)) / a0);
	s->a1 = (float)((-2.0 * ((A - 1.0) + (A + 1.0) * cw)) / a0);
	s->a2 = (float)(((A + 1.0) + (A - 1.0) * cw - sq) / a0);
}

static void lowEndDesignPeak(LowEndStage *s, double f, float fs, float gainDb, double q)
{
	if (fabsf(gainDb) < 1e-6f) { lowEndIdentity(s); return; }
	const double A = pow(10.0, (double)gainDb / 40.0);
	const double w0 = 2.0 * M_PI * lowEndClamp(f, fs) / (double)fs;
	const double alpha = sin(w0) / (2.0 * q);
	const double a0 = 1.0 + alpha / A;
	s->b0 = (float)((1.0 + alpha * A) / a0);
	s->b1 = (float)((-2.0 * cos(w0)) / a0);
	s->b2 = (float)((1.0 - alpha * A) / a0);
	s->a1 = (float)((-2.0 * cos(w0)) / a0);
	s->a2 = (float)((1.0 - alpha / A) / a0);
}

void LowEndSetParam(JamesDSPLib *jdsp, float subsonicHz,
                    float weightHz, float weightDb,
                    float mudHz, float mudDb, float mixPct)
{
	LowEnd *l = &jdsp->lowEnd;
	const float fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;

	l->subsonicHz = subsonicHz;
	if (subsonicHz >= 10.0f)
	{
		/* The two Butterworth section Qs for fourth order. */
		lowEndDesignHighpass(&l->sub1, subsonicHz, fs, 0.54119610);
		lowEndDesignHighpass(&l->sub2, subsonicHz, fs, 1.30656296);
	}
	else
	{
		lowEndIdentity(&l->sub1);
		lowEndIdentity(&l->sub2);
	}

	lowEndDesignLowShelf(&l->weight, weightHz, fs, weightDb);
	/* Wide on purpose - a narrow notch here belongs in the equaliser. */
	lowEndDesignPeak(&l->mud, mudHz, fs, mudDb, 0.9);

	l->weightHz = weightHz; l->weightDb = weightDb;
	l->mudHz = mudHz; l->mudDb = mudDb;

	l->mix = mixPct * 0.01f;
	if (l->mix < 0.0f) l->mix = 0.0f;
	if (l->mix > 1.0f) l->mix = 1.0f;
	l->fs = fs;

	l->transparent = subsonicHz < 10.0f &&
		fabsf(weightDb) < 1e-6f && fabsf(mudDb) < 1e-6f;
}

void LowEndProcess(JamesDSPLib *jdsp, size_t n)
{
	LowEnd *l = &jdsp->lowEnd;
	if (l->mix <= 0.0f || l->transparent)
		return;

	float *left = jdsp->tmpBuffer[0];
	float *right = jdsp->tmpBuffer[1];

	for (size_t i = 0; i < n; i++)
	{
		const float dryL = left[i], dryR = right[i];
		float l0 = dryL, r0 = dryR;

		l0 = lowEndRun(&l->sub1, 0, l0);
		l0 = lowEndRun(&l->sub2, 0, l0);
		l0 = lowEndRun(&l->weight, 0, l0);
		l0 = lowEndRun(&l->mud, 0, l0);

		r0 = lowEndRun(&l->sub1, 1, r0);
		r0 = lowEndRun(&l->sub2, 1, r0);
		r0 = lowEndRun(&l->weight, 1, r0);
		r0 = lowEndRun(&l->mud, 1, r0);

		if (l->mix >= 1.0f)
		{
			left[i] = l0;
			right[i] = r0;
		}
		else
		{
			left[i] = dryL + (l0 - dryL) * l->mix;
			right[i] = dryR + (r0 - dryR) * l->mix;
		}
	}
}

void LowEndEnable(JamesDSPLib *jdsp)
{
	if (jdsp->lowEndEnabled)
		return;
	jdsp_lock(jdsp);
	LowEnd *l = &jdsp->lowEnd;
	l->fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;
	LowEndStage *all[4] = { &l->sub1, &l->sub2, &l->weight, &l->mud };
	for (int i = 0; i < 4; i++)
	{
		all[i]->z1[0] = all[i]->z2[0] = 0.0f;
		all[i]->z1[1] = all[i]->z2[1] = 0.0f;
	}
	jdsp->lowEndEnabled = 1;
	jdsp_unlock(jdsp);
}

void LowEndDisable(JamesDSPLib *jdsp)
{
	jdsp_lock(jdsp);
	jdsp->lowEndEnabled = 0;
	jdsp_unlock(jdsp);
}
