// Does the balance control put the sound where it says, and leave it alone at
// centre?
//
//   $NDK/aarch64-linux-android29-clang -O2 -w -ffp-contract=off -I $J \
//       tools/balance_harness.c $J/Effects/balance.c tools/lockstub.c -lm -o balance
//
// The centre case is the one that matters most. This card sits in every chain
// and most people will never move it, so if centred were merely "close to"
// unity it would quietly cost everyone a fraction of a dB for nothing.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define FS 48000.0
#define N  48000

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

/* Distinct signals per channel, so a swap is unmistakable. */
static void fill(void)
{
	for (int i = 0; i < N; i++)
	{
		bufL[i] = (float)(0.5 * sin(2.0 * M_PI * 300.0 * (double)i / FS));
		bufR[i] = (float)(0.25 * sin(2.0 * M_PI * 900.0 * (double)i / FS));
	}
}

static double rmsOf(const float *b)
{
	double a = 0.0;
	/* Skip the first 10 ms: the gains smooth towards their target, so the very
	   start is a ramp rather than the steady state being measured. */
	for (int i = 480; i < N; i++) a += (double)b[i] * (double)b[i];
	return sqrt(a / (double)(N - 480));
}

static void check(const char *what, double got, double lo, double hi)
{
	const int ok = got >= lo && got <= hi;
	printf("  %-46s %9.4f  [%8.4f..%8.4f]  %s\n", what, got, lo, hi, ok ? "ok" : "FAIL");
	if (!ok) failures++;
}

int main(void)
{
	printf("== balance ==\n\n");

	printf("centred\n");
	{
		prepare();
		BalanceSetParam(&g_lib, 0.0f, 0, 0.0f);
		BalanceEnable(&g_lib);
		fill();
		float refL[64], refR[64];
		memcpy(refL, bufL, sizeof(refL));
		memcpy(refR, bufR, sizeof(refR));
		BalanceProcess(&g_lib, N);
		const int same = memcmp(refL, bufL, sizeof(refL)) == 0 &&
		                 memcmp(refR, bufR, sizeof(refR)) == 0;
		check("centred, unswapped, stereo is bit-identical", same ? 1 : 0, 1, 1);
	}

	printf("\nbalance moves the sound\n");
	{
		prepare(); BalanceSetParam(&g_lib, -100.0f, 0, 0.0f); BalanceEnable(&g_lib);
		fill();
		const double beforeL = rmsOf(bufL);
		BalanceProcess(&g_lib, N);
		check("full left silences the right", rmsOf(bufR), 0.0, 1e-4);
		check("full left leaves the left alone", rmsOf(bufL) / beforeL, 0.999, 1.001);
	}
	{
		prepare(); BalanceSetParam(&g_lib, 100.0f, 0, 0.0f); BalanceEnable(&g_lib);
		fill();
		const double beforeR = rmsOf(bufR);
		BalanceProcess(&g_lib, N);
		check("full right silences the left", rmsOf(bufL), 0.0, 1e-4);
		check("full right leaves the right alone", rmsOf(bufR) / beforeR, 0.999, 1.001);
	}
	{
		/* Half left should cut the right by half, and never lift the left -
		   the whole point of attenuating the far side instead of boosting. */
		prepare(); BalanceSetParam(&g_lib, -50.0f, 0, 0.0f); BalanceEnable(&g_lib);
		fill();
		const double bL = rmsOf(bufL), bR = rmsOf(bufR);
		BalanceProcess(&g_lib, N);
		check("half left halves the right", rmsOf(bufR) / bR, 0.49, 0.51);
		check("half left never boosts the left", rmsOf(bufL) / bL, 0.999, 1.001);
	}

	printf("\nswap\n");
	{
		prepare(); BalanceSetParam(&g_lib, 0.0f, 1, 0.0f); BalanceEnable(&g_lib);
		fill();
		const double bL = rmsOf(bufL), bR = rmsOf(bufR);
		BalanceProcess(&g_lib, N);
		check("left now carries what the right had", rmsOf(bufL) / bR, 0.999, 1.001);
		check("right now carries what the left had", rmsOf(bufR) / bL, 0.999, 1.001);
	}

	printf("\nmono\n");
	{
		prepare(); BalanceSetParam(&g_lib, 0.0f, 0, 100.0f); BalanceEnable(&g_lib);
		fill();
		BalanceProcess(&g_lib, N);
		double diff = 0.0;
		for (int i = 480; i < N; i++) diff += fabs((double)bufL[i] - (double)bufR[i]);
		check("fully mono makes both channels identical", diff, 0.0, 1e-3);
	}
	{
		/* Half way must be genuinely half way, not a switch that only acts at
		   the top of its travel. */
		prepare(); BalanceSetParam(&g_lib, 0.0f, 0, 50.0f); BalanceEnable(&g_lib);
		fill();
		BalanceProcess(&g_lib, N);
		double diff = 0.0;
		for (int i = 480; i < N; i++) diff += fabs((double)bufL[i] - (double)bufR[i]);
		double full = 0.0;
		prepare(); BalanceSetParam(&g_lib, 0.0f, 0, 0.0f); BalanceEnable(&g_lib);
		fill(); BalanceProcess(&g_lib, N);
		for (int i = 480; i < N; i++) full += fabs((double)bufL[i] - (double)bufR[i]);
		check("half mono halves the difference", diff / full, 0.49, 0.51);
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
