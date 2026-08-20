// Does the exciter actually generate harmonics, in the right places, and
// nothing at all when it is idle?
//
//   NDK=$LOCALAPPDATA/Android/Sdk/ndk/<ver>/toolchains/llvm/prebuilt/windows-x86_64/bin
//   J=app/src/main/cpp/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp
//   $NDK/aarch64-linux-android29-clang -O2 -w -ffp-contract=off -I $J \
//       tools/exciter_harness.c $J/Effects/exciter.c tools/lockstub.c -lm -o exciter
//
// Every probe tone is snapped to an FFT bin. That matters more here than
// anywhere else in this project: an unwindowed Goertzel on an off-bin tone
// leaks about 50 dB, and the harmonics being measured sit well below that, so
// off-bin probing would measure leakage from the fundamental rather than the
// distortion under test.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define NWIN 16384
#define N    (NWIN * 3)

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

static double toneDb(double freq, double fs)
{
	const double w = 2.0 * M_PI * freq / fs;
	const double coeff = 2.0 * cos(w);
	double s1 = 0.0, s2 = 0.0;
	for (int i = N - NWIN; i < N; i++)
	{
		const double s0 = coeff * s1 - s2 + (double)bufL[i];
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
	for (int i = 0; i < N; i++) if (bufL[i] != dryL[i]) return 0;
	return 1;
}

/* Excite one band only, at a given character, and report the harmonics of a
   tone placed inside that band. */
static void runOne(int character, double toneWant, double fs, float amp,
                   float a1, float a2, float a3, float a4, float drive,
                   double *fund, double *h2, double *h3)
{
	prepare((float)fs);
	ExciterSetParam(&g_lib, 150.0f, 900.0f, 4500.0f, a1, a2, a3, a4,
	                character, drive, 100.0f);
	ExciterEnable(&g_lib);
	const double f = binFreq(toneWant, fs);
	fillTone(f, fs, amp);
	ExciterProcess(&g_lib, N);
	*fund = toneDb(f, fs);
	*h2 = toneDb(2.0 * f, fs);
	*h3 = toneDb(3.0 * f, fs);
}

int main(void)
{
	const double fs = 48000.0;
	printf("== exciter ==\n\n");

	/* ---- idle is exactly idle --------------------------------------------- */
	printf("idle\n");
	{
		prepare((float)fs);
		ExciterSetParam(&g_lib, 150.0f, 900.0f, 4500.0f, 0,0,0,0,
		                EXCITER_TUBE, 6.0f, 100.0f);
		ExciterEnable(&g_lib);
		fillTone(binFreq(400.0, fs), fs, 0.5f);
		ExciterProcess(&g_lib, N);
		check("every amount at zero is bit-identical", bitIdentical() ? 1.0 : 0.0, 1.0, 1.0);
	}
	{
		prepare((float)fs);
		ExciterSetParam(&g_lib, 150.0f, 900.0f, 4500.0f, 80,80,80,80,
		                EXCITER_TUBE, 6.0f, 0.0f);
		ExciterEnable(&g_lib);
		fillTone(binFreq(400.0, fs), fs, 0.5f);
		ExciterProcess(&g_lib, N);
		check("mix 0% is bit-identical", bitIdentical() ? 1.0 : 0.0, 1.0, 1.0);
	}

	/* ---- it makes harmonics ----------------------------------------------- */
	printf("\nharmonics are generated\n");
	{
		double f, h2, h3;
		/* A 400 Hz tone sits in band 2 (150..900), so band 2 is the one driven. */
		runOne(EXCITER_WARM, 400.0, fs, 0.5f, 0, 80, 0, 0, 8.0f, &f, &h2, &h3);
		check("warm: third harmonic present (dBc)", h3 - f, -70.0, -12.0);
	}
	{
		double f, h2, h3;
		runOne(EXCITER_TRIODE, 400.0, fs, 0.5f, 0, 80, 0, 0, 8.0f, &f, &h2, &h3);
		check("triode: second harmonic present (dBc)", h2 - f, -70.0, -8.0);
	}

	/* ---- the characters really are different ------------------------------ */
	printf("\ncharacters differ in the way they claim\n");
	{
		/* Warm, retro and tape are symmetric, so they make odd harmonics and
		   almost no second. Tube and triode are lopsided and make a second.
		   That distinction is the whole reason to offer both kinds. */
		double f, h2, h3;
		double evenOdd[EXCITER_CHAR_COUNT];
		const char *names[EXCITER_CHAR_COUNT] =
			{ "warm", "retro", "tape", "tube", "triode" };
		for (int c = 0; c < EXCITER_CHAR_COUNT; c++)
		{
			runOne(c, 400.0, fs, 0.5f, 0, 80, 0, 0, 8.0f, &f, &h2, &h3);
			evenOdd[c] = h2 - h3;
			printf("    %-8s  2nd %7.2f dBc   3rd %7.2f dBc\n",
			       names[c], h2 - f, h3 - f);
		}
		check("triode is more even-harmonic than warm",
		      evenOdd[EXCITER_TRIODE] - evenOdd[EXCITER_WARM], 6.0, 200.0);
		check("tube is more even-harmonic than retro",
		      evenOdd[EXCITER_TUBE] - evenOdd[EXCITER_RETRO], 6.0, 200.0);
	}

	/* ---- amount is a level control, not an on/off -------------------------- */
	printf("\namount scales what is added\n");
	{
		double f1, h2a, h3a, f2, h2b, h3b;
		runOne(EXCITER_WARM, 400.0, fs, 0.5f, 0, 20, 0, 0, 8.0f, &f1, &h2a, &h3a);
		runOne(EXCITER_WARM, 400.0, fs, 0.5f, 0, 80, 0, 0, 8.0f, &f2, &h2b, &h3b);
		/* Four times the amount is four times the added signal: about 12 dB. */
		check("4x amount is about 12 dB more harmonic", h3b - h3a, 8.0, 16.0);
	}

	/* ---- bands are independent -------------------------------------------- */
	printf("\nband selectivity\n");
	{
		/* Drive only the top band; a 400 Hz tone lives in band 2 and must come
		   back very close to untouched. */
		double f, h2, h3;
		prepare((float)fs);
		ExciterSetParam(&g_lib, 150.0f, 900.0f, 4500.0f, 0, 0, 0, 100.0f,
		                EXCITER_TRIODE, 12.0f, 100.0f);
		ExciterEnable(&g_lib);
		const double t = binFreq(400.0, fs);
		fillTone(t, fs, 0.5f);
		const double before = toneDb(t, fs);
		ExciterProcess(&g_lib, N);
		check("400 Hz barely moved by a top-band setting",
		      fabs(toneDb(t, fs) - before), 0.0, 0.6);
	}

	/* ---- no DC left behind -------------------------------------------------
	   The asymmetric characters rectify, and four bands of that would stack up
	   into an offset that eats headroom and thumps on every enable. */
	printf("\nDC\n");
	{
		prepare((float)fs);
		ExciterSetParam(&g_lib, 150.0f, 900.0f, 4500.0f, 100,100,100,100,
		                EXCITER_TRIODE, 15.0f, 100.0f);
		ExciterEnable(&g_lib);
		fillTone(binFreq(400.0, fs), fs, 0.6f);
		ExciterProcess(&g_lib, N);
		double sum = 0.0;
		for (int i = N - NWIN; i < N; i++) sum += (double)bufL[i];
		const double dc = fabs(sum / (double)NWIN);
		check("mean offset stays at zero", 20.0 * log10(dc > 1e-12 ? dc : 1e-12),
		      -400.0, -60.0);
	}

	/* ---- the defaults the card ships with actually do something ------------ */
	printf("\nshipped defaults\n");
	{
		double f, h2, h3;
		runOne(EXCITER_TUBE, 60.0, fs, 0.5f, 30.0f, 12.0f, 18.0f, 35.0f, 6.0f,
		       &f, &h2, &h3);
		check("defaults excite the bottom (2nd, dBc)", h2 - f, -70.0, -14.0);
		runOne(EXCITER_TUBE, 8000.0, fs, 0.35f, 30.0f, 12.0f, 18.0f, 35.0f, 6.0f,
		       &f, &h2, &h3);
		check("defaults excite the top (2nd, dBc)", h2 - f, -70.0, -14.0);
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
			ExciterSetParam(&g_lib, 150.0f, 900.0f, 4500.0f, 100,100,100,100,
			                EXCITER_TRIODE, 20.0f, 100.0f);
			ExciterEnable(&g_lib);
			for (int pass = 0; pass < 6; pass++)
			{
				for (int i = 0; i < N; i++)
				{
					const double t = (double)(pass * N + i) / rates[r];
					const double v = 0.6 * sin(2.0 * M_PI * 70.0 * t)
					               + 0.3 * sin(2.0 * M_PI * 1000.0 * t)
					               + 0.2 * sin(2.0 * M_PI * 9000.0 * t);
					bufL[i] = (float)v;
					bufR[i] = (float)(v * 0.9);
				}
				ExciterProcess(&g_lib, N);
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
