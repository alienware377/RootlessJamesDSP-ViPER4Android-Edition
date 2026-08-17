// Stability and behaviour harness for the multiband distortion effect.
//
// Built for the device rather than the host, because the arm64 build is the
// one that ships and single-precision behaviour is exactly what is under test.
// Run it before the effect goes anywhere near an APK: the two worst bugs this
// project has shipped were both untested DSP, and both silenced audio rather
// than sounding wrong.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define N 4096

static JamesDSPLib g_lib;
static float bufL[N], bufR[N];

static void prepare(float fs)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = fs;
	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
}

// A signal with content across the spectrum, so a band-select filter has
// something to select and something to leave alone.
static void fillTest(float amp)
{
	for (int i = 0; i < N; i++)
	{
		double t = (double)i / 48000.0;
		double v = 0.5 * sin(2.0 * M_PI * 60.0 * t)
		         + 0.3 * sin(2.0 * M_PI * 700.0 * t)
		         + 0.2 * sin(2.0 * M_PI * 6000.0 * t);
		bufL[i] = (float)(v * amp);
		bufR[i] = (float)(v * amp * 0.9);
	}
}

static int check(const char *name)
{
	float peak = 0.0f;
	double sum = 0.0;
	int bad = 0;
	for (int i = 0; i < N; i++)
	{
		if (!isfinite(bufL[i]) || !isfinite(bufR[i])) bad++;
		float a = fabsf(bufL[i]);
		if (a > peak) peak = a;
		sum += (double)bufL[i] * bufL[i];
	}
	double rms = sqrt(sum / N);
	printf("%-34s peak %6.3f  rms %6.4f  %s\n", name, peak, rms,
		bad ? "*** NON-FINITE ***" : (peak > 8.0f ? "*** RUNAWAY ***" : "ok"));
	return bad || peak > 8.0f;
}

// Feed silence after a loud burst and confirm the effect settles rather than
// self-oscillating - the failure mode a feedback path can hide until it is fed
// something it likes.
static int checkTail(const char *name)
{
	// Silence has to be re-fed on every pass. Processing in place and running
	// again feeds the effect its own output, which measures a forty-deep chain
	// of the effect rather than its decay - the first version of this test did
	// exactly that and reported ringing on models that do not ring.
	for (int k = 0; k < 40; k++)
	{
		memset(bufL, 0, sizeof(bufL));
		memset(bufR, 0, sizeof(bufR));
		MultibandDistProcess(&g_lib, N);
	}
	double energy = 0.0;
	for (int i = 0; i < N; i++) energy += fabs(bufL[i]);
	int bad = !(energy < 1.0);
	printf("%-34s tail energy %10.6f  %s\n", name, energy, bad ? "*** RINGING ***" : "ok");
	return bad;
}

int main(void)
{
	int fail = 0;
	const char *models[] = { "soft", "hard", "tube", "overdrive",
	                         "fold", "fuzz", "rectify", "crush" };

	// A default-looking band list: one low-pass cutoff, which is what the
	// editor starts with.
	float lowpass[4] = { 250.0f, 0.0f, 0.707f, MBD_FILTER_LOW_PASS };

	printf("== every model, hard driven, split routing ==\n");
	for (int mdl = 0; mdl < MBD_MODEL_COUNT; mdl++)
	{
		prepare(48000.0f);
		MultibandDistEnable(&g_lib);
		MultibandDistSetBands(&g_lib, lowpass, 1);
		MultibandDistSetParam(&g_lib, MBD_ROUTING_SPLIT, mdl,
			100.0f, 60.0f, 80.0f,   /* drive, bias, shape */
			4.0f, 70.0f,            /* bits, downsample */
			80.0f, 150.0f,          /* tone, band gain */
			4.0f, 20.0f, 85.0f,     /* chorus rate, depth, feedback */
			100.0f, MBD_CHORUS_VOICES, 100.0f,
			100.0f);
		fillTest(0.8f);
		MultibandDistProcess(&g_lib, N);
		fail |= check(models[mdl]);
		fail |= checkTail(models[mdl]);
		MultibandDistDisable(&g_lib);
	}

	printf("\n== the null: drive 0 must return the input untouched ==\n");
	{
		prepare(48000.0f);
		MultibandDistEnable(&g_lib);
		MultibandDistSetBands(&g_lib, lowpass, 1);
		MultibandDistSetParam(&g_lib, MBD_ROUTING_SPLIT, MBD_MODEL_SOFT,
			0.0f, 0.0f, 50.0f, 16.0f, 0.0f, 50.0f, 100.0f,
			0.6f, 6.0f, 0.0f, 50.0f, 2, 0.0f, 100.0f);
		fillTest(0.5f);
		static float ref[N];
		memcpy(ref, bufL, sizeof(ref));
		MultibandDistProcess(&g_lib, N);
		double worst = 0.0;
		for (int i = 64; i < N; i++)
		{
			double d = fabs((double)bufL[i] - ref[i]);
			if (d > worst) worst = d;
		}
		// Exact, not approximate: at neutral settings the stage is skipped
		// outright rather than run and cancelled, so there is nothing left
		// to deviate.
		int bad = !(worst < 1e-6);
		printf("%-34s worst deviation %.9f  %s\n", "split, drive 0", worst,
			bad ? "*** NOT NULL ***" : "ok");
		fail |= bad;
		MultibandDistDisable(&g_lib);
	}

	printf("\n== mix 0 must be bit-exact dry, whatever else is set ==\n");
	{
		prepare(48000.0f);
		MultibandDistEnable(&g_lib);
		MultibandDistSetBands(&g_lib, lowpass, 1);
		MultibandDistSetParam(&g_lib, MBD_ROUTING_PARALLEL, MBD_MODEL_FUZZ,
			100.0f, 100.0f, 100.0f, 2.0f, 100.0f, 100.0f, 200.0f,
			8.0f, 25.0f, 90.0f, 100.0f, 4, 100.0f, 0.0f);
		fillTest(0.7f);
		static float ref[N];
		memcpy(ref, bufL, sizeof(ref));
		MultibandDistProcess(&g_lib, N);
		double worst = 0.0;
		for (int i = 0; i < N; i++)
		{
			double d = fabs((double)bufL[i] - ref[i]);
			if (d > worst) worst = d;
		}
		int bad = !(worst < 1e-6);
		printf("%-34s worst deviation %.9f  %s\n", "mix 0", worst,
			bad ? "*** LEAKS ***" : "ok");
		fail |= bad;
		MultibandDistDisable(&g_lib);
	}

	printf("\n== every filter type in the band cascade ==\n");
	{
		const char *fnames[] = { "peaking", "low shelf", "high shelf",
		                         "low pass", "high pass" };
		for (int ft = 0; ft <= MBD_FILTER_HIGH_PASS; ft++)
		{
			prepare(48000.0f);
			MultibandDistEnable(&g_lib);
			float band[8] = { 300.0f, 12.0f, 2.0f, (float)ft,
			                  4000.0f, -9.0f, 0.5f, (float)ft };
			MultibandDistSetBands(&g_lib, band, 2);
			MultibandDistSetParam(&g_lib, MBD_ROUTING_SPLIT, MBD_MODEL_OVERDRIVE,
				70.0f, 0.0f, 50.0f, 16.0f, 0.0f, 50.0f, 100.0f,
				1.0f, 8.0f, 40.0f, 50.0f, 3, 40.0f, 100.0f);
			fillTest(0.8f);
			MultibandDistProcess(&g_lib, N);
			fail |= check(fnames[ft]);
			MultibandDistDisable(&g_lib);
		}
	}

	printf("\n== sample rates ==\n");
	{
		float rates[] = { 44100.0f, 48000.0f, 96000.0f, 192000.0f };
		for (int r = 0; r < 4; r++)
		{
			char label[48];
			prepare(rates[r]);
			MultibandDistEnable(&g_lib);
			MultibandDistSetBands(&g_lib, lowpass, 1);
			MultibandDistSetParam(&g_lib, MBD_ROUTING_SPLIT, MBD_MODEL_FOLD,
				90.0f, 40.0f, 100.0f, 8.0f, 50.0f, 60.0f, 130.0f,
				3.0f, 25.0f, 85.0f, 100.0f, 4, 90.0f, 100.0f);
			fillTest(0.8f);
			MultibandDistProcess(&g_lib, N);
			snprintf(label, sizeof(label), "%.0f Hz, chorus at max", rates[r]);
			fail |= check(label);
			MultibandDistDisable(&g_lib);
		}
	}

	printf("\n== no bands selected, and more bands than the cascade holds ==\n");
	{
		prepare(48000.0f);
		MultibandDistEnable(&g_lib);
		MultibandDistSetBands(&g_lib, NULL, 0);
		MultibandDistSetParam(&g_lib, MBD_ROUTING_SPLIT, MBD_MODEL_TUBE,
			80.0f, 0.0f, 50.0f, 16.0f, 0.0f, 50.0f, 100.0f,
			1.0f, 5.0f, 0.0f, 50.0f, 2, 0.0f, 100.0f);
		fillTest(0.8f);
		MultibandDistProcess(&g_lib, N);
		fail |= check("no bands");

		static float many[4 * 40];
		for (int i = 0; i < 40; i++)
		{
			many[i * 4 + 0] = 100.0f + i * 400.0f;
			many[i * 4 + 1] = 6.0f;
			many[i * 4 + 2] = 1.0f;
			many[i * 4 + 3] = MBD_FILTER_PEAKING;
		}
		MultibandDistSetBands(&g_lib, many, 40);
		fillTest(0.8f);
		MultibandDistProcess(&g_lib, N);
		fail |= check("40 bands, clamped to cascade");
		MultibandDistDisable(&g_lib);
	}

	printf("\n== enable, disable, enable again ==\n");
	{
		prepare(48000.0f);
		for (int k = 0; k < 3; k++)
		{
			MultibandDistEnable(&g_lib);
			MultibandDistSetBands(&g_lib, lowpass, 1);
			MultibandDistSetParam(&g_lib, MBD_ROUTING_PARALLEL, MBD_MODEL_CRUSH,
				60.0f, 0.0f, 50.0f, 3.0f, 80.0f, 50.0f, 100.0f,
				2.0f, 10.0f, 50.0f, 60.0f, 2, 60.0f, 100.0f);
			fillTest(0.6f);
			MultibandDistProcess(&g_lib, N);
			MultibandDistDisable(&g_lib);
		}
		fail |= check("three enable/disable cycles");
	}

	printf("\n%s\n", fail ? "FAILURES PRESENT" : "all checks passed");
	return fail ? 1 : 0;
}
