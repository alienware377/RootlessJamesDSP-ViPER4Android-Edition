// When the sample rate changes under an effect that is already running, does it
// follow?
//
//   NDK=$LOCALAPPDATA/Android/Sdk/ndk/<ver>/toolchains/llvm/prebuilt/windows-x86_64/bin
//   J=app/src/main/cpp/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp
//   $NDK/aarch64-linux-android29-clang -O2 -w -ffp-contract=off -I $J \
//       tools/ratechange_harness.c $J/Effects/{tape,exciter,lowend,transient,imaging,dynamicEq}.c \
//       tools/lockstub.c -lm -o ratechange
//
// The assertion is an equivalence rather than a measurement, which makes it both
// stronger and simpler than probing corner frequencies: set an effect up at one
// rate, tell it the rate has changed, and it must then behave EXACTLY as it
// would have done had it been set up at the new rate to begin with. Sample for
// sample, since both start from cleared state.
//
// Each effect is checked twice. The second run deliberately skips the refresh,
// and must NOT match - otherwise the first check proves nothing, because an
// effect that ignored the rate entirely would sail through it.
//
// Rates are 48000 and 44100 because those are the only two the engine ever runs
// at internally: JamesDSPSetSampleRate turns on the ASRC and pins jdsp->fs to
// one or the other whenever the stream is outside that range.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define N 65536

static JamesDSPLib g_lib;
static float bufL[N], bufR[N];
static float refL[N], refR[N];
static int failures = 0;

static void prepare(float fs)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = fs;
	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
}

/* Broadband and not quite the same in both channels, so the stereo-dependent
   effects are exercised rather than fed a mono signal they might treat as a
   special case. */
static void fillNoise(void)
{
	unsigned s = 9001u;
	for (int i = 0; i < N; i++)
	{
		s = s * 1664525u + 1013904223u;
		const float a = 0.5f * ((float)((s >> 9) & 0xFFFF) / 32768.0f - 1.0f);
		s = s * 1664525u + 1013904223u;
		const float b = 0.5f * ((float)((s >> 9) & 0xFFFF) / 32768.0f - 1.0f);
		bufL[i] = a;
		bufR[i] = a * 0.7f + b * 0.3f;
	}
}

static void keep(void)
{
	memcpy(refL, bufL, sizeof(refL));
	memcpy(refR, bufR, sizeof(refR));
}

static int matchesReference(void)
{
	for (int i = 0; i < N; i++)
		if (bufL[i] != refL[i] || bufR[i] != refR[i]) return 0;
	return 1;
}

static void check(const char *what, int got, int want)
{
	const int ok = got == want;
	printf("  %-56s %-8s  %s\n", what, got ? "same" : "differs", ok ? "ok" : "FAIL");
	if (!ok) failures++;
}

/* setup() configures and enables one effect at whatever rate is currently set;
   run() processes the buffer; refresh() is the function under test. */
typedef void (*Fn)(JamesDSPLib *);

static void trial(const char *name, Fn setup, Fn run, Fn refresh)
{
	const float oldRate = 48000.0f, newRate = 44100.0f;

	/* The reference: built at the new rate from the start. */
	prepare(newRate);
	setup(&g_lib);
	fillNoise();
	run(&g_lib);
	keep();

	/* Built at the old rate, then told the rate has changed. */
	prepare(oldRate);
	setup(&g_lib);
	g_lib.fs = newRate;
	refresh(&g_lib);
	fillNoise();
	run(&g_lib);
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "%s: after a refresh, matches native %g Hz",
		         name, (double)newRate);
		check(buf, matchesReference(), 1);
	}

	/* The control. Without the refresh this must be wrong, or the check above
	   is not testing anything. */
	prepare(oldRate);
	setup(&g_lib);
	g_lib.fs = newRate;
	fillNoise();
	run(&g_lib);
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "%s: without one, does not (control)", name);
		check(buf, matchesReference(), 0);
	}
}

/* ---- the six, at settings where every rate-dependent term is in play ------- */

static void tapeSetup(JamesDSPLib *j)
{
	TapeSetParam(j, 40.0f, 35.0f, 45.0f, -30.0f, 5.0f, 100.0f);
	TapeEnable(j);
}
static void tapeRun(JamesDSPLib *j) { TapeProcess(j, N); }

static void exciterSetup(JamesDSPLib *j)
{
	ExciterSetParam(j, 150.0f, 900.0f, 4500.0f, 40.0f, 25.0f, 30.0f, 45.0f,
	                EXCITER_TRIODE, 8.0f, 100.0f);
	ExciterEnable(j);
}
static void exciterRun(JamesDSPLib *j) { ExciterProcess(j, N); }

static void lowEndSetup(JamesDSPLib *j)
{
	LowEndSetParam(j, 30.0f, 90.0f, 4.0f, 300.0f, -3.5f, 100.0f);
	LowEndEnable(j);
}
static void lowEndRun(JamesDSPLib *j) { LowEndProcess(j, N); }

static void transientSetup(JamesDSPLib *j)
{
	TransientSetParam(j, 200.0f, 3000.0f, 60.0f, -30.0f, 40.0f, -20.0f,
	                  20.0f, 0.0f, 9.0f, 100.0f);
	TransientEnable(j);
}
static void transientRun(JamesDSPLib *j) { TransientProcess(j, N); }

static void imagingSetup(JamesDSPLib *j)
{
	ImagingSetParam(j, 120.0f, 250.0f, 1500.0f, 6000.0f,
	                1.0f, 1.25f, 1.6f, 100.0f);
	ImagingEnable(j);
}
static void imagingRun(JamesDSPLib *j) { ImagingProcess(j, N); }

static void dynEqSetup(JamesDSPLib *j)
{
	// freq, Q, threshold dB, ratio, attack ms, release ms, range dB, mode
	const float bands[3 * DYNEQ_VALUES_PER_BAND] = {
		180.0f, 1.0f, -30.0f, 3.0f, 15.0f, 150.0f, -6.0f, (float)DYNEQ_MODE_COMPRESS,
		3200.0f, 1.4f, -32.0f, 3.0f,  3.0f,  80.0f, -5.0f, (float)DYNEQ_MODE_COMPRESS,
		6800.0f, 3.0f, -34.0f, 4.0f,  1.0f,  40.0f, -8.0f, (float)DYNEQ_MODE_COMPRESS
	};
	DynamicEqSetBands(j, bands, 3);
	DynamicEqSetParam(j, 100.0f, MS_MODE_STEREO);
	DynamicEqEnable(j);
}
static void dynEqRun(JamesDSPLib *j) { DynamicEqProcess(j, N); }

int main(void)
{
	printf("== sample rate change under a running effect ==\n\n");
	trial("vintage tape", tapeSetup, tapeRun, TapeRefresh);
	trial("exciter", exciterSetup, exciterRun, ExciterRefresh);
	trial("low end", lowEndSetup, lowEndRun, LowEndRefresh);
	trial("impact", transientSetup, transientRun, TransientRefresh);
	trial("stereo imaging", imagingSetup, imagingRun, ImagingRefresh);
	trial("dynamic EQ", dynEqSetup, dynEqRun, DynamicEqRefresh);

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
