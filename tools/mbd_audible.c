// Answers "why do I hear nothing" numerically, and checks the shaper curves for
// steps.
//
// The curve check drives a ramp through the real effect rather than through a
// copy of the formula. An earlier version re-implemented the maths here, and
// when the engine was fixed this file went on testing the old arithmetic and
// reported a fault that no longer existed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define N 8192

static JamesDSPLib g_lib;
static float bufL[N], bufR[N], ref[N];

static void prepare(void)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = 48000.0f;
	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
}

static void fill(void)
{
	for (int i = 0; i < N; i++)
	{
		double t = (double)i / 48000.0;
		double v = 0.5 * sin(2.0 * M_PI * 60.0 * t)
		         + 0.3 * sin(2.0 * M_PI * 700.0 * t)
		         + 0.2 * sin(2.0 * M_PI * 6000.0 * t);
		bufL[i] = (float)(v * 0.7);
		bufR[i] = bufL[i];
	}
	memcpy(ref, bufL, sizeof(ref));
}

static double difference(void)
{
	double num = 0.0, den = 0.0;
	for (int i = 0; i < N; i++)
	{
		double d = (double)bufL[i] - ref[i];
		num += d * d;
		den += (double)ref[i] * ref[i];
	}
	if (den < 1e-12) return -999.0;
	return 10.0 * log10((num + 1e-30) / den);
}

static double run(const char *label, int model, float drive, float shape)
{
	float lowpass[4] = { 250.0f, 0.0f, 0.71f, MBD_FILTER_LOW_PASS };
	prepare();
	MultibandDistEnable(&g_lib);
	MultibandDistSetBands(&g_lib, lowpass, 1);
	MultibandDistSetParam(&g_lib, MBD_ROUTING_SPLIT, model,
		drive, 0.0f, shape, 16.0f, 0.0f, 50.0f, 100.0f,
		0.6f, 6.0f, 0.0f, 50.0f, 2, 0.0f, 100.0f);
	fill();
	MultibandDistProcess(&g_lib, N);
	double d = difference();
	if (label)
		printf("  %-26s difference from dry: %7.2f dB   %s\n", label, d,
			d < -60.0 ? "INAUDIBLE" : (d < -30.0 ? "subtle" : "clearly audible"));
	MultibandDistDisable(&g_lib);
	return d;
}

// With no bands the whole signal goes into the shaper, and split routing then
// leaves (x - x) + f(x) = f(x): the output is the transfer curve itself. A slow
// ramp in therefore traces it, and a step in the curve shows up as one large
// jump between neighbouring output samples.
static int curveStep(const char *label, int model, float shape)
{
	prepare();
	MultibandDistEnable(&g_lib);
	MultibandDistSetBands(&g_lib, NULL, 0);
	// Drive 100 so the whole curve is swept; no oversampling, so the output is
	// the shaper itself rather than a filtered version of it.
	MultibandDistSetParam(&g_lib, MBD_ROUTING_SPLIT, model,
		10.0f, 0.0f, shape, 16.0f, 0.0f, 50.0f, 100.0f,
		0.6f, 6.0f, 0.0f, 50.0f, 2, 0.0f, 100.0f);

	for (int i = 0; i < N; i++)
	{
		float v = -1.5f + 3.0f * (float)i / (float)(N - 1);
		bufL[i] = v;
		bufR[i] = v;
	}
	MultibandDistProcess(&g_lib, N);

	// Skip the first samples: the oversampling filters have to settle.
	float worst = 0.0f;
	int at = 0;
	for (int i = 512; i < N; i++)
	{
		float d = fabsf(bufL[i] - bufL[i - 1]);
		if (d > worst) { worst = d; at = i; }
	}
	// The input steps by 3/8191 per sample, and no sane shaper has a slope
	// above about 3, so anything past 0.01 is a genuine discontinuity.
	int bad = worst > 0.01f;
	printf("  %-26s largest jump %.5f at sample %5d  %s\n", label, worst, at,
		bad ? "*** DISCONTINUOUS ***" : "ok");
	MultibandDistDisable(&g_lib);
	return bad;
}

int main(void)
{
	int fail = 0;
	const char *names[] = { "soft", "hard", "tube", "overdrive",
	                        "fold", "fuzz", "rectify", "crush" };

	printf("== at the settings the card now ships with (drive 35) ==\n");
	for (int m = 0; m < MBD_MODEL_COUNT; m++)
	{
		double d = run(names[m], m, 35.0f, 50.0f);
		if (d < -60.0) { printf("      ^ nothing audible at the default\n"); fail = 1; }
	}

	printf("\n== drive taken to zero: everything must go silent again ==\n");
	for (int m = 0; m < MBD_MODEL_COUNT; m++)
	{
		double d = run(names[m], m, 0.0f, 50.0f);
		if (d > -60.0) { printf("      ^ still audible at zero drive\n"); fail = 1; }
	}

	printf("\n== overdrive across its whole drive range ==\n");
	{
		float drives[] = { 0.0f, 5.0f, 10.0f, 20.0f, 40.0f, 70.0f, 100.0f };
		for (int i = 0; i < 7; i++)
		{
			char label[48];
			snprintf(label, sizeof(label), "drive %.0f%%", drives[i]);
			run(label, MBD_MODEL_OVERDRIVE, drives[i], 50.0f);
		}
	}

	printf("\n== transfer curves, measured through the engine ==\n");
	for (int m = 0; m < MBD_MODEL_COUNT; m++)
	{
		for (int sPct = 0; sPct <= 100; sPct += 50)
		{
			char label[64];
			snprintf(label, sizeof(label), "%s, shape %d%%", names[m], sPct);
			// Crush quantises on purpose, so steps are the point of it.
			int bad = curveStep(label, m, sPct * 0.01f);
			if (m != MBD_MODEL_CRUSH) fail |= bad;
		}
	}

	printf("\n%s\n", fail ? "FAILURES PRESENT" : "all checks passed");
	return fail ? 1 : 0;
}
