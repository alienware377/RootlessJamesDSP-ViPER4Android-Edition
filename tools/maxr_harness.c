// Stability and behaviour harness for the maximiser.
//
// A limiter has one promise - nothing leaves above the ceiling - and it is
// measurable, so this checks it directly rather than only looking for NaN.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define N 8192

static JamesDSPLib g_lib;
static float bufL[N], bufR[N];

static void prepare(float fs)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = fs;
	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
}

static void fillMusic(float amp)
{
	for (int i = 0; i < N; i++)
	{
		double t = (double)i / 48000.0;
		double v = 0.5 * sin(2.0 * M_PI * 55.0 * t)
		         + 0.3 * sin(2.0 * M_PI * 440.0 * t)
		         + 0.2 * sin(2.0 * M_PI * 3300.0 * t);
		bufL[i] = (float)(v * amp);
		bufR[i] = (float)(v * amp * 0.85);
	}
}

// Impulses on one channel only, which is what stereo linking is about.
static void fillTransients(float amp)
{
	memset(bufL, 0, sizeof(bufL));
	memset(bufR, 0, sizeof(bufR));
	for (int i = 0; i < N; i++)
	{
		double t = (double)i / 48000.0;
		bufL[i] = (float)(0.15 * sin(2.0 * M_PI * 220.0 * t));
		bufR[i] = bufL[i];
		if (i % 1024 == 0) bufL[i] = amp;
	}
}

static int checkCeiling(const char *name, float ceilingDb)
{
	float ceiling = powf(10.0f, ceilingDb / 20.0f);
	float peak = 0.0f;
	double sum = 0.0;
	int bad = 0;
	for (int i = 0; i < N; i++)
	{
		if (!isfinite(bufL[i]) || !isfinite(bufR[i])) bad++;
		float a = fabsf(bufL[i]); if (a > peak) peak = a;
		a = fabsf(bufR[i]); if (a > peak) peak = a;
		sum += (double)bufL[i] * bufL[i];
	}
	double rms = sqrt(sum / N);
	// A hair of tolerance for float rounding in the final clamp, nothing more.
	int over = peak > ceiling * 1.0005f;
	printf("%-38s peak %7.4f (ceiling %6.4f)  rms %6.4f  %s\n",
		name, peak, ceiling, rms,
		bad ? "*** NON-FINITE ***" : (over ? "*** OVER CEILING ***" : "ok"));
	return bad || over;
}

int main(void)
{
	int fail = 0;
	const char *modes[] = { "transparent", "punchy", "warm", "aggressive" };

	printf("== every mode, driven hard into a low ceiling ==\n");
	for (int mode = 0; mode < MAXR_MODE_COUNT; mode++)
	{
		char label[64];
		prepare(48000.0f);
		MaximizerEnable(&g_lib);
		MaximizerSetParam(&g_lib, mode, 24.0f, -1.0f, 50.0f,
			100.0f, 100.0f, 1, 100.0f, 2);
		fillMusic(0.9f);
		MaximizerProcess(&g_lib, N);
		snprintf(label, sizeof(label), "%s, +24dB in", modes[mode]);
		fail |= checkCeiling(label, -1.0f);
		MaximizerDisable(&g_lib);
	}

	printf("\n== the ceiling holds at every setting of it ==\n");
	{
		float ceilings[] = { 0.0f, -0.3f, -1.0f, -3.0f, -6.0f, -12.0f };
		for (int c = 0; c < 6; c++)
		{
			char label[64];
			prepare(48000.0f);
			MaximizerEnable(&g_lib);
			MaximizerSetParam(&g_lib, MAXR_MODE_AGGRESSIVE, 18.0f, ceilings[c],
				5.0f, 60.0f, 80.0f, 1, 50.0f, 2);
			fillMusic(1.0f);
			MaximizerProcess(&g_lib, N);
			snprintf(label, sizeof(label), "ceiling %.1f dB", ceilings[c]);
			fail |= checkCeiling(label, ceilings[c]);
			MaximizerDisable(&g_lib);
		}
	}

	printf("\n== unity: no gain, no character, ceiling open ==\n");
	{
		prepare(48000.0f);
		MaximizerEnable(&g_lib);
		MaximizerSetParam(&g_lib, MAXR_MODE_TRANSPARENT, 0.0f, 0.0f, 200.0f,
			0.0f, 0.0f, 0, 100.0f, 0);
		fillMusic(0.3f);
		static float ref[N];
		memcpy(ref, bufL, sizeof(ref));
		MaximizerProcess(&g_lib, N);
		// The output is the input delayed by the lookahead, so compare against
		// the input shifted by that much rather than sample for sample.
		int look = g_lib.maximizer.lookahead;
		double worst = 0.0;
		for (int i = look + 16; i < N; i++)
		{
			double d = fabs((double)bufL[i] - ref[i - look]);
			if (d > worst) worst = d;
		}
		int bad = !(worst < 1e-4);
		printf("%-38s worst deviation %.9f (lookahead %d)  %s\n",
			"quiet signal passes through", worst, look,
			bad ? "*** COLOURED ***" : "ok");
		fail |= bad;
		MaximizerDisable(&g_lib);
	}

	printf("\n== stereo link: unlinked must not move the quiet channel ==\n");
	{
		prepare(48000.0f);
		MaximizerEnable(&g_lib);
		MaximizerSetParam(&g_lib, MAXR_MODE_TRANSPARENT, 6.0f, -1.0f, 100.0f,
			0.0f, 0.0f, 0, 0.0f, 0);
		fillTransients(0.95f);
		static float refR[N];
		memcpy(refR, bufR, sizeof(refR));
		MaximizerProcess(&g_lib, N);
		int look = g_lib.maximizer.lookahead;
		float gain = powf(10.0f, 6.0f / 20.0f);
		double worst = 0.0;
		for (int i = look + 16; i < N; i++)
		{
			double d = fabs((double)bufR[i] - refR[i - look] * gain);
			if (d > worst) worst = d;
		}
		// The right channel never exceeds the ceiling on its own, so with the
		// link off nothing should touch it beyond the input gain.
		int bad = !(worst < 1e-3);
		printf("%-38s worst deviation %.6f  %s\n",
			"unlinked, quiet channel", worst, bad ? "*** PULLED DOWN ***" : "ok");
		fail |= bad;

		// Linked, the same impulses must pull it down.
		prepare(48000.0f);
		MaximizerEnable(&g_lib);
		MaximizerSetParam(&g_lib, MAXR_MODE_TRANSPARENT, 6.0f, -1.0f, 100.0f,
			0.0f, 0.0f, 0, 100.0f, 0);
		fillTransients(0.95f);
		MaximizerProcess(&g_lib, N);
		double sum = 0.0;
		for (int i = 0; i < N; i++) sum += fabs(bufR[i]);
		printf("%-38s right-channel energy %.1f  (lower than unlinked is correct)\n",
			"linked, quiet channel", sum);
		MaximizerDisable(&g_lib);
	}

	printf("\n== sample rates, true peak on, 4x ==\n");
	{
		float rates[] = { 44100.0f, 48000.0f, 96000.0f, 192000.0f };
		for (int r = 0; r < 4; r++)
		{
			char label[64];
			prepare(rates[r]);
			MaximizerEnable(&g_lib);
			MaximizerSetParam(&g_lib, MAXR_MODE_WARM, 20.0f, -0.3f, 300.0f,
				80.0f, 50.0f, 1, 100.0f, 2);
			fillMusic(0.95f);
			MaximizerProcess(&g_lib, N);
			snprintf(label, sizeof(label), "%.0f Hz", rates[r]);
			fail |= checkCeiling(label, -0.3f);
			MaximizerDisable(&g_lib);
		}
	}

	printf("\n== a long run, to walk the sample counter ==\n");
	{
		prepare(48000.0f);
		MaximizerEnable(&g_lib);
		MaximizerSetParam(&g_lib, MAXR_MODE_PUNCHY, 12.0f, -0.5f, 30.0f,
			40.0f, 30.0f, 1, 100.0f, 1);
		// The window index is unsigned and compared by difference, so this is
		// really a check that the deque survives being run continuously.
		for (int k = 0; k < 200; k++)
		{
			fillMusic(0.85f);
			MaximizerProcess(&g_lib, N);
		}
		fail |= checkCeiling("1.6M samples continuous", -0.5f);
		MaximizerDisable(&g_lib);
	}

	printf("\n== enable, disable, enable again ==\n");
	{
		prepare(48000.0f);
		for (int k = 0; k < 3; k++)
		{
			MaximizerEnable(&g_lib);
			MaximizerSetParam(&g_lib, MAXR_MODE_AGGRESSIVE, 15.0f, -2.0f, 20.0f,
				70.0f, 60.0f, 1, 80.0f, 2);
			fillMusic(0.8f);
			MaximizerProcess(&g_lib, N);
			MaximizerDisable(&g_lib);
		}
		fail |= checkCeiling("three cycles", -2.0f);
	}

	printf("\n%s\n", fail ? "FAILURES PRESENT" : "all checks passed");
	return fail ? 1 : 0;
}
