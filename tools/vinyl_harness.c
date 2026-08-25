// Does the vinyl card actually make the noises it advertises, and none at all
// when every control is at rest?
//
//   NDK=$LOCALAPPDATA/Android/Sdk/ndk/<ver>/toolchains/llvm/prebuilt/windows-x86_64/bin
//   J=app/src/main/cpp/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp
//   $NDK/aarch64-linux-android29-clang -O2 -w -ffp-contract=off -I $J \
//       tools/vinyl_harness.c $J/Effects/vinyl.c tools/lockstub.c -lm -o vinyl
//
// This effect is unlike the others here: it ADDS a signal rather than filtering
// one, so the usual "measure the response" approach does not apply. What is
// measured instead is what arrives in silence - feed it digital zero and every
// noise source shows up unmasked, at its own level and in its own band.
//
// The transparency check therefore matters more here than anywhere else. An
// effect that adds a bed cannot be "close enough" to silent at rest, because
// its whole job is to be audible; if the early return were ever lost, every
// user would get a permanently noisy output with no control that stops it.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define FS   48000.0
#define N    (48000 * 4)

static JamesDSPLib g_lib;
static float bufL[N], bufR[N];
static int failures = 0;

static void prepare(void)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = (float)FS;
	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
}

static void silence(void) { memset(bufL, 0, sizeof(bufL)); memset(bufR, 0, sizeof(bufR)); }

static void tone(double f, float amp)
{
	for (int i = 0; i < N; i++)
		bufL[i] = bufR[i] = (float)(amp * sin(2.0 * M_PI * f * (double)i / FS));
}

static double rms(void)
{
	double a = 0.0;
	for (int i = 0; i < N; i++) a += (double)bufL[i] * (double)bufL[i];
	return sqrt(a / (double)N);
}

static double peak(void)
{
	double m = 0.0;
	for (int i = 0; i < N; i++) { const double v = fabs((double)bufL[i]); if (v > m) m = v; }
	return m;
}

/* How many samples stand well above the noise floor - the signature of
   impulsive events rather than a continuous bed. */
static int events(double threshold)
{
	int n = 0;
	for (int i = 1; i < N; i++)
		if (fabs((double)bufL[i]) > threshold && fabs((double)bufL[i - 1]) <= threshold) n++;
	return n;
}

/* Rough spectral centroid via one-pole difference energy: high numbers mean the
   energy sits high in the band. Enough to tell a click from a rumble. */
static double brightness(void)
{
	double lo = 0.0, hi = 0.0, prev = 0.0;
	for (int i = 0; i < N; i++)
	{
		const double x = bufL[i];
		hi += (x - prev) * (x - prev);
		lo += x * x;
		prev = x;
	}
	return lo > 0.0 ? hi / lo : 0.0;
}

static void check(const char *what, double got, double lo, double hi)
{
	const int ok = got >= lo && got <= hi;
	printf("  %-50s %10.4f  [%9.4f..%9.4f]  %s\n", what, got, lo, hi, ok ? "ok" : "FAIL");
	if (!ok) failures++;
}

/* every control at zero */
static void allOff(void) { VinylSetParam(&g_lib, 0,0,0,0,0,0,0,0,0,0,0, 100.0f); }

int main(void)
{
	printf("== vintage vinyl ==\n\n");

	printf("at rest\n");
	{
		prepare(); allOff(); VinylEnable(&g_lib);
		tone(440.0, 0.5f);
		float before[64]; memcpy(before, bufL, sizeof(before));
		VinylProcess(&g_lib, N);
		int identical = memcmp(before, bufL, sizeof(before)) == 0;
		check("every control at zero is bit-identical", identical ? 1 : 0, 1, 1);
	}
	{
		prepare();
		VinylSetParam(&g_lib, 80,80,80,80,80,80,80,80,80, 6.0f, 0, 0.0f);
		VinylEnable(&g_lib);
		silence();
		VinylProcess(&g_lib, N);
		check("mix 0% adds nothing", rms(), 0.0, 1e-9);
	}

	printf("\neach source produces something, alone\n");
	{
		const struct { const char *name; int idx; } srcs[] = {
			{ "surface", 0 }, { "crackle", 1 }, { "pops", 3 }, { "clicks", 4 },
			{ "sizzle", 5 }, { "hiss", 6 }, { "prickle", 7 }, { "rumble", 8 }
		};
		for (unsigned s = 0; s < sizeof(srcs)/sizeof(srcs[0]); s++)
		{
			float v[9] = {0,0,0,0,0,0,0,0,0};
			v[srcs[s].idx] = 70.0f;
			if (srcs[s].idx == 1) v[2] = 60.0f;   /* crackle needs a size */
			prepare();
			VinylSetParam(&g_lib, v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],
			              0.0f, 0.0f, 100.0f);
			VinylEnable(&g_lib);
			silence();
			VinylProcess(&g_lib, N);
			char buf[96];
			snprintf(buf, sizeof(buf), "%s alone is audible", srcs[s].name);
			check(buf, 20.0 * log10(rms() > 1e-12 ? rms() : 1e-12), -80.0, -3.0);
		}
	}

	printf("\nthe sources are different from each other\n");
	{
		/* Rumble must sit far below clicks in the spectrum, or the two controls
		   are doing the same thing under different names. */
		prepare();
		VinylSetParam(&g_lib, 0,0,0,0,0,0,0,0, 90.0f, 0,0, 100.0f);
		VinylEnable(&g_lib); silence(); VinylProcess(&g_lib, N);
		const double rumbleBright = brightness();

		prepare();
		VinylSetParam(&g_lib, 0,0,0,0, 90.0f, 0,0,0,0, 0,0, 100.0f);
		VinylEnable(&g_lib); silence(); VinylProcess(&g_lib, N);
		const double clickBright = brightness();

		printf("      rumble %.5f vs clicks %.5f\n", rumbleBright, clickBright);
		check("clicks are far brighter than rumble", clickBright / (rumbleBright + 1e-9), 5.0, 1e9);
	}
	{
		/* Pops are rare and loud; crackle is frequent and small. If the event
		   counts came out the same way round the two controls would be
		   indistinguishable in use. */
		prepare();
		VinylSetParam(&g_lib, 0, 90.0f, 40.0f, 0,0,0,0,0,0, 0,0, 100.0f);
		VinylEnable(&g_lib); silence(); VinylProcess(&g_lib, N);
		const int crackleCount = events(0.01);

		prepare();
		VinylSetParam(&g_lib, 0,0,0, 90.0f, 0,0,0,0,0, 0,0, 100.0f);
		VinylEnable(&g_lib); silence(); VinylProcess(&g_lib, N);
		const int popCount = events(0.01);

		printf("      crackle %d events, pops %d over 4 s\n", crackleCount, popCount);
		check("crackle is far more frequent than pops",
		      (double)crackleCount / (double)(popCount + 1), 5.0, 1e6);
		check("pops still happen at all", (double)popCount, 1.0, 100.0);
	}

	printf("\nwear is the only control that touches the music\n");
	{
		prepare();
		VinylSetParam(&g_lib, 0,0,0,0,0,0,0,0,0, 12.0f, 0.0f, 100.0f);
		VinylEnable(&g_lib);
		tone(12000.0, 0.4f);
		const double before = rms();
		VinylProcess(&g_lib, N);
		const double after = rms();
		check("12 dB wear dulls 12 kHz (dB)", 20.0 * log10(after / before), -14.0, -4.0);
	}
	{
		prepare();
		VinylSetParam(&g_lib, 0,0,0,0,0,0,0,0,0, 12.0f, 0.0f, 100.0f);
		VinylEnable(&g_lib);
		tone(200.0, 0.4f);
		const double before = rms();
		VinylProcess(&g_lib, N);
		check("200 Hz is left alone by wear (dB)",
		      fabs(20.0 * log10(rms() / before)), 0.0, 0.6);
	}

	printf("\nfollow tracks the programme\n");
	{
		/* With follow up, a silent input must stay far quieter than a loud one:
		   that is the entire point of the control. */
		prepare();
		VinylSetParam(&g_lib, 70.0f, 70.0f, 50.0f, 0,0,0, 40.0f, 0,0, 0.0f, 100.0f, 100.0f);
		VinylEnable(&g_lib);
		silence();
		VinylProcess(&g_lib, N);
		const double quiet = rms();

		prepare();
		VinylSetParam(&g_lib, 70.0f, 70.0f, 50.0f, 0,0,0, 40.0f, 0,0, 0.0f, 100.0f, 100.0f);
		VinylEnable(&g_lib);
		tone(440.0, 0.5f);
		VinylProcess(&g_lib, N);
		/* Subtract the tone's own energy: what is left is the bed. */
		double resid = 0.0;
		for (int i = 0; i < N; i++)
		{
			const double t = 0.5 * sin(2.0 * M_PI * 440.0 * (double)i / FS);
			resid += ((double)bufL[i] - t) * ((double)bufL[i] - t);
		}
		const double loud = sqrt(resid / (double)N);
		printf("      bed in silence %.6f, under music %.6f\n", quiet, loud);
		check("the bed is much quieter in silence", loud / (quiet + 1e-9), 4.0, 1e9);
	}

	printf("\nstability\n");
	{
		const double rates[2] = { 44100.0, 48000.0 };
		int bad = 0; double pk = 0.0;
		for (int r = 0; r < 2; r++)
		{
			prepare();
			g_lib.fs = (float)rates[r];
			VinylSetParam(&g_lib, 100,100,100,100,100,100,100,100,100, 24.0f, 0.0f, 100.0f);
			VinylEnable(&g_lib);
			for (int pass = 0; pass < 3; pass++)
			{
				tone(220.0, 0.8f);
				VinylProcess(&g_lib, N);
				for (int i = 0; i < N; i++)
				{
					if (!isfinite(bufL[i]) || !isfinite(bufR[i])) bad++;
					const double a = fabs((double)bufL[i]);
					if (a > pk) pk = a;
				}
			}
		}
		check("non-finite samples", (double)bad, 0.0, 0.0);
		check("peak stays bounded", pk, 0.05, 8.0);
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
