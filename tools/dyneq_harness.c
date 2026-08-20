// Does the dynamic EQ actually move, and only when it should?
//
// Built for arm64 and run on the device, because that is the build that ships
// and single precision is where these things go wrong.
//
//   NDK=$LOCALAPPDATA/Android/Sdk/ndk/<ver>/toolchains/llvm/prebuilt/windows-x86_64/bin
//   J=app/src/main/cpp/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp
//   $NDK/aarch64-linux-android29-clang -O2 -w -ffp-contract=off -I $J \
//       tools/dyneq_harness.c $J/Effects/dynamicEq.c tools/lockstub.c -lm -o dyneq
//
// Measurement note: every probe tone is snapped to an FFT bin of the analysis
// window. An unwindowed Goertzel on a tone that is not bin-centred leaks about
// 50 dB, which is louder than most of what is being measured here.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define NWIN 8192
#define N    (NWIN * 4)

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

/* The nearest frequency that completes a whole number of cycles in the window. */
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
		bufL[i] = (float)v;
		bufR[i] = (float)v;
	}
}

/* Level at one frequency over the final window, by Goertzel. */
static double toneDb(const float *x, double freq, double fs)
{
	const double w = 2.0 * M_PI * freq / fs;
	const double coeff = 2.0 * cos(w);
	double s0 = 0.0, s1 = 0.0, s2 = 0.0;
	for (int i = N - NWIN; i < N; i++)
	{
		s0 = coeff * s1 - s2 + (double)x[i];
		s2 = s1; s1 = s0;
	}
	const double re = s1 - s2 * cos(w);
	const double im = s2 * sin(w);
	const double mag = 2.0 * sqrt(re * re + im * im) / (double)NWIN;
	return 20.0 * log10(mag > 1e-12 ? mag : 1e-12);
}

static void check(const char *what, double got, double lo, double hi)
{
	const int ok = got >= lo && got <= hi;
	printf("  %-46s %8.2f  [%7.2f..%7.2f]  %s\n", what, got, lo, hi, ok ? "ok" : "FAIL");
	if (!ok) failures++;
}

static void setOneBand(float freq, float q, float thr, float ratio,
                       float att, float rel, float range, int mode)
{
	const float b[DYNEQ_VALUES_PER_BAND] =
		{ freq, q, thr, ratio, att, rel, range, (float)mode };
	DynamicEqSetBands(&g_lib, b, 1);
}

int main(void)
{
	const double fs = 48000.0;
	printf("== dynamic EQ ==\n\n");

	/* ---- it does nothing until something crosses the threshold ---------- */
	printf("at rest\n");
	{
		prepare((float)fs);
		setOneBand(3000.0f, 2.0f, -20.0f, 4.0f, 5.0f, 100.0f, -12.0f,
		           DYNEQ_MODE_COMPRESS);
		DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_STEREO);
		DynamicEqEnable(&g_lib);

		const double f = binFreq(3000.0, fs);
		/* -40 dBFS is 20 dB under the threshold, so the band must not act. */
		fillTone(f, fs, 0.01f);
		static float dry[N];
		memcpy(dry, bufL, sizeof(dry));
		DynamicEqProcess(&g_lib, N);
		double worst = 0.0;
		for (int i = 0; i < N; i++)
		{
			const double e = fabs((double)bufL[i] - (double)dry[i]);
			if (e > worst) worst = e;
		}
		printf("  %-46s %.3e\n", "largest sample difference below threshold", worst);
		check("quiet tone passes through (dB error)",
		      20.0 * log10(worst > 1e-12 ? worst / 0.01 : 1e-12), -400.0, -80.0);
	}

	/* ---- the gain law ---------------------------------------------------- */
	printf("\ngain law, 4:1 above -20 dBFS\n");
	{
		/* A tone 10 dB over a -20 dB threshold at 4:1 should come back 7.5 dB
		   down: the excess is divided by the ratio. */
		prepare((float)fs);
		setOneBand(3000.0f, 2.0f, -20.0f, 4.0f, 5.0f, 100.0f, -24.0f,
		           DYNEQ_MODE_COMPRESS);
		DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_STEREO);
		DynamicEqEnable(&g_lib);

		const double f = binFreq(3000.0, fs);
		fillTone(f, fs, powf(10.0f, -10.0f / 20.0f));   /* -10 dBFS */
		const double in = toneDb(bufL, f, fs);
		DynamicEqProcess(&g_lib, N);
		const double out = toneDb(bufL, f, fs);
		check("reduction at 10 dB over threshold", in - out, 5.5, 9.5);
	}

	/* ---- the range clamp holds ------------------------------------------- */
	printf("\nrange clamp\n");
	{
		prepare((float)fs);
		setOneBand(3000.0f, 2.0f, -40.0f, 20.0f, 5.0f, 100.0f, -6.0f,
		           DYNEQ_MODE_COMPRESS);
		DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_STEREO);
		DynamicEqEnable(&g_lib);

		const double f = binFreq(3000.0, fs);
		fillTone(f, fs, 0.7f);          /* far over the threshold */
		const double in = toneDb(bufL, f, fs);
		DynamicEqProcess(&g_lib, N);
		const double out = toneDb(bufL, f, fs);
		/* The law alone would ask for about 32 dB; the range says 6. */
		check("reduction never exceeds the range", in - out, 4.5, 7.5);
	}

	/* ---- it only touches its own band ------------------------------------ */
	printf("\nband selectivity\n");
	{
		prepare((float)fs);
		setOneBand(6800.0f, 3.0f, -40.0f, 8.0f, 1.0f, 40.0f, -12.0f,
		           DYNEQ_MODE_COMPRESS);
		DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_STEREO);
		DynamicEqEnable(&g_lib);

		const double far = binFreq(200.0, fs);
		fillTone(far, fs, 0.5f);
		const double in = toneDb(bufL, far, fs);
		DynamicEqProcess(&g_lib, N);
		const double out = toneDb(bufL, far, fs);
		check("200 Hz untouched by a 6.8 kHz band", fabs(in - out), 0.0, 0.5);
	}

	/* ---- expansion is the mirror of compression -------------------------- */
	printf("\nexpander mode\n");
	{
		prepare((float)fs);
		setOneBand(1000.0f, 1.5f, -20.0f, 3.0f, 5.0f, 100.0f, 6.0f,
		           DYNEQ_MODE_EXPAND);
		DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_STEREO);
		DynamicEqEnable(&g_lib);

		const double f = binFreq(1000.0, fs);
		fillTone(f, fs, powf(10.0f, -30.0f / 20.0f));   /* 10 dB under */
		const double in = toneDb(bufL, f, fs);
		DynamicEqProcess(&g_lib, N);
		const double out = toneDb(bufL, f, fs);
		check("quiet tone is lifted", out - in, 4.0, 7.5);
	}

	/* ---- the defaults the card ships with actually do something ---------- */
	printf("\nshipped defaults\n");
	{
		prepare((float)fs);
		const float defaults[3 * DYNEQ_VALUES_PER_BAND] = {
			180.0f, 1.0f, -22.0f, 3.0f, 15.0f, 150.0f, -6.0f, (float)DYNEQ_MODE_COMPRESS,
			3200.0f, 1.4f, -26.0f, 3.0f,  3.0f,  80.0f, -5.0f, (float)DYNEQ_MODE_COMPRESS,
			6800.0f, 3.0f, -30.0f, 4.0f,  1.0f,  40.0f, -8.0f, (float)DYNEQ_MODE_COMPRESS
		};
		DynamicEqSetBands(&g_lib, defaults, 3);
		DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_STEREO);
		DynamicEqEnable(&g_lib);

		/* One tone per default band, each loud enough to cross its threshold,
		   measured on its own frequency - a broadband difference would let a
		   dead band hide behind a live one. */
		const double f1 = binFreq(180.0, fs);
		fillTone(f1, fs, powf(10.0f, -7.0f / 20.0f));
		double in1 = toneDb(bufL, f1, fs);
		DynamicEqProcess(&g_lib, N);
		check("default band 1 (180 Hz) acts", in1 - toneDb(bufL, f1, fs), 1.0, 8.0);

		prepare((float)fs);
		DynamicEqSetBands(&g_lib, defaults, 3);
		DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_STEREO);
		DynamicEqEnable(&g_lib);
		const double f3 = binFreq(6800.0, fs);
		fillTone(f3, fs, powf(10.0f, -14.0f / 20.0f));
		double in3 = toneDb(bufL, f3, fs);
		DynamicEqProcess(&g_lib, N);
		check("default band 3 (6.8 kHz) acts", in3 - toneDb(bufL, f3, fs), 1.0, 10.0);
	}

	/* ---- mix at zero is the identity ------------------------------------- */
	printf("\nmix\n");
	{
		prepare((float)fs);
		setOneBand(3000.0f, 2.0f, -40.0f, 8.0f, 1.0f, 40.0f, -12.0f,
		           DYNEQ_MODE_COMPRESS);
		DynamicEqSetParam(&g_lib, 0.0f, MS_MODE_STEREO);
		DynamicEqEnable(&g_lib);
		const double f = binFreq(3000.0, fs);
		fillTone(f, fs, 0.5f);
		static float dry[N];
		memcpy(dry, bufL, sizeof(dry));
		DynamicEqProcess(&g_lib, N);
		int same = 1;
		for (int i = 0; i < N; i++) if (bufL[i] != dry[i]) { same = 0; break; }
		check("mix 0% is bit-identical", same ? 1.0 : 0.0, 1.0, 1.0);
	}

	/* ---- stability ------------------------------------------------------- */
	printf("\nstability\n");
	{
		const double rates[4] = { 44100.0, 48000.0, 96000.0, 192000.0 };
		int bad = 0;
		double peak = 0.0;
		for (int r = 0; r < 4; r++)
		{
			prepare((float)rates[r]);
			const float defaults[2 * DYNEQ_VALUES_PER_BAND] = {
				120.0f, 1.0f, -30.0f, 6.0f, 1.0f, 30.0f, -12.0f, (float)DYNEQ_MODE_COMPRESS,
				9000.0f, 4.0f, -35.0f, 8.0f, 1.0f, 20.0f, 8.0f, (float)DYNEQ_MODE_EXPAND
			};
			DynamicEqSetBands(&g_lib, defaults, 2);
			DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_STEREO);
			DynamicEqEnable(&g_lib);
			for (int pass = 0; pass < 12; pass++)
			{
				for (int i = 0; i < N; i++)
				{
					const double t = (double)(pass * N + i) / rates[r];
					const double v = 0.7 * sin(2.0 * M_PI * 110.0 * t)
					               + 0.2 * sin(2.0 * M_PI * 9000.0 * t)
					               + 0.05 * ((double)rand() / RAND_MAX - 0.5);
					bufL[i] = (float)v;
					bufR[i] = (float)(v * 0.9);
				}
				DynamicEqProcess(&g_lib, N);
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

	/* ---- mid and side modes ---------------------------------------------
	   A band can be pointed at the centre of the image or at its edges. What
	   makes this worth having is that the untouched half really is untouched:
	   a de-esser on the centre must not dull the reverb spread around it. */
	printf("\nmid / side\n");
	{
		/* A tone living only in the side, with the band pointed at the mid,
		   must come back as it went in - there is nothing there to work on. */
		prepare((float)fs);
		setOneBand(3000.0f, 2.0f, -40.0f, 8.0f, 1.0f, 40.0f, -18.0f,
		           DYNEQ_MODE_COMPRESS);
		DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_MID);
		DynamicEqEnable(&g_lib);
		const double f = binFreq(3000.0, fs);
		for (int i = 0; i < N; i++)
		{
			const double v = 0.5 * sin(2.0 * M_PI * f * (double)i / fs);
			bufL[i] = (float)v;
			bufR[i] = (float)(-v);
		}
		const double before = toneDb(bufL, f, fs);
		DynamicEqProcess(&g_lib, N);
		check("side-only tone ignored by a mid band",
		      fabs(toneDb(bufL, f, fs) - before), 0.0, 0.3);
	}
	{
		/* The mirror image: same tone, band pointed at the side, is caught. */
		prepare((float)fs);
		setOneBand(3000.0f, 2.0f, -40.0f, 8.0f, 1.0f, 40.0f, -18.0f,
		           DYNEQ_MODE_COMPRESS);
		DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_SIDE);
		DynamicEqEnable(&g_lib);
		const double f = binFreq(3000.0, fs);
		for (int i = 0; i < N; i++)
		{
			const double v = 0.5 * sin(2.0 * M_PI * f * (double)i / fs);
			bufL[i] = (float)v;
			bufR[i] = (float)(-v);
		}
		const double before = toneDb(bufL, f, fs);
		DynamicEqProcess(&g_lib, N);
		check("side-only tone caught by a side band",
		      toneDb(bufL, f, fs) - before, -20.0, -3.0);
	}
	{
		/* A centred tone is the other way round. */
		prepare((float)fs);
		setOneBand(3000.0f, 2.0f, -40.0f, 8.0f, 1.0f, 40.0f, -18.0f,
		           DYNEQ_MODE_COMPRESS);
		DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_SIDE);
		DynamicEqEnable(&g_lib);
		const double f = binFreq(3000.0, fs);
		fillTone(f, fs, 0.5f);
		const double before = toneDb(bufL, f, fs);
		DynamicEqProcess(&g_lib, N);
		check("centred tone ignored by a side band",
		      fabs(toneDb(bufL, f, fs) - before), 0.0, 0.3);
	}
	{
		/* Mono in stays mono out: mid mode has no side to put back, so the two
		   channels cannot drift apart. */
		prepare((float)fs);
		setOneBand(3000.0f, 2.0f, -40.0f, 8.0f, 1.0f, 40.0f, -18.0f,
		           DYNEQ_MODE_COMPRESS);
		DynamicEqSetParam(&g_lib, 100.0f, MS_MODE_MID);
		DynamicEqEnable(&g_lib);
		fillTone(binFreq(3000.0, fs), fs, 0.5f);
		DynamicEqProcess(&g_lib, N);
		int mono = 1;
		for (int i = 0; i < N; i++) if (bufL[i] != bufR[i]) { mono = 0; break; }
		check("mono stays exactly mono in mid mode", mono ? 1.0 : 0.0, 1.0, 1.0);
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
