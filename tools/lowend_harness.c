// Does the low-end card do its three jobs, and only in the places it should?
//
//   NDK=$LOCALAPPDATA/Android/Sdk/ndk/<ver>/toolchains/llvm/prebuilt/windows-x86_64/bin
//   J=app/src/main/cpp/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp
//   $NDK/aarch64-linux-android29-clang -O2 -w -ffp-contract=off -I $J \
//       tools/lowend_harness.c $J/Effects/lowend.c tools/lockstub.c -lm -o lowend
//
// Every probe tone is snapped to an FFT bin of the analysis window. Unwindowed
// Goertzel on an off-bin tone leaks about 50 dB, which is louder than most of
// what is being measured here.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define NWIN 16384
#define N    (NWIN * 4)

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

static double toneDb(const float *x, double freq, double fs)
{
	const double w = 2.0 * M_PI * freq / fs;
	const double coeff = 2.0 * cos(w);
	double s1 = 0.0, s2 = 0.0;
	for (int i = N - NWIN; i < N; i++)
	{
		const double s0 = coeff * s1 - s2 + (double)x[i];
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

/* Level change at one frequency, in dB, for the current settings. */
static double responseAt(double want, double fs, float subsonic,
                         float weightHz, float weightDb,
                         float mudHz, float mudDb)
{
	prepare((float)fs);
	LowEndSetParam(&g_lib, subsonic, weightHz, weightDb, mudHz, mudDb, 100.0f);
	LowEndEnable(&g_lib);
	const double f = binFreq(want, fs);
	fillTone(f, fs, 0.3f);
	const double before = toneDb(bufL, f, fs);
	LowEndProcess(&g_lib, N);
	return toneDb(bufL, f, fs) - before;
}

int main(void)
{
	const double fs = 48000.0;
	printf("== low end ==\n\n");

	/* ---- neutral does nothing at all -------------------------------------- */
	printf("neutral\n");
	{
		prepare((float)fs);
		LowEndSetParam(&g_lib, 0.0f, 90.0f, 0.0f, 300.0f, 0.0f, 100.0f);
		LowEndEnable(&g_lib);
		fillTone(binFreq(1000.0, fs), fs, 0.5f);
		LowEndProcess(&g_lib, N);
		int same = 1;
		for (int i = 0; i < N; i++) if (bufL[i] != dryL[i]) { same = 0; break; }
		check("everything off is bit-identical", same ? 1.0 : 0.0, 1.0, 1.0);
	}

	/* ---- subsonic ---------------------------------------------------------- */
	printf("\nsubsonic filter at 30 Hz\n");
	{
		/* Fourth order is 24 dB per octave, so an octave down should be well
		   past 20 dB of rejection. */
		check("15 Hz rejected", responseAt(15.0, fs, 30.0f, 90.0f, 0.0f, 300.0f, 0.0f),
		      -80.0, -18.0);
		check("30 Hz is the corner (about -3 dB)",
		      responseAt(30.0, fs, 30.0f, 90.0f, 0.0f, 300.0f, 0.0f), -5.0, -1.5);
		check("100 Hz untouched",
		      fabs(responseAt(100.0, fs, 30.0f, 90.0f, 0.0f, 300.0f, 0.0f)), 0.0, 0.6);
		check("1 kHz untouched",
		      fabs(responseAt(1000.0, fs, 30.0f, 90.0f, 0.0f, 300.0f, 0.0f)), 0.0, 0.1);
	}

	/* ---- weight ------------------------------------------------------------ */
	printf("\nweight shelf\n");
	{
		/* A shelf reaches its full gain well below the corner. */
		check("+6 dB lifts 30 Hz", responseAt(30.0, fs, 0.0f, 90.0f, 6.0f, 300.0f, 0.0f),
		      5.0, 6.6);
		check("-6 dB drops it", responseAt(30.0, fs, 0.0f, 90.0f, -6.0f, 300.0f, 0.0f),
		      -6.6, -5.0);
		check("2 kHz left alone",
		      fabs(responseAt(2000.0, fs, 0.0f, 90.0f, 6.0f, 300.0f, 0.0f)), 0.0, 0.3);
	}

	/* ---- mud --------------------------------------------------------------- */
	printf("\nmud dip\n");
	{
		check("-6 dB at 300 Hz", responseAt(300.0, fs, 0.0f, 90.0f, 0.0f, 300.0f, -6.0f),
		      -6.6, -5.4);
		/* Wide by design, so an octave out is still touched - but only a little,
		   and two octaves out barely at all. */
		check("40 Hz mostly spared",
		      fabs(responseAt(40.0, fs, 0.0f, 90.0f, 0.0f, 300.0f, -6.0f)), 0.0, 1.2);
		check("4 kHz mostly spared",
		      fabs(responseAt(4000.0, fs, 0.0f, 90.0f, 0.0f, 300.0f, -6.0f)), 0.0, 1.2);
	}

	/* ---- the three do not fight each other -------------------------------- */
	printf("\nall three together\n");
	{
		const double atSub = responseAt(15.0, fs, 30.0f, 90.0f, 6.0f, 300.0f, -6.0f);
		check("subsonic still wins below its corner", atSub, -80.0, -12.0);
		const double at1k = responseAt(1000.0, fs, 30.0f, 90.0f, 6.0f, 300.0f, -6.0f);
		check("1 kHz barely moved by any of them", fabs(at1k), 0.0, 1.0);
	}

	/* ---- mix --------------------------------------------------------------- */
	printf("\nmix\n");
	{
		prepare((float)fs);
		LowEndSetParam(&g_lib, 30.0f, 90.0f, 6.0f, 300.0f, -6.0f, 0.0f);
		LowEndEnable(&g_lib);
		fillTone(binFreq(60.0, fs), fs, 0.5f);
		LowEndProcess(&g_lib, N);
		int same = 1;
		for (int i = 0; i < N; i++) if (bufL[i] != dryL[i]) { same = 0; break; }
		check("mix 0% is bit-identical", same ? 1.0 : 0.0, 1.0, 1.0);
	}

	/* ---- the defaults the card ships with actually do something ------------ */
	printf("\nshipped defaults (30 Hz, +3 dB @ 90, -2.5 dB @ 300)\n");
	{
		check("rumble at 15 Hz is removed",
		      responseAt(15.0, fs, 30.0f, 90.0f, 3.0f, 300.0f, -2.5f), -80.0, -12.0);
		check("body added at 50 Hz",
		      responseAt(50.0, fs, 30.0f, 90.0f, 3.0f, 300.0f, -2.5f), 0.8, 4.0);
		check("thickness eased at 300 Hz",
		      responseAt(300.0, fs, 30.0f, 90.0f, 3.0f, 300.0f, -2.5f), -3.2, -1.0);
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
			LowEndSetParam(&g_lib, 40.0f, 90.0f, 9.0f, 300.0f, -9.0f, 100.0f);
			LowEndEnable(&g_lib);
			for (int pass = 0; pass < 8; pass++)
			{
				for (int i = 0; i < N; i++)
				{
					const double t = (double)(pass * N + i) / rates[r];
					const double v = 0.6 * sin(2.0 * M_PI * 45.0 * t)
					               + 0.3 * sin(2.0 * M_PI * 300.0 * t)
					               + 0.1 * sin(2.0 * M_PI * 8.0 * t);   /* rumble */
					bufL[i] = (float)v;
					bufR[i] = (float)(v * 0.9);
				}
				LowEndProcess(&g_lib, N);
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
