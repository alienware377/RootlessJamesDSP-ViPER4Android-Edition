/*
 * Dynamic equaliser: bands that only act when the signal asks them to.
 *
 * A static EQ cut is always there, so taming a boom that happens on four notes
 * costs body on everything else. Each band here watches its own slice of the
 * spectrum through a bandpass sidechain, and moves a peaking filter only while
 * that slice is over (or under) a threshold. At rest the filter sits at 0 dB,
 * where a peaking biquad is exactly the identity - b equals a term for term -
 * so an idle band is bit-transparent rather than merely quiet.
 *
 * The sidechain is the mono sum. Detecting per channel would let the two sides
 * duck by different amounts, which moves the stereo image on every transient.
 *
 * Coefficients are recomputed on a schedule rather than per sample: a peaking
 * biquad costs a handful of transcendentals to design and the gain cannot move
 * meaningfully within a few samples anyway. The interval is short enough that
 * the steps land far below the noise floor, and the gain that drives it is
 * already smoothed by the follower.
 */
#include <math.h>
#include <string.h>
#include "../jdsp_header.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* How often the peaking coefficients are redesigned, in samples. 32 is 0.67 ms
   at 48 kHz - far shorter than the fastest useful attack. */
#define DYNEQ_REDESIGN 32
/* Below this the gain is treated as no change at all, which keeps an idle band
   on the identity path. */
#define DYNEQ_EPS_DB 0.02f

static float dynEqDb2Lin(float db) { return powf(10.0f, db * 0.05f); }

/*
 * Sidechain bandpass, constant 0 dB at the centre.
 *
 * The peak-gain form rather than the constant-skirt one: the threshold is meant
 * to mean "this band reached this level", which only holds if a tone at the
 * centre frequency passes through at unity.
 */
static void dynEqDesignSidechain(DynEqBand *b, float fs)
{
	const double w0 = 2.0 * M_PI * (double)b->freq / (double)fs;
	const double q = b->q < 0.05f ? 0.05 : (double)b->q;
	const double alpha = sin(w0) / (2.0 * q);
	const double a0 = 1.0 + alpha;
	b->sb0 = (float)(alpha / a0);
	b->sb1 = 0.0f;
	b->sb2 = (float)(-alpha / a0);
	b->sa1 = (float)((-2.0 * cos(w0)) / a0);
	b->sa2 = (float)((1.0 - alpha) / a0);
}

/* The band the listener hears: an RBJ peaking filter at the current gain. */
static void dynEqDesignPeaking(DynEqBand *b, float fs, float gainDb)
{
	const double A = pow(10.0, (double)gainDb / 40.0);
	const double w0 = 2.0 * M_PI * (double)b->freq / (double)fs;
	const double q = b->q < 0.05f ? 0.05 : (double)b->q;
	const double alpha = sin(w0) / (2.0 * q);
	const double a0 = 1.0 + alpha / A;
	b->b0 = (float)((1.0 + alpha * A) / a0);
	b->b1 = (float)((-2.0 * cos(w0)) / a0);
	b->b2 = (float)((1.0 - alpha * A) / a0);
	b->a1 = (float)((-2.0 * cos(w0)) / a0);
	b->a2 = (float)((1.0 - alpha / A) / a0);
}

static float dynEqRunBiquad(float x, float b0, float b1, float b2,
                            float a1, float a2, float *z1, float *z2)
{
	const float y = b0 * x + *z1;
	*z1 = b1 * x - a1 * y + *z2;
	*z2 = b2 * x - a2 * y;
	return y;
}

/* One-pole coefficient for a time constant in milliseconds. */
static float dynEqCoef(float ms, float fs)
{
	if (ms <= 0.0f) return 0.0f;
	return expf(-1.0f / ((ms * 0.001f) * fs));
}

static void dynEqRefreshBand(DynEqBand *b, float fs)
{
	if (b->freq < 20.0f) b->freq = 20.0f;
	if (b->freq > fs * 0.45f) b->freq = fs * 0.45f;
	dynEqDesignSidechain(b, fs);
	dynEqDesignPeaking(b, fs, b->appliedDb);
	b->attC = dynEqCoef(b->attackMs, fs);
	b->relC = dynEqCoef(b->releaseMs, fs);
}

/* Redesign every band at whatever rate the engine is now running. Unlocked on
   purpose - the only caller is JamesDSPSetSampleRate, which already holds the
   lock, and jdsp_lock is not recursive. See LowEndRefresh for the reasoning.

   dynEqRefreshBand already redesigns the peaking filter at the gain currently
   applied rather than at zero, so a band that happens to be ducking when the
   rate changes keeps ducking by the same amount instead of jumping back to
   flat. The follower state is left alone for the same reason. */
void DynamicEqRefresh(JamesDSPLib *jdsp)
{
	DynamicEq *d = &jdsp->dynamicEq;
	const float fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;
	for (int i = 0; i < d->numBands; i++)
		dynEqRefreshBand(&d->band[i], fs);
	d->fs = fs;
}

void DynamicEqSetBands(JamesDSPLib *jdsp, const float *bands, int count)
{
	/* Same reasoning as the lock in DynamicEqSetParam below: Process runs on the
	   audio thread under this lock, and this function rewrites both filters of
	   every band. It was missed when the others were locked because the name
	   does not end in SetParam - and it is the worst one to leave open, since
	   changing numBands mid-block would have Process walk bands whose
	   coefficients are half written. */
	jdsp_lock(jdsp);
	DynamicEq *d = &jdsp->dynamicEq;
	const float fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;
	if (count < 0) count = 0;
	if (count > DYNEQ_MAX_BANDS) count = DYNEQ_MAX_BANDS;

	const int previous = d->numBands;
	for (int i = 0; i < count; i++)
	{
		const float *v = bands + i * DYNEQ_VALUES_PER_BAND;
		DynEqBand *b = &d->band[i];
		b->freq = v[0];
		b->q = v[1];
		b->thresholdDb = v[2];
		b->ratio = v[3] < 1.0f ? 1.0f : v[3];
		b->attackMs = v[4];
		b->releaseMs = v[5];
		b->rangeDb = v[6];
		b->mode = (int)v[7];
		dynEqRefreshBand(b, fs);
	}
	/* Only the newly occupied slots start from silence. Clearing every band
	   would click whenever a band is added while audio is running. */
	for (int i = previous; i < count; i++)
	{
		DynEqBand *b = &d->band[i];
		b->env = 0.0f;
		b->gainDb = b->appliedDb = 0.0f;
		b->sz1[0] = b->sz2[0] = b->sz1[1] = b->sz2[1] = 0.0f;
		b->z1[0] = b->z2[0] = b->z1[1] = b->z2[1] = 0.0f;
		dynEqDesignPeaking(b, fs, 0.0f);
	}
	d->numBands = count;
	d->fs = fs;
	jdsp_unlock(jdsp);
}

void DynamicEqSetParam(JamesDSPLib *jdsp, float mixPct, int msMode)
{
	/* Held for the whole update. Process runs on the audio thread under this
	   same lock, so without it a block could be filtered with half the old
	   coefficients and half the new ones - which for a steep or resonant
	   setting is not a glitch but a burst. The single-threaded harness cannot
	   see this, and the one crash this project has shipped came from exactly
	   this class of problem. */
	jdsp_lock(jdsp);
	DynamicEq *d = &jdsp->dynamicEq;
	d->mix = mixPct * 0.01f;
	if (d->mix < 0.0f) d->mix = 0.0f;
	if (d->mix > 1.0f) d->mix = 1.0f;
	if (msMode < 0 || msMode >= MS_MODE_COUNT) msMode = MS_MODE_STEREO;
	d->msMode = msMode;
	jdsp_unlock(jdsp);
}

void DynamicEqProcess(JamesDSPLib *jdsp, size_t n)
{
	DynamicEq *d = &jdsp->dynamicEq;
	if (!d->numBands || d->mix <= 0.0f)
		return;

	float *left = jdsp->tmpBuffer[0];
	float *right = jdsp->tmpBuffer[1];
	const float fs = d->fs > 0.0f ? d->fs : 48000.0f;

	for (size_t i = 0; i < n; i++)
	{
		const float dryL = left[i], dryR = right[i];
		float l, r, mono;

		if (d->msMode == MS_MODE_STEREO)
		{
			l = dryL; r = dryR;
			/* Sidechain from the sum, so both channels always move together. */
			mono = (dryL + dryR) * 0.5f;
		}
		else
		{
			/* Working on one half of the stereo picture: the chosen part goes
			   through the filters and is its own sidechain, the other is held
			   aside untouched and put back afterwards. Treating the centre and
			   the edges separately is what lets a de-esser catch a vocal
			   without also dulling the reverb around it. */
			const float mid = (dryL + dryR) * 0.5f;
			const float side = (dryL - dryR) * 0.5f;
			if (d->msMode == MS_MODE_MID) { l = mid; r = side; }
			else                          { l = side; r = mid; }
			mono = l;
		}

		for (int k = 0; k < d->numBands; k++)
		{
			DynEqBand *b = &d->band[k];
			if (b->rangeDb == 0.0f)
				continue;

			const float sc = dynEqRunBiquad(mono, b->sb0, b->sb1, b->sb2,
			                                b->sa1, b->sa2, &b->sz1[0], &b->sz2[0]);
			const float mag = fabsf(sc);
			/* Attack when rising, release when falling - a single coefficient
			   would make a fast band chatter and a slow one miss transients. */
			const float c = mag > b->env ? b->attC : b->relC;
			b->env = mag + c * (b->env - mag);

			/* -120 dB floor: silence would otherwise send the log to -inf and
			   an expander band to its full boost on a digital black passage. */
			const float envDb = 20.0f * log10f(b->env > 1e-6f ? b->env : 1e-6f);
			float target = 0.0f;
			const float over = envDb - b->thresholdDb;
			if (b->mode == DYNEQ_MODE_COMPRESS)
			{
				/* Cut once it climbs past the threshold. */
				if (over > 0.0f)
					target = -over * (1.0f - 1.0f / b->ratio);
			}
			else
			{
				/* Lift what falls below it. */
				if (over < 0.0f)
					target = -over * (1.0f - 1.0f / b->ratio);
			}
			/* rangeDb carries the sign the user asked for, so clamp toward it
			   rather than assuming a direction. */
			if (b->rangeDb < 0.0f)
			{
				if (target < b->rangeDb) target = b->rangeDb;
				if (target > 0.0f) target = 0.0f;
			}
			else
			{
				if (target > b->rangeDb) target = b->rangeDb;
				if (target < 0.0f) target = 0.0f;
			}
			b->gainDb = target;

			l = dynEqRunBiquad(l, b->b0, b->b1, b->b2, b->a1, b->a2,
			                   &b->z1[0], &b->z2[0]);
			/* In mid or side mode r is the untouched half, so it is carried
			   through rather than filtered. */
			if (d->msMode == MS_MODE_STEREO)
				r = dynEqRunBiquad(r, b->b0, b->b1, b->b2, b->a1, b->a2,
				                   &b->z1[1], &b->z2[1]);
		}

		if (++d->redesign >= DYNEQ_REDESIGN)
		{
			d->redesign = 0;
			for (int k = 0; k < d->numBands; k++)
			{
				DynEqBand *b = &d->band[k];
				if (fabsf(b->gainDb - b->appliedDb) > DYNEQ_EPS_DB)
				{
					b->appliedDb = b->gainDb;
					dynEqDesignPeaking(b, fs, b->appliedDb);
				}
			}
		}

		float wetL, wetR;
		if (d->msMode == MS_MODE_STEREO)
		{
			wetL = l; wetR = r;
		}
		else
		{
			const float mid = (d->msMode == MS_MODE_MID) ? l : r;
			const float side = (d->msMode == MS_MODE_MID) ? r : l;
			wetL = mid + side;
			wetR = mid - side;
		}

		if (d->mix >= 1.0f)
		{
			left[i] = wetL;
			right[i] = wetR;
		}
		else
		{
			left[i] = dryL + (wetL - dryL) * d->mix;
			right[i] = dryR + (wetR - dryR) * d->mix;
		}
	}
}

void DynamicEqEnable(JamesDSPLib *jdsp)
{
	if (jdsp->dynamicEqEnabled)
		return;
	jdsp_lock(jdsp);
	DynamicEq *d = &jdsp->dynamicEq;
	d->fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;
	d->redesign = 0;
	for (int k = 0; k < d->numBands; k++)
	{
		DynEqBand *b = &d->band[k];
		b->env = 0.0f;
		b->gainDb = b->appliedDb = 0.0f;
		b->sz1[0] = b->sz2[0] = b->sz1[1] = b->sz2[1] = 0.0f;
		b->z1[0] = b->z2[0] = b->z1[1] = b->z2[1] = 0.0f;
		dynEqRefreshBand(b, d->fs);
	}
	jdsp->dynamicEqEnabled = 1;
	jdsp_unlock(jdsp);
}

void DynamicEqDisable(JamesDSPLib *jdsp)
{
	jdsp_lock(jdsp);
	jdsp->dynamicEqEnabled = 0;
	jdsp_unlock(jdsp);
}
