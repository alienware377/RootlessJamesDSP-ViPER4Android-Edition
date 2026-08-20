// What do the new effects cost, and what happens when the music stops?
//
//   NDK=$LOCALAPPDATA/Android/Sdk/ndk/<ver>/toolchains/llvm/prebuilt/windows-x86_64/bin
//   J=app/src/main/cpp/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp
//   $NDK/aarch64-linux-android29-clang -O2 -w -ffp-contract=off -I $J \
//       tools/perf_harness.c $J/Effects/{tape,exciter,lowend,transient,imaging,dynamicEq}.c \
//       tools/lockstub.c -lm -o perf
//
// Two questions, one harness, because the second is only visible in the timing
// of the first.
//
// COST. Every effect here runs per-sample, and two of them (Impact and the
// exciter) call transcendentals in the inner loop. On a phone that is a battery
// question as much as a glitch question, and neither is answerable by reading
// the code. So: nanoseconds per sample on real hardware, and what share of one
// core that is at 48 kHz.
//
// DENORMALS. When a track ends, the input becomes exact zeroes but every IIR
// filter and envelope follower keeps decaying towards zero rather than reaching
// it. On the way it passes through the denormal range, and hardware that traps
// denormals to microcode can slow down by one or two orders of magnitude - so
// the moment the music stops is precisely when the effect gets most expensive,
// which is a real way to produce dropouts at the worst time.
//
// Reading the ARM documentation cannot settle this: whether it bites depends on
// the FPCR flush-to-zero bit as the audio thread actually finds it, and on
// whether the compiler used NEON (which flushes regardless) or scalar ops.
// Measuring settles it. Silence is fed only AFTER loud audio, so the state is
// genuinely decaying rather than already at rest.
//
// Both phases copy their block in from a prepared buffer, so the loop overhead
// is identical between them and the ratio means what it looks like. The first
// row measures that copy on its own, and is the number to subtract.
//
// The silent phase is timed block by block rather than as a total, and judged on
// its WORST block against its median. Averaging over the whole run would have
// hidden the very thing being looked for: a filter decaying at 0.996 per sample
// spends around twenty thousand samples on its way down to 1e-38 and only about
// four thousand more crossing the denormal range below it. That is one percent
// of this run, so even a fifty-fold stall would only lift the average by half,
// and a threshold set to catch it would be indistinguishable from noise. Per
// block, the same stall is unmissable.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "jdsp_header.h"

#define BLOCK  1024
#define BLOCKS 400
#define TOTAL  (BLOCK * BLOCKS)

static JamesDSPLib g_lib;
static float bufL[BLOCK], bufR[BLOCK];
static float srcL[BLOCK], srcR[BLOCK];
static float zero[BLOCK];
static int failures = 0;

static void prepare(void)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = 48000.0f;
	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
}

static void fillSource(void)
{
	unsigned s = 4242u;
	for (int i = 0; i < BLOCK; i++)
	{
		s = s * 1664525u + 1013904223u;
		const float a = 0.6f * ((float)((s >> 9) & 0xFFFF) / 32768.0f - 1.0f);
		srcL[i] = a;
		srcR[i] = a * 0.8f;
	}
	memset(zero, 0, sizeof(zero));
}

static double now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

typedef void (*Fn)(JamesDSPLib *);

/* Run `blocks` blocks of either the noise or silence, returning ns per sample.
   The copy in is part of the measurement in both cases, deliberately. */
static double timeRun(Fn run, int silent, int blocks)
{
	const double t0 = now_ns();
	for (int b = 0; b < blocks; b++)
	{
		memcpy(bufL, silent ? zero : srcL, sizeof(bufL));
		memcpy(bufR, silent ? zero : srcR, sizeof(bufR));
		if (run) run(&g_lib);
	}
	const double t1 = now_ns();
	return (t1 - t0) / (double)(blocks * BLOCK);
}

/* As above, but keeping every block's time so the worst one can be found. */
static double blockNs[BLOCKS];

static int cmpDouble(const void *a, const void *b)
{
	const double x = *(const double *)a, y = *(const double *)b;
	return x < y ? -1 : (x > y ? 1 : 0);
}

static void timeRunPerBlock(Fn run, int silent, double *median, double *worst)
{
	for (int b = 0; b < BLOCKS; b++)
	{
		const double t0 = now_ns();
		memcpy(bufL, silent ? zero : srcL, sizeof(bufL));
		memcpy(bufR, silent ? zero : srcR, sizeof(bufR));
		if (run) run(&g_lib);
		blockNs[b] = (now_ns() - t0) / (double)BLOCK;
	}
	double sorted[BLOCKS];
	memcpy(sorted, blockNs, sizeof(sorted));
	qsort(sorted, BLOCKS, sizeof(double), cmpDouble);
	*median = sorted[BLOCKS / 2];
	/* Second from the top, not the top: one stray block is the scheduler
	   taking the core away, which happens on any phone and is not a property
	   of the code. A denormal stall lasts thousands of samples, so it lands in
	   many consecutive blocks and cannot hide below this. */
	*worst = sorted[BLOCKS - 2];
}

static void check(const char *what, double got, double lo, double hi)
{
	const int ok = got >= lo && got <= hi;
	printf("  %-46s %9.2f  [%8.2f..%8.2f]  %s\n", what, got, lo, hi, ok ? "ok" : "FAIL");
	if (!ok) failures++;
}

static double baseline = 0.0;

static void measure(const char *name, Fn setup, Fn run)
{
	prepare();
	if (setup) setup(&g_lib);

	/* Warm the caches and, more importantly, get the filter state up off zero
	   before anything is timed. */
	timeRun(run, 0, 32);

	const double loud = timeRun(run, 0, BLOCKS);
	const double loudNet = loud - baseline;
	/* Share of one core, if this were the only thing running at 48 kHz. */
	const double core = loudNet * 48000.0 / 1e9 * 100.0;

	/* Straight from loud audio into digital silence, which is the transition
	   that matters - the state is decaying through the denormal range now. */
	double median = 0.0, worst = 0.0;
	timeRunPerBlock(run, 1, &median, &worst);

	printf("  %-16s  loud %7.2f ns/sample (%5.2f%% of a core)"
	       "   silent: median %6.2f worst %6.2f\n",
	       name, loudNet, core, median - baseline, worst - baseline);

	/* What is asserted is the SHAPE of the slowdown, not its size. The first
	   attempt here compared the worst block against the median and tripped on
	   the tape at 3.2x, which looked like a finding and was not: the slow
	   blocks were at 30 and 98, scattered, and three more runs put the ratio
	   between 2.3 and 2.7. This tablet's governor moves the clock around
	   enough that any threshold loose enough to survive it would be too loose
	   to catch a stall.

	   The shape is unambiguous where the size is not. State decaying at 0.996
	   per sample spends roughly four thousand samples crossing the denormal
	   range - four consecutive blocks at this size, and far more for the
	   longer time constants - whereas the scheduler takes a core away for one
	   block at a time. So: the longest UNBROKEN run of slow blocks. */
	int longest = 0, current = 0, at = -1;
	for (int b = 0; b < BLOCKS; b++)
	{
		if (blockNs[b] > median * 2.0)
		{
			if (++current > longest) { longest = current; at = b - current + 1; }
		}
		else current = 0;
	}
	if (longest)
		printf("      longest unbroken slow run: %d block%s at %d"
		       " (worst block %.2f vs median %.2f)\n",
		       longest, longest == 1 ? "" : "s", at, worst - baseline,
		       median - baseline);

	char buf[128];
	snprintf(buf, sizeof(buf), "%s: silence decays without stalling", name);
	check(buf, (double)longest, 0.0, 3.0);
}

/* ---- the six at the settings they actually ship with ---------------------- */

static void tapeSetup(JamesDSPLib *j)
{
	TapeSetParam(j, 25.0f, 30.0f, 35.0f, -20.0f, 3.0f, 100.0f);
	TapeEnable(j);
}
static void tapeRun(JamesDSPLib *j) { TapeProcess(j, BLOCK); }

static void exciterSetup(JamesDSPLib *j)
{
	ExciterSetParam(j, 150.0f, 900.0f, 4500.0f, 30.0f, 12.0f, 18.0f, 35.0f,
	                EXCITER_TUBE, 6.0f, 100.0f);
	ExciterEnable(j);
}
static void exciterRun(JamesDSPLib *j) { ExciterProcess(j, BLOCK); }

static void lowEndSetup(JamesDSPLib *j)
{
	LowEndSetParam(j, 30.0f, 90.0f, 3.0f, 300.0f, -3.0f, 100.0f);
	LowEndEnable(j);
}
static void lowEndRun(JamesDSPLib *j) { LowEndProcess(j, BLOCK); }

static void transientSetup(JamesDSPLib *j)
{
	TransientSetParam(j, 200.0f, 3000.0f, 45.0f, -20.0f, 30.0f, -15.0f,
	                  0.0f, 0.0f, 6.0f, 100.0f);
	TransientEnable(j);
}
static void transientRun(JamesDSPLib *j) { TransientProcess(j, BLOCK); }

static void imagingSetup(JamesDSPLib *j)
{
	ImagingSetParam(j, 120.0f, 250.0f, 1500.0f, 6000.0f,
	                1.0f, 1.15f, 1.6f, 100.0f);
	ImagingEnable(j);
}
static void imagingRun(JamesDSPLib *j) { ImagingProcess(j, BLOCK); }

static void dynEqSetup(JamesDSPLib *j)
{
	// freq, Q, threshold dB, ratio, attack ms, release ms, range dB, mode
	const float bands[3 * DYNEQ_VALUES_PER_BAND] = {
		180.0f, 1.0f, -22.0f, 3.0f, 15.0f, 150.0f, -6.0f, (float)DYNEQ_MODE_COMPRESS,
		3200.0f, 1.4f, -26.0f, 3.0f,  3.0f,  80.0f, -5.0f, (float)DYNEQ_MODE_COMPRESS,
		6800.0f, 3.0f, -30.0f, 4.0f,  1.0f,  40.0f, -8.0f, (float)DYNEQ_MODE_COMPRESS
	};
	DynamicEqSetBands(j, bands, 3);
	DynamicEqSetParam(j, 100.0f, MS_MODE_STEREO);
	DynamicEqEnable(j);
}
static void dynEqRun(JamesDSPLib *j) { DynamicEqProcess(j, BLOCK); }

int main(void)
{
	printf("== cost, and what silence costs ==\n\n");
	fillSource();

	prepare();
	timeRun(NULL, 0, 32);
	baseline = timeRun(NULL, 0, BLOCKS);
	printf("  %-16s  %7.2f ns/sample (subtracted from every row below)\n\n",
	       "block copy", baseline);

	measure("vintage tape", tapeSetup, tapeRun);
	measure("exciter", exciterSetup, exciterRun);
	measure("low end", lowEndSetup, lowEndRun);
	measure("impact", transientSetup, transientRun);
	measure("stereo imaging", imagingSetup, imagingRun);
	measure("dynamic EQ", dynEqSetup, dynEqRun);

	printf("\ndenormal behaviour\n");
	/* The assertions were emitted inline above, one per effect. */

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
