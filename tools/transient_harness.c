// Does the impact shaper sharpen onsets, and only onsets?
//
//   NDK=$LOCALAPPDATA/Android/Sdk/ndk/<ver>/toolchains/llvm/prebuilt/windows-x86_64/bin
//   J=app/src/main/cpp/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp
//   $NDK/aarch64-linux-android29-clang -O2 -w -ffp-contract=off -I $J \
//       tools/transient_harness.c $J/Effects/transient.c tools/lockstub.c -lm -o transient
//
// The load-bearing test is the steady tone: a compressor would pull it down,
// and this must not touch it at all. If that one passes while the percussive
// ones also pass, the effect really is keying on the shape of a note rather
// than on how loud it is.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define N 96000          /* two seconds at 48k */

static JamesDSPLib g_lib;
static float bufL[N], bufR[N];
static float dryL[N], dryR[N];
static int failures = 0;

static void prepare(float fs)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = fs;
	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
}

static void keep(void)
{
	memcpy(dryL, bufL, sizeof(dryL));
	memcpy(dryR, bufR, sizeof(dryR));
}

/* Percussive hits: a fast rise, an exponential tail, repeated. */
static void fillHits(double fs, double toneHz, double decayMs, double periodMs, float amp)
{
	const int period = (int)(periodMs * 0.001 * fs);
	const double decay = decayMs * 0.001 * fs;
	for (int i = 0; i < N; i++)
	{
		const int phase = i % period;
		const double env = exp(-(double)phase / decay);
		const double v = amp * env * sin(2.0 * M_PI * toneHz * (double)i / fs);
		bufL[i] = (float)v;
		bufR[i] = (float)v;
	}
	keep();
}

static void fillSteady(double fs, double toneHz, float amp)
{
	for (int i = 0; i < N; i++)
	{
		const double v = amp * sin(2.0 * M_PI * toneHz * (double)i / fs);
		bufL[i] = (float)v;
		bufR[i] = (float)v;
	}
	keep();
}

/* Peak of the processed signal over the second half, in dB relative to dry. */
static double peakChangeDb(void)
{
	double pd = 0.0, pw = 0.0;
	for (int i = N / 2; i < N; i++)
	{
		const double a = fabs((double)dryL[i]);
		const double b = fabs((double)bufL[i]);
		if (a > pd) pd = a;
		if (b > pw) pw = b;
	}
	if (pd <= 0.0) pd = 1e-12;
	if (pw <= 0.0) pw = 1e-12;
	return 20.0 * log10(pw / pd);
}

/* Energy in the tail of each hit - the last third of every period. */
static double tailChangeDb(double fs, double periodMs)
{
	const int period = (int)(periodMs * 0.001 * fs);
	double ed = 0.0, ew = 0.0;
	for (int i = N / 2; i < N; i++)
	{
		if (i % period < (period * 2) / 3) continue;
		ed += (double)dryL[i] * (double)dryL[i];
		ew += (double)bufL[i] * (double)bufL[i];
	}
	if (ed <= 0.0) ed = 1e-24;
	if (ew <= 0.0) ew = 1e-24;
	return 10.0 * log10(ew / ed);
}

static double rmsChangeDb(void)
{
	double ed = 0.0, ew = 0.0;
	for (int i = N / 2; i < N; i++)
	{
		ed += (double)dryL[i] * (double)dryL[i];
		ew += (double)bufL[i] * (double)bufL[i];
	}
	if (ed <= 0.0) ed = 1e-24;
	if (ew <= 0.0) ew = 1e-24;
	return 10.0 * log10(ew / ed);
}

static void check(const char *what, double got, double lo, double hi)
{
	const int ok = got >= lo && got <= hi;
	printf("  %-46s %9.2f  [%8.2f..%8.2f]  %s\n", what, got, lo, hi, ok ? "ok" : "FAIL");
	if (!ok) failures++;
}

static int bitIdentical(void)
{
	for (int i = 0; i < N; i++)
		if (bufL[i] != dryL[i] || bufR[i] != dryR[i]) return 0;
	return 1;
}

/* attack/sustain on the low band only, unless stated otherwise. */
static void setLow(float attack, float sustain, float range)
{
	TransientSetParam(&g_lib, 200.0f, 3000.0f, attack, sustain,
	                  0.0f, 0.0f, 0.0f, 0.0f, range, 100.0f);
}

int main(void)
{
	const double fs = 48000.0;
	printf("== impact / transient shaper ==\n\n");

	/* ---- neutral does nothing at all -------------------------------------- */
	printf("neutral\n");
	{
		prepare((float)fs);
		TransientSetParam(&g_lib, 200.0f, 3000.0f, 0,0, 0,0, 0,0, 9.0f, 100.0f);
		TransientEnable(&g_lib);
		fillHits(fs, 110.0, 60.0, 400.0, 0.7f);
		TransientProcess(&g_lib, N);
		check("no shaping is bit-identical", bitIdentical() ? 1.0 : 0.0, 1.0, 1.0);
	}
	{
		prepare((float)fs);
		TransientSetParam(&g_lib, 200.0f, 3000.0f, 80,80, 80,80, 80,80, 0.0f, 100.0f);
		TransientEnable(&g_lib);
		fillHits(fs, 110.0, 60.0, 400.0, 0.7f);
		TransientProcess(&g_lib, N);
		check("zero range is bit-identical", bitIdentical() ? 1.0 : 0.0, 1.0, 1.0);
	}

	/* ---- a steady tone is not a transient --------------------------------- */
	printf("\nsteady tone (a compressor would move this)\n");
	{
		prepare((float)fs);
		setLow(90.0f, -90.0f, 12.0f);
		TransientEnable(&g_lib);
		fillSteady(fs, 110.0, 0.7f);
		TransientProcess(&g_lib, N);
		check("held tone is left alone (dB)", fabs(rmsChangeDb()), 0.0, 0.5);
	}

	/* ---- attack ----------------------------------------------------------- */
	printf("\nattack\n");
	{
		prepare((float)fs);
		setLow(80.0f, 0.0f, 12.0f);
		TransientEnable(&g_lib);
		fillHits(fs, 110.0, 60.0, 400.0, 0.35f);
		TransientProcess(&g_lib, N);
		check("positive attack raises the hit peak", peakChangeDb(), 1.0, 12.0);
	}
	{
		prepare((float)fs);
		setLow(-80.0f, 0.0f, 12.0f);
		TransientEnable(&g_lib);
		fillHits(fs, 110.0, 60.0, 400.0, 0.35f);
		TransientProcess(&g_lib, N);
		check("negative attack softens it", peakChangeDb(), -12.0, -1.0);
	}

	/* ---- sustain ---------------------------------------------------------- */
	printf("\nsustain\n");
	{
		prepare((float)fs);
		setLow(0.0f, 90.0f, 12.0f);
		TransientEnable(&g_lib);
		fillHits(fs, 110.0, 90.0, 500.0, 0.35f);
		TransientProcess(&g_lib, N);
		check("positive sustain lifts the tail", tailChangeDb(fs, 500.0), 1.0, 12.0);
	}
	{
		prepare((float)fs);
		setLow(0.0f, -90.0f, 12.0f);
		TransientEnable(&g_lib);
		fillHits(fs, 110.0, 90.0, 500.0, 0.35f);
		TransientProcess(&g_lib, N);
		check("negative sustain shortens it", tailChangeDb(fs, 500.0), -12.0, -1.0);
	}

	/* ---- the range clamp holds -------------------------------------------- */
	printf("\nrange clamp\n");
	{
		prepare((float)fs);
		setLow(100.0f, 0.0f, 3.0f);
		TransientEnable(&g_lib);
		fillHits(fs, 110.0, 60.0, 400.0, 0.35f);
		TransientProcess(&g_lib, N);
		check("never exceeds the range", peakChangeDb(), 0.2, 3.6);
	}

	/* ---- it only shapes the range it was told to -------------------------- */
	printf("\nband selectivity\n");
	{
		/* Shaping the top band must leave a low-frequency hit nearly alone. */
		prepare((float)fs);
		TransientSetParam(&g_lib, 200.0f, 3000.0f, 0,0, 0,0, 100.0f,0, 12.0f, 100.0f);
		TransientEnable(&g_lib);
		fillHits(fs, 80.0, 60.0, 400.0, 0.35f);
		TransientProcess(&g_lib, N);
		check("80 Hz hit barely moved by a top-band setting",
		      fabs(peakChangeDb()), 0.0, 1.5);
	}

	/* ---- mix ---------------------------------------------------------------*/
	printf("\nmix\n");
	{
		prepare((float)fs);
		setLow(90.0f, 0.0f, 12.0f);
		TransientSetParam(&g_lib, 200.0f, 3000.0f, 90,0, 0,0, 0,0, 12.0f, 0.0f);
		TransientEnable(&g_lib);
		fillHits(fs, 110.0, 60.0, 400.0, 0.35f);
		TransientProcess(&g_lib, N);
		check("mix 0% is bit-identical", bitIdentical() ? 1.0 : 0.0, 1.0, 1.0);
	}

	/* ---- the defaults the card ships with actually do something ------------ */
	printf("\nshipped defaults\n");
	{
		prepare((float)fs);
		TransientSetParam(&g_lib, 200.0f, 3000.0f,
		                  45.0f, -20.0f, 30.0f, -15.0f, 0.0f, 0.0f, 9.0f, 100.0f);
		TransientEnable(&g_lib);
		fillHits(fs, 110.0, 80.0, 450.0, 0.35f);
		TransientProcess(&g_lib, N);
		const double peak = peakChangeDb();
		const double tail = tailChangeDb(fs, 450.0);
		check("defaults add punch", peak, 0.8, 9.5);
		check("defaults tighten the tail", tail, -9.5, -0.5);
	}

	/* ---- stability -------------------------------------------------------- */
	printf("\nstability\n");
	{
		const double rates[4] = { 44100.0, 48000.0, 96000.0, 192000.0 };
		int bad = 0;
		double peak = 0.0;
		for (int r = 0; r < 4; r++)
		{
			prepare((float)rates[r]);
			TransientSetParam(&g_lib, 200.0f, 3000.0f,
			                  90.0f, 60.0f, 70.0f, -50.0f, -40.0f, 80.0f, 18.0f, 100.0f);
			TransientEnable(&g_lib);
			for (int pass = 0; pass < 6; pass++)
			{
				fillHits(rates[r], 120.0, 50.0, 300.0, 0.9f);
				TransientProcess(&g_lib, N);
				for (int i = 0; i < N; i++)
				{
					if (!isfinite(bufL[i]) || !isfinite(bufR[i])) bad++;
					const double a = fabs((double)bufL[i]);
					if (a > peak) peak = a;
				}
			}
		}
		check("non-finite samples across four rates", (double)bad, 0.0, 0.0);
		check("peak stays bounded", peak, 0.05, 16.0);
	}

	/* ---- silence in, silence out ------------------------------------------ */
	printf("\nsilence\n");
	{
		prepare((float)fs);
		TransientSetParam(&g_lib, 200.0f, 3000.0f, 100,100, 100,100, 100,100, 24.0f, 100.0f);
		TransientEnable(&g_lib);
		memset(bufL, 0, sizeof(bufL));
		memset(bufR, 0, sizeof(bufR));
		keep();
		TransientProcess(&g_lib, N);
		double worst = 0.0;
		for (int i = 0; i < N; i++)
		{
			const double a = fabs((double)bufL[i]);
			if (a > worst) worst = a;
		}
		/* The log floor must not turn digital black into a full boost. */
		check("digital silence stays silent", worst, 0.0, 1e-9);
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
