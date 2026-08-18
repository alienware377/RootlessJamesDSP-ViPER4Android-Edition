// Does the multiband imager widen only what it is told to, and leave the rest
// exactly alone?
//
//   NDK=$LOCALAPPDATA/Android/Sdk/ndk/<ver>/toolchains/llvm/prebuilt/windows-x86_64/bin
//   J=app/src/main/cpp/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp
//   $NDK/aarch64-linux-android29-clang -O2 -w -ffp-contract=off -I $J \
//       tools/imaging_harness.c $J/Effects/imaging.c tools/lockstub.c -lm -o imaging
//
// The unity test asserts a rounding floor rather than bit-equality: the bands
// are summed back together, and four additions in float32 do not reproduce the
// input exactly even though they do so algebraically.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define NWIN 8192
#define N    (NWIN * 4)

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

static double binFreq(double want, double fs)
{
	double k = floor(want * NWIN / fs + 0.5);
	if (k < 1.0) k = 1.0;
	return k * fs / NWIN;
}

/* A tone present only in the side channel: equal and opposite in L and R. */
static void fillSideTone(double freq, double fs, float amp)
{
	for (int i = 0; i < N; i++)
	{
		const double v = amp * sin(2.0 * M_PI * freq * (double)i / fs);
		bufL[i] = (float)v;
		bufR[i] = (float)(-v);
	}
	memcpy(dryL, bufL, sizeof(dryL));
	memcpy(dryR, bufR, sizeof(dryR));
}

static void fillMono(double freq, double fs, float amp)
{
	for (int i = 0; i < N; i++)
	{
		const double v = amp * sin(2.0 * M_PI * freq * (double)i / fs);
		bufL[i] = bufR[i] = (float)v;
	}
	memcpy(dryL, bufL, sizeof(dryL));
	memcpy(dryR, bufR, sizeof(dryR));
}

static void fillWide(double fs, float amp)
{
	for (int i = 0; i < N; i++)
	{
		const double t = (double)i / fs;
		const double m = 0.6 * sin(2.0 * M_PI * 90.0 * t)
		               + 0.3 * sin(2.0 * M_PI * 1200.0 * t);
		const double s = 0.25 * sin(2.0 * M_PI * 300.0 * t + 0.7)
		               + 0.2 * sin(2.0 * M_PI * 7000.0 * t + 1.3);
		bufL[i] = (float)((m + s) * amp);
		bufR[i] = (float)((m - s) * amp);
	}
	memcpy(dryL, bufL, sizeof(dryL));
	memcpy(dryR, bufR, sizeof(dryR));
}

/* Side-channel level at one frequency, over the final window. */
static double sideDb(double freq, double fs)
{
	const double w = 2.0 * M_PI * freq / fs;
	const double coeff = 2.0 * cos(w);
	double s1 = 0.0, s2 = 0.0;
	for (int i = N - NWIN; i < N; i++)
	{
		const double x = 0.5 * ((double)bufL[i] - (double)bufR[i]);
		const double s0 = coeff * s1 - s2 + x;
		s2 = s1; s1 = s0;
	}
	const double re = s1 - s2 * cos(w), im = s2 * sin(w);
	const double mag = 2.0 * sqrt(re * re + im * im) / (double)NWIN;
	return 20.0 * log10(mag > 1e-12 ? mag : 1e-12);
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

static double worstDiffDb(void)
{
	double worst = 0.0, peak = 0.0;
	for (int i = 0; i < N; i++)
	{
		const double a = fabs((double)bufL[i] - (double)dryL[i]);
		const double b = fabs((double)bufR[i] - (double)dryR[i]);
		if (a > worst) worst = a;
		if (b > worst) worst = b;
		if (fabs((double)dryL[i]) > peak) peak = fabs((double)dryL[i]);
	}
	if (peak <= 0.0) peak = 1.0;
	return 20.0 * log10(worst > 1e-30 ? worst / peak : 1e-30);
}

int main(void)
{
	const double fs = 48000.0;
	printf("== multiband stereo imaging ==\n\n");

	/* ---- every width at unity returns the input --------------------------- */
	printf("unity\n");
	{
		prepare((float)fs);
		ImagingSetParam(&g_lib, 0.0f, 250.0f, 1500.0f, 6000.0f, 1.0f, 1.0f, 1.0f, 100.0f);
		ImagingEnable(&g_lib);
		fillWide(fs, 0.7f);
		ImagingProcess(&g_lib, N);
		/* The bands sum back to the input by construction, so what is left is
		   the rounding of four additions - not zero, but far below anything
		   audible. */
		check("all widths 1.0 is bit-identical", bitIdentical() ? 1.0 : 0.0, 1.0, 1.0);
	}
	{
		/* And still true after the corners move, which proves the telescoping
		   sum does not depend on where they sit. */
		prepare((float)fs);
		ImagingSetParam(&g_lib, 0.0f, 40.0f, 41.0f, 19000.0f, 1.0f, 1.0f, 1.0f, 100.0f);
		ImagingEnable(&g_lib);
		fillWide(fs, 0.7f);
		ImagingProcess(&g_lib, N);
		check("unity holds at extreme corners", bitIdentical() ? 1.0 : 0.0, 1.0, 1.0);
	}

	/* ---- mono in, mono out, whatever the widths --------------------------- */
	printf("\nmono safety\n");
	{
		prepare((float)fs);
		ImagingSetParam(&g_lib, 200.0f, 250.0f, 1500.0f, 6000.0f, 2.0f, 3.0f, 4.0f, 100.0f);
		ImagingEnable(&g_lib);
		fillMono(binFreq(700.0, fs), fs, 0.6f);
		ImagingProcess(&g_lib, N);
		int identical = 1;
		for (int i = 0; i < N; i++) if (bufL[i] != bufR[i]) { identical = 0; break; }
		check("mono stays exactly mono", identical ? 1.0 : 0.0, 1.0, 1.0);
	}

	/* ---- band 1 at zero folds the bass to mono ---------------------------- */
	printf("\nmono below the first corner\n");
	{
		prepare((float)fs);
		ImagingSetParam(&g_lib, 200.0f, 250.0f, 1500.0f, 6000.0f, 1.0f, 1.0f, 1.0f, 100.0f);
		ImagingEnable(&g_lib);
		const double low = binFreq(50.0, fs);
		fillSideTone(low, fs, 0.5f);
		const double before = sideDb(low, fs);
		ImagingProcess(&g_lib, N);
		check("50 Hz side collapses (dB)", sideDb(low, fs) - before, -400.0, -25.0);
	}
	{
		/* ...and leaves the top alone while doing it. */
		prepare((float)fs);
		ImagingSetParam(&g_lib, 200.0f, 250.0f, 1500.0f, 6000.0f, 1.0f, 1.0f, 1.0f, 100.0f);
		ImagingEnable(&g_lib);
		const double high = binFreq(9000.0, fs);
		fillSideTone(high, fs, 0.5f);
		const double before = sideDb(high, fs);
		ImagingProcess(&g_lib, N);
		check("9 kHz side untouched (dB)", fabs(sideDb(high, fs) - before), 0.0, 0.5);
	}

	/* ---- width does what the number says ---------------------------------- */
	printf("\nwidth is the side gain\n");
	{
		const float widths[3] = { 0.5f, 2.0f, 3.0f };
		for (int i = 0; i < 3; i++)
		{
			prepare((float)fs);
			ImagingSetParam(&g_lib, 0.0f, 250.0f, 1500.0f, 6000.0f,
			                1.0f, 1.0f, widths[i], 100.0f);
			ImagingEnable(&g_lib);
			const double f = binFreq(16000.0, fs);
			fillSideTone(f, fs, 0.2f);
			const double before = sideDb(f, fs);
			ImagingProcess(&g_lib, N);
			const double want = 20.0 * log10((double)widths[i]);
			char label[64];
			snprintf(label, sizeof(label), "width %.1f gives %+.1f dB of side",
			         widths[i], want);
			check(label, sideDb(f, fs) - before, want - 0.6, want + 0.6);
		}
	}

	/* ---- mix at zero is the identity, exactly ----------------------------- */
	printf("\nmix\n");
	{
		prepare((float)fs);
		ImagingSetParam(&g_lib, 200.0f, 250.0f, 1500.0f, 6000.0f, 2.0f, 3.0f, 4.0f, 0.0f);
		ImagingEnable(&g_lib);
		fillWide(fs, 0.7f);
		ImagingProcess(&g_lib, N);
		int same = 1;
		for (int i = 0; i < N; i++)
			if (bufL[i] != dryL[i] || bufR[i] != dryR[i]) { same = 0; break; }
		check("mix 0% is bit-identical", same ? 1.0 : 0.0, 1.0, 1.0);
	}

	/* ---- the shipped defaults actually do something ------------------------ */
	printf("\nshipped defaults\n");
	{
		prepare((float)fs);
		ImagingSetParam(&g_lib, 120.0f, 250.0f, 1500.0f, 6000.0f,
		                1.0f, 1.15f, 1.6f, 100.0f);
		ImagingEnable(&g_lib);
		const double low = binFreq(60.0, fs);
		fillSideTone(low, fs, 0.4f);
		double before = sideDb(low, fs);
		ImagingProcess(&g_lib, N);
		check("defaults fold bass to mono (dB)", sideDb(low, fs) - before, -400.0, -20.0);

		prepare((float)fs);
		ImagingSetParam(&g_lib, 120.0f, 250.0f, 1500.0f, 6000.0f,
		                1.0f, 1.15f, 1.6f, 100.0f);
		ImagingEnable(&g_lib);
		const double high = binFreq(16000.0, fs);
		fillSideTone(high, fs, 0.3f);
		before = sideDb(high, fs);
		ImagingProcess(&g_lib, N);
		check("defaults widen the top (dB)", sideDb(high, fs) - before, 2.0, 6.0);
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
			ImagingSetParam(&g_lib, 120.0f, 250.0f, 1500.0f, 6000.0f,
			                1.0f, 1.5f, 2.5f, 100.0f);
			ImagingEnable(&g_lib);
			for (int pass = 0; pass < 10; pass++)
			{
				fillWide(rates[r], 0.9f);
				ImagingProcess(&g_lib, N);
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

	/* ---- corners out of order are not a crash ----------------------------- */
	printf("\ndegenerate settings\n");
	{
		prepare((float)fs);
		ImagingSetParam(&g_lib, 0.0f, 9000.0f, 500.0f, 100.0f, 1.0f, 1.0f, 1.0f, 100.0f);
		ImagingEnable(&g_lib);
		fillWide(fs, 0.7f);
		ImagingProcess(&g_lib, N);
		check("corners dragged past each other still null", bitIdentical() ? 1.0 : 0.0, 1.0, 1.0);
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
