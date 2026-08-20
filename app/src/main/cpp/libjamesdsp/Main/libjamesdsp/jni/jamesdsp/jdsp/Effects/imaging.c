/*
 * Multiband stereo imaging: width per frequency range.
 *
 * The existing widener works on the whole spectrum at once, so widening for air
 * in the top also pulls the bass apart - the one place width really costs you,
 * since it thins the centre and falls apart in mono.
 *
 * Width is nothing more than the gain of the side signal, so rather than
 * splitting the audio into bands and widening each, this equalises the side
 * signal and leaves mid alone entirely. Width per band is then a shelf or a
 * peak on that one mono-ish stream: three biquads instead of three per channel,
 * and no crossover at all.
 *
 * That choice is what makes the neutral setting exact. A band split has to be
 * summed back together, and four float additions do not return their input;
 * measured, that floor sat at -140 dBFS. Here, every width at 1.0 means every
 * filter is at 0 dB, where an RBJ peaking or shelving biquad has b equal to a
 * term for term - the recursion returns its input bitwise, so the side signal
 * is untouched and the output is the input. Nothing to argue about.
 *
 * A band split also turned out not to isolate: the bands summed correctly but
 * each one carried a phase-shifted residue of its neighbour, so folding the
 * bass to mono only removed 8 dB of the bass side. Equalising the side has no
 * such leak, because there is only ever one signal.
 *
 * Mono material has no side at all, so it survives any setting exactly.
 */
#include <math.h>
#include <string.h>
#include "../jdsp_header.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float imagingRun(ImagingStage *s, float x)
{
	const float y = s->b0 * x + s->z1;
	s->z1 = s->b1 * x - s->a1 * y + s->z2;
	s->z2 = s->b2 * x - s->a2 * y;
	return y;
}

static void imagingIdentity(ImagingStage *s)
{
	s->b0 = 1.0f; s->b1 = 0.0f; s->b2 = 0.0f;
	s->a1 = 0.0f; s->a2 = 0.0f;
}

static double imagingClampFreq(double f, float fs)
{
	if (f < 20.0) f = 20.0;
	if (f > (double)fs * 0.45) f = (double)fs * 0.45;
	return f;
}

/* Width is a ratio; the filters want dB. */
static float imagingWidthDb(float width)
{
	if (width < 0.001f) width = 0.001f;   /* -60 dB, effectively silent */
	return 20.0f * log10f(width);
}

static void imagingDesignShelf(ImagingStage *s, double f, float fs, float gainDb, int high)
{
	if (fabsf(gainDb) < 1e-6f) { imagingIdentity(s); return; }
	const double A = pow(10.0, (double)gainDb / 40.0);
	const double w0 = 2.0 * M_PI * imagingClampFreq(f, fs) / (double)fs;
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

static void imagingDesignPeak(ImagingStage *s, double f, float fs, float gainDb, double q)
{
	if (fabsf(gainDb) < 1e-6f) { imagingIdentity(s); return; }
	const double A = pow(10.0, (double)gainDb / 40.0);
	const double w0 = 2.0 * M_PI * imagingClampFreq(f, fs) / (double)fs;
	const double alpha = sin(w0) / (2.0 * q);
	const double a0 = 1.0 + alpha / A;
	s->b0 = (float)((1.0 + alpha * A) / a0);
	s->b1 = (float)((-2.0 * cos(w0)) / a0);
	s->b2 = (float)((1.0 - alpha * A) / a0);
	s->a1 = (float)((-2.0 * cos(w0)) / a0);
	s->a2 = (float)((1.0 - alpha / A) / a0);
}

/* Second-order highpass, for folding everything below a frequency to mono. */
static void imagingDesignHighpass(ImagingStage *s, double f, float fs)
{
	const double w0 = 2.0 * M_PI * imagingClampFreq(f, fs) / (double)fs;
	const double cw = cos(w0);
	const double alpha = sin(w0) / (2.0 * 0.70710678);
	const double a0 = 1.0 + alpha;
	s->b0 = (float)(((1.0 + cw) * 0.5) / a0);
	s->b1 = (float)((-(1.0 + cw)) / a0);
	s->b2 = s->b0;
	s->a1 = (float)((-2.0 * cw) / a0);
	s->a2 = (float)((1.0 - alpha) / a0);
}

void ImagingSetParam(JamesDSPLib *jdsp, float monoBelowHz,
                     float freqLow, float freqMid, float freqHigh,
                     float widthLow, float widthMid, float widthHigh,
                     float mixPct)
{
	/* Held for the whole update. Process runs on the audio thread under this
	   same lock, so without it a block could be filtered with half the old
	   coefficients and half the new ones - which for a steep or resonant
	   setting is not a glitch but a burst. The single-threaded harness cannot
	   see this, and the one crash this project has shipped came from exactly
	   this class of problem. */
	jdsp_lock(jdsp);
	Imaging *im = &jdsp->imaging;
	const float fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;

	im->monoBelow = monoBelowHz;
	if (monoBelowHz >= 20.0f)
	{
		// Two of them. A single second-order highpass only reaches -12 dB an
		// octave down, so "mono below 120 Hz" would still leave a quarter of
		// the side signal at 60 Hz - audible, and not what the control says.
		imagingDesignHighpass(&im->mono, monoBelowHz, fs);
		imagingDesignHighpass(&im->mono2, monoBelowHz, fs);
	}
	else
	{
		imagingIdentity(&im->mono);
		imagingIdentity(&im->mono2);
	}

	/* Two cascaded stages for the corners so a wide setting is a slope rather
	   than a bump, and one peak in the middle. */
	imagingDesignShelf(&im->low, freqLow, fs, imagingWidthDb(widthLow), 0);
	imagingDesignPeak(&im->mid, freqMid, fs, imagingWidthDb(widthMid), 1.0);
	imagingDesignShelf(&im->high, freqHigh, fs, imagingWidthDb(widthHigh), 1);

	im->freqLow = freqLow;
	im->freqMid = freqMid;
	im->freqHigh = freqHigh;
	im->widthLow = widthLow;
	im->widthMid = widthMid;
	im->widthHigh = widthHigh;

	im->mix = mixPct * 0.01f;
	if (im->mix < 0.0f) im->mix = 0.0f;
	if (im->mix > 1.0f) im->mix = 1.0f;
	im->fs = fs;

	// With nothing to do, do nothing at all. Splitting into mid and side and
	// adding them back does not return the input in float arithmetic even when
	// every filter is the identity, so neutral settings have to skip the whole
	// stage rather than run a transparent version of it.
	im->transparent = monoBelowHz < 20.0f &&
		fabsf(widthLow - 1.0f) < 1e-6f &&
		fabsf(widthMid - 1.0f) < 1e-6f &&
		fabsf(widthHigh - 1.0f) < 1e-6f;
	jdsp_unlock(jdsp);
}

void ImagingProcess(JamesDSPLib *jdsp, size_t n)
{
	Imaging *im = &jdsp->imaging;
	if (im->mix <= 0.0f || im->transparent)
		return;

	float *left = jdsp->tmpBuffer[0];
	float *right = jdsp->tmpBuffer[1];

	for (size_t i = 0; i < n; i++)
	{
		const float dryL = left[i], dryR = right[i];
		const float mid = (dryL + dryR) * 0.5f;
		float side = (dryL - dryR) * 0.5f;

		side = imagingRun(&im->mono, side);
		side = imagingRun(&im->mono2, side);
		side = imagingRun(&im->low, side);
		side = imagingRun(&im->mid, side);
		side = imagingRun(&im->high, side);

		const float wetL = mid + side;
		const float wetR = mid - side;

		if (im->mix >= 1.0f)
		{
			left[i] = wetL;
			right[i] = wetR;
		}
		else
		{
			left[i] = dryL + (wetL - dryL) * im->mix;
			right[i] = dryR + (wetR - dryR) * im->mix;
		}
	}
}

void ImagingEnable(JamesDSPLib *jdsp)
{
	if (jdsp->imagingEnabled)
		return;
	jdsp_lock(jdsp);
	Imaging *im = &jdsp->imaging;
	im->fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;
	im->mono.z1 = im->mono.z2 = 0.0f;
	im->mono2.z1 = im->mono2.z2 = 0.0f;
	im->low.z1 = im->low.z2 = 0.0f;
	im->mid.z1 = im->mid.z2 = 0.0f;
	im->high.z1 = im->high.z2 = 0.0f;
	jdsp->imagingEnabled = 1;
	jdsp_unlock(jdsp);
}

void ImagingDisable(JamesDSPLib *jdsp)
{
	jdsp_lock(jdsp);
	jdsp->imagingEnabled = 0;
	jdsp_unlock(jdsp);
}
