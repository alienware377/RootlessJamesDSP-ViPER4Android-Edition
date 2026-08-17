// Does a cutoff actually cut off?
//
// The complaint that prompted this was precise: the low-pass "goes all the way
// across the freq spectrum before dropping". That is what one biquad does - it
// rolls off at 12 dB/octave, which over the ten octaves of the audio band is a
// tilt, not a corner. So this measures the slope rather than merely checking
// that a filter was installed, and it measures the coefficients the engine
// actually loaded rather than recomputing them here, which would only prove
// this file agrees with itself.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define N 8192

static JamesDSPLib g_lib;
static float bufL[N], bufR[N];
static int failures = 0;

static void prepare(float fs)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = fs;
	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
}

// |H(e^jw)| in dB for the whole loaded cascade, from the coefficients the
// engine will run. Sections are in series, so the dB add.
static double responseDb(const MultibandDist *m, double freq, double fs)
{
	double w = 2.0 * M_PI * freq / fs, total = 0.0;
	for (int i = 0; i < m->numBands; i++)
	{
		double cw = cos(w), c2w = cos(2.0 * w), sw = sin(w), s2w = sin(2.0 * w);
		double nr = m->b0[i] + m->b1[i] * cw + m->b2[i] * c2w;
		double ni = -(m->b1[i] * sw + m->b2[i] * s2w);
		// The engine stores a1/a2 already normalised by a0.
		double dr = 1.0 + m->a1[i] * cw + m->a2[i] * c2w;
		double di = -(m->a1[i] * sw + m->a2[i] * s2w);
		double d = dr * dr + di * di;
		if (d <= 0.0) return -999.0;
		total += 10.0 * log10((nr * nr + ni * ni) / d);
	}
	return total;
}

static void check(const char *what, double got, double lo, double hi)
{
	int ok = got >= lo && got <= hi;
	printf("  %-42s %9.2f  [%7.1f..%7.1f]  %s\n",
		what, got, lo, hi, ok ? "ok" : "FAIL");
	if (!ok) failures++;
}

static void load(float freq, float slope, float q, int type)
{
	float band[4] = { freq, slope, q, (float)type };
	MultibandDistSetBands(&g_lib, band, 1);
}

// A cutoff at `freq` should be flat below it, down 3 dB at it, and down by
// `slope` for every octave past it.
static void measure(const char *name, float freq, float slope, int type)
{
	MultibandDist *m = &g_lib.multibandDist;
	load(freq, slope, 0.70710678f, type);
	int expectSections = (int)((slope < 6.0f ? 48.0f : slope) / 12.0f + 0.5f);
	if (expectSections < 1) expectSections = 1;
	if (expectSections > MBD_MAX_CUTOFF_STAGES) expectSections = MBD_MAX_CUTOFF_STAGES;
	double perOctave = expectSections * 12.0;

	printf("%s @ %.0f Hz, slope %.0f dB/oct\n", name, freq, slope);
	check("sections loaded", (double)m->numBands, expectSections, expectSections);

	double lo = (type == MBD_FILTER_LOW_PASS) ? freq / 4.0 : freq * 4.0;
	double one = (type == MBD_FILTER_LOW_PASS) ? freq * 2.0 : freq / 2.0;
	double two = (type == MBD_FILTER_LOW_PASS) ? freq * 4.0 : freq / 4.0;

	// Butterworth is maximally flat: two octaves into the passband should be
	// untouched. A single-biquad "cutoff" with a raised Q would bulge here.
	check("passband, 2 oct in (dB)", responseDb(m, lo, g_lib.fs), -0.6, 0.3);
	check("corner (dB)", responseDb(m, freq, g_lib.fs), -4.5, -1.5);

	// The point of the whole change. One octave past the corner the level has
	// to have fallen by the full slope, not by 12 dB.
	double d1 = responseDb(m, one, g_lib.fs);
	check("1 oct past corner (dB)", d1, -perOctave - 9.0, -perOctave + 9.0);
	double d2 = responseDb(m, two, g_lib.fs);
	check("2 oct past corner (dB)", d2, -2.0 * perOctave - 14.0, -2.0 * perOctave + 14.0);
	printf("  -> measured %.1f dB/octave in the stopband\n\n", d1 - d2);
}

int main(void)
{
	printf("== multiband distortion cutoff slope ==\n\n");
	prepare(48000.0f);

	// What shipped before: one biquad. Kept as a measurement rather than a
	// target, so the numbers below have something to be compared against.
	measure("low-pass, shallow", 1000.0f, 12.0f, MBD_FILTER_LOW_PASS);
	measure("low-pass, default", 1000.0f, 48.0f, MBD_FILTER_LOW_PASS);
	measure("low-pass, steepest", 1000.0f, 96.0f, MBD_FILTER_LOW_PASS);
	measure("high-pass, default", 1000.0f, 48.0f, MBD_FILTER_HIGH_PASS);

	// Bands saved before the slope field existed carry 0 there. They must come
	// back as the default rather than as a single section.
	printf("legacy band (slope field absent)\n");
	load(1000.0f, 0.0f, 0.70710678f, MBD_FILTER_LOW_PASS);
	check("sections loaded", (double)g_lib.multibandDist.numBands, 4, 4);
	printf("\n");

	// A cutoff eats one slot per 12 dB/octave, so a full editor of steep
	// cutoffs must clamp rather than write past the arrays.
	printf("slot exhaustion\n");
	float many[8 * 4];
	for (int i = 0; i < 8; i++)
	{
		many[i * 4 + 0] = 200.0f * (i + 1);
		many[i * 4 + 1] = 96.0f;
		many[i * 4 + 2] = 0.70710678f;
		many[i * 4 + 3] = (float)MBD_FILTER_LOW_PASS;
	}
	MultibandDistSetBands(&g_lib, many, 8);
	check("clamped to array size", (double)g_lib.multibandDist.numBands, 1, MBD_MAX_BANDS);
	printf("\n");

	// Steep cascades are the classic place for a filter to blow up in single
	// precision, so run audio through the real process path and look at it.
	printf("stability with audio\n");
	load(1000.0f, 96.0f, 0.70710678f, MBD_FILTER_LOW_PASS);
	MultibandDistSetParam(&g_lib, MBD_ROUTING_SPLIT, MBD_MODEL_TUBE,
		/* drive */ 60.0f, /* bias */ 10.0f, /* shape */ 50.0f,
		/* bits */ 16.0f, /* downsample */ 0.0f,
		/* tone */ 50.0f, /* band gain */ 50.0f,
		/* chorus */ 1.5f, 3.0f, 20.0f, 60.0f, 3, 30.0f,
		/* mix */ 100.0f);
	MultibandDistEnable(&g_lib);
	double worst = 0.0;
	int bad = 0;
	for (int pass = 0; pass < 8; pass++)
	{
		for (int i = 0; i < N; i++)
		{
			double t = (double)(pass * N + i) / 48000.0;
			double v = 0.6 * sin(2.0 * M_PI * 220.0 * t)
			         + 0.3 * sin(2.0 * M_PI * 3300.0 * t);
			bufL[i] = (float)v;
			bufR[i] = (float)(v * 0.9);
		}
		MultibandDistProcess(&g_lib, N);
		for (int i = 0; i < N; i++)
		{
			float a = fabsf(bufL[i]), b = fabsf(bufR[i]);
			if (!isfinite(bufL[i]) || !isfinite(bufR[i])) bad++;
			if (a > worst) worst = a;
			if (b > worst) worst = b;
		}
	}
	check("non-finite samples", (double)bad, 0, 0);
	check("peak stays bounded", worst, 0.05, 8.0);

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
		failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
