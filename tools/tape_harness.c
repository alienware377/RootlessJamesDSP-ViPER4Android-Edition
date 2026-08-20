// Does the tape emulation actually wobble, saturate and tilt - and do nothing
// at all when every control is at rest?
//
//   NDK=$LOCALAPPDATA/Android/Sdk/ndk/<ver>/toolchains/llvm/prebuilt/windows-x86_64/bin
//   J=app/src/main/cpp/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp
//   $NDK/aarch64-linux-android29-clang -O2 -w -ffp-contract=off -I $J \
//       tools/tape_harness.c $J/Effects/tape.c tools/lockstub.c -lm -o tape
//
// Wow and flutter are pitch modulation, and the honest way to measure that is
// to show the read position moving: cross-correlate dry against wet in two
// windows some distance apart and compare the lag that fits best. A spectral
// test cannot do it - wow runs at well under a hertz, and resolving sidebands
// that close needs a window longer than the whole test signal.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define NWIN 16384
#define N    (48000 * 4)          /* four seconds, several wow cycles */

static JamesDSPLib g_lib;
static float bufL[N], bufR[N];
static float dryL[N];
static int failures = 0;

static void prepare(float fs)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = fs;
	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
}

static double binFreq(double want, double fs)
{
	double k = floor(want * NWIN / fs + 0.5);
	if (k < 1.0) k = 1.0;
	return k * fs / NWIN;
}

static void fillTone(double freq, double fs, float amp)
{
	for (int i = 0; i < N; i++)
	{
		const double v = amp * sin(2.0 * M_PI * freq * (double)i / fs);
		bufL[i] = bufR[i] = (float)v;
	}
	memcpy(dryL, bufL, sizeof(dryL));
}

/* Broadband, so cross-correlation has a single sharp peak to find. */
static void fillNoise(float amp)
{
	unsigned s = 22222u;
	for (int i = 0; i < N; i++)
	{
		s = s * 1664525u + 1013904223u;
		const float v = amp * ((float)((s >> 9) & 0xFFFF) / 32768.0f - 1.0f);
		bufL[i] = bufR[i] = v;
	}
	memcpy(dryL, bufL, sizeof(dryL));
}

static double toneDb(double freq, double fs, int from)
{
	const double w = 2.0 * M_PI * freq / fs;
	const double coeff = 2.0 * cos(w);
	double s1 = 0.0, s2 = 0.0;
	for (int i = from; i < from + NWIN; i++)
	{
		const double s0 = coeff * s1 - s2 + (double)bufL[i];
		s2 = s1; s1 = s0;
	}
	const double re = s1 - s2 * cos(w), im = s2 * sin(w);
	const double mag = 2.0 * sqrt(re * re + im * im) / (double)NWIN;
	return 20.0 * log10(mag > 1e-12 ? mag : 1e-12);
}

/* Which lag lines the wet signal up with the dry one, around this position. */
static double bestLag(int centre, int span, int maxLag)
{
	double best = -1e30;
	int bestL = 0;
	for (int lag = 0; lag <= maxLag; lag++)
	{
		double acc = 0.0;
		for (int i = centre; i < centre + span; i++)
			acc += (double)dryL[i - lag] * (double)bufL[i];
		if (acc > best) { best = acc; bestL = lag; }
	}
	return (double)bestL;
}

static void check(const char *what, double got, double lo, double hi)
{
	const int ok = got >= lo && got <= hi;
	printf("  %-46s %9.2f  [%8.2f..%8.2f]  %s\n", what, got, lo, hi, ok ? "ok" : "FAIL");
	if (!ok) failures++;
}

static int bitIdentical(void)
{
	for (int i = 0; i < N; i++) if (bufL[i] != dryL[i]) return 0;
	return 1;
}

int main(void)
{
	const double fs = 48000.0;
	printf("== vintage tape ==\n\n");

	/* ---- at rest, nothing at all ------------------------------------------ */
	printf("at rest\n");
	{
		prepare((float)fs);
		TapeSetParam(&g_lib, 0, 0, 0, 0, 0, 100.0f);
		TapeEnable(&g_lib);
		fillTone(binFreq(1000.0, fs), fs, 0.5f);
		TapeProcess(&g_lib, N);
		/* Not merely quiet: the delay line alone would shift everything twelve
		   milliseconds, which is exactly the kind of "transparent" that is not. */
		check("every control at zero is bit-identical", bitIdentical() ? 1.0 : 0.0, 1.0, 1.0);
	}
	{
		prepare((float)fs);
		TapeSetParam(&g_lib, 80, 80, 80, -50, 6, 0.0f);
		TapeEnable(&g_lib);
		fillTone(binFreq(1000.0, fs), fs, 0.5f);
		TapeProcess(&g_lib, N);
		check("mix 0% is bit-identical", bitIdentical() ? 1.0 : 0.0, 1.0, 1.0);
	}

	/* ---- wow moves the read position -------------------------------------- */
	printf("\nwow (the read position must really move)\n");
	{
		prepare((float)fs);
		TapeSetParam(&g_lib, 0, 0, 0, 0, 0, 100.0f);
		g_lib.tape.transparent = 0;          /* force the line, no modulation */
		TapeEnable(&g_lib);
		fillNoise(0.4f);
		TapeProcess(&g_lib, N);
		const double a = bestLag(60000, 8192, 1400);
		const double b = bestLag(150000, 8192, 1400);
		check("with no wow the lag never changes", fabs(a - b), 0.0, 0.5);
	}
	{
		prepare((float)fs);
		TapeSetParam(&g_lib, 100.0f, 0, 0, 0, 0, 100.0f);
		TapeEnable(&g_lib);
		fillNoise(0.4f);
		TapeProcess(&g_lib, N);
		/* 0.7 Hz, so a quarter of a cycle is about 0.36 s. These two windows
		   are 1.9 s apart, well over half a cycle, and full depth is 0.0035s
		   either way - 168 samples of swing at 48k. */
		const double a = bestLag(60000, 8192, 1400);
		const double b = bestLag(150000, 8192, 1400);
		check("with wow the lag moves (samples)", fabs(a - b), 20.0, 400.0);
	}
	{
		prepare((float)fs);
		TapeSetParam(&g_lib, 0, 100.0f, 0, 0, 0, 100.0f);
		TapeEnable(&g_lib);
		fillNoise(0.4f);
		TapeProcess(&g_lib, N);
		/* Flutter is ten times quicker and a tenth as deep, so it is measured
		   over windows a tenth as far apart. */
		const double a = bestLag(60000, 2048, 1400);
		const double b = bestLag(63000, 2048, 1400);
		check("with flutter the lag moves (samples)", fabs(a - b), 1.0, 60.0);
	}

	/* ---- saturation makes harmonics --------------------------------------- */
	printf("\nsaturation\n");
	{
		prepare((float)fs);
		TapeSetParam(&g_lib, 0, 0, 100.0f, 0, 0, 100.0f);
		TapeEnable(&g_lib);
		const double f = binFreq(1000.0, fs);
		fillTone(f, fs, 0.5f);
		TapeProcess(&g_lib, N);
		const double fund = toneDb(f, fs, N - NWIN);
		const double h3 = toneDb(3.0 * f, fs, N - NWIN);
		check("third harmonic appears (dBc)", h3 - fund, -60.0, -10.0);
	}

	/* ---- bias tilts the top ------------------------------------------------ */
	printf("\nbias\n");
	{
		prepare((float)fs);
		TapeSetParam(&g_lib, 0, 0, 0, -100.0f, 0, 100.0f);
		TapeEnable(&g_lib);
		const double f = binFreq(12000.0, fs);
		fillTone(f, fs, 0.4f);
		const double before = toneDb(f, fs, N - NWIN);
		TapeProcess(&g_lib, N);
		/* Brighter, not duller - see the note in tape.c. */
		check("under-bias brightens the top (dB)", toneDb(f, fs, N - NWIN) - before,
		      2.0, 9.0);
	}
	{
		prepare((float)fs);
		TapeSetParam(&g_lib, 0, 0, 0, 100.0f, 0, 100.0f);
		TapeEnable(&g_lib);
		const double f = binFreq(12000.0, fs);
		fillTone(f, fs, 0.4f);
		const double before = toneDb(f, fs, N - NWIN);
		TapeProcess(&g_lib, N);
		check("over-bias dulls it (dB)", toneDb(f, fs, N - NWIN) - before,
		      -9.0, -2.0);
	}

	/* ---- head bump --------------------------------------------------------- */
	printf("\nhead bump\n");
	{
		prepare((float)fs);
		TapeSetParam(&g_lib, 0, 0, 0, 0, 6.0f, 100.0f);
		TapeEnable(&g_lib);
		const double f = binFreq(60.0, fs);
		fillTone(f, fs, 0.4f);
		const double before = toneDb(f, fs, N - NWIN);
		TapeProcess(&g_lib, N);
		check("60 Hz is lifted (dB)", toneDb(f, fs, N - NWIN) - before, 4.0, 7.5);
	}
	{
		prepare((float)fs);
		TapeSetParam(&g_lib, 0, 0, 0, 0, 6.0f, 100.0f);
		TapeEnable(&g_lib);
		const double f = binFreq(1000.0, fs);
		fillTone(f, fs, 0.4f);
		const double before = toneDb(f, fs, N - NWIN);
		TapeProcess(&g_lib, N);
		check("1 kHz left alone by the bump",
		      fabs(toneDb(f, fs, N - NWIN) - before), 0.0, 0.5);
	}

	/* ---- the defaults the card ships with actually do something ------------ */
	printf("\nshipped defaults (wow 25, flutter 30, sat 35, bias -20, bump 3)\n");
	{
		prepare((float)fs);
		TapeSetParam(&g_lib, 25.0f, 30.0f, 35.0f, -20.0f, 3.0f, 100.0f);
		TapeEnable(&g_lib);
		fillNoise(0.4f);
		TapeProcess(&g_lib, N);
		const double a = bestLag(60000, 8192, 1400);
		const double b = bestLag(150000, 8192, 1400);
		check("defaults wobble audibly (samples)", fabs(a - b), 4.0, 200.0);

		prepare((float)fs);
		TapeSetParam(&g_lib, 25.0f, 30.0f, 35.0f, -20.0f, 3.0f, 100.0f);
		TapeEnable(&g_lib);
		const double f = binFreq(60.0, fs);
		fillTone(f, fs, 0.4f);
		const double before = toneDb(f, fs, N - NWIN);
		TapeProcess(&g_lib, N);
		check("defaults add weight at 60 Hz (dB)",
		      toneDb(f, fs, N - NWIN) - before, 1.0, 5.0);
	}

	/* ---- stability --------------------------------------------------------- */
	printf("\nstability\n");
	{
		const double rates[4] = { 44100.0, 48000.0, 96000.0, 192000.0 };
		int bad = 0;
		double peak = 0.0;
		for (int r = 0; r < 4; r++)
		{
			prepare((float)rates[r]);
			TapeSetParam(&g_lib, 100.0f, 100.0f, 100.0f, -100.0f, 9.0f, 100.0f);
			TapeEnable(&g_lib);
			for (int pass = 0; pass < 4; pass++)
			{
				fillNoise(0.9f);
				TapeProcess(&g_lib, N);
				for (int i = 0; i < N; i++)
				{
					if (!isfinite(bufL[i]) || !isfinite(bufR[i])) bad++;
					const double a = fabs((double)bufL[i]);
					if (a > peak) peak = a;
				}
			}
		}
		check("non-finite samples across four rates", (double)bad, 0.0, 0.0);
		check("peak stays bounded", peak, 0.05, 8.0);
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
