// What does switching a card on during playback actually sound like?
//
//   NDK=$LOCALAPPDATA/Android/Sdk/ndk/<ver>/toolchains/llvm/prebuilt/windows-x86_64/bin
//   J=app/src/main/cpp/libjamesdsp/Main/libjamesdsp/jni/jamesdsp/jdsp
//   $NDK/aarch64-linux-android29-clang -O2 -w -ffp-contract=off -I $J \
//       tools/enable_harness.c $J/Effects/{tape,exciter,lowend,transient,imaging,dynamicEq}.c \
//       tools/lockstub.c -lm -o enable
//
// The chain skips an effect entirely while its flag is clear, so the instant the
// user taps a card the effect starts mid-stream with whatever state Enable left
// it in. Every other harness here starts from silence and runs to the end, so
// none of them has ever looked at that moment - and it is the moment the user
// most directly causes and most directly hears.
//
// Two things can go wrong and they need different measurements:
//
//   A gap. Any effect holding a delay line starts with it empty, so the first
//   read comes back as silence rather than as audio. Measured as the level just
//   after the switch against the level just before.
//
//   A click. State that does not match the signal - or a gain that jumps -
//   shows up as one step far larger than the waveform's own slope. Measured as
//   the biggest sample-to-sample step near the switch against the biggest step
//   in undisturbed running.
//
// The test signal is a steady tone. Noise would hide both: its own
// sample-to-sample steps are large and random, so a click could not be told
// apart from the signal, and its level is only meaningful averaged over a
// window. A tone has a known slope and a constant level, so both faults stand
// out immediately.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define FS     48000.0
#define N      (48000 * 2)
#define SWITCH (48000)          /* one second in, on a zero crossing */
#define WIN    960              /* 20 ms */

static JamesDSPLib g_lib;
static float bufL[N], bufR[N];
static int failures = 0;

typedef void (*Fn)(JamesDSPLib *);

static void prepare(void)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = (float)FS;
}

/* 200 Hz divides 48000 exactly, so the switch lands on a zero crossing and any
   step seen there is the effect's doing rather than the tone's. */
static void fillTone(void)
{
	for (int i = 0; i < N; i++)
	{
		const float v = 0.5f * (float)sin(2.0 * M_PI * 200.0 * (double)i / FS);
		bufL[i] = v;
		bufR[i] = v;
	}
}

static double rms(int from, int count)
{
	double acc = 0.0;
	for (int i = from; i < from + count; i++) acc += (double)bufL[i] * (double)bufL[i];
	return sqrt(acc / (double)count);
}

static double maxStep(int from, int count)
{
	double m = 0.0;
	for (int i = from + 1; i < from + count; i++)
	{
		const double d = fabs((double)bufL[i] - (double)bufL[i - 1]);
		if (d > m) m = d;
	}
	return m;
}

static void check(const char *what, double got, double lo, double hi)
{
	const int ok = got >= lo && got <= hi;
	printf("  %-52s %8.2f  [%7.2f..%7.2f]  %s\n", what, got, lo, hi, ok ? "ok" : "FAIL");
	if (!ok) failures++;
}

/* Run the tone through, switching the effect on part way, exactly as the chain
   would: nothing at all is called while it is off. */
static void trial(const char *name, Fn setup, Fn enable, void (*run)(JamesDSPLib *, size_t))
{
	prepare();
	setup(&g_lib);
	fillTone();

	/* Before the switch the chain does not call Process, so the buffer keeps
	   the dry signal - which is what actually happens, and is why the effect
	   has no history when it starts. */
	enable(&g_lib);

	/* From the switch to the end, in blocks, on the tail of the buffer only. */
	g_lib.tmpBuffer[0] = bufL + SWITCH;
	g_lib.tmpBuffer[1] = bufR + SWITCH;
	run(&g_lib, N - SWITCH);

	const double before = rms(SWITCH - WIN, WIN);
	const double after = rms(SWITCH, WIN);
	const double dropDb = 20.0 * log10((after > 1e-12 ? after : 1e-12) / before);

	/* The waveform's own steepest step, measured well after everything has
	   settled, is the yardstick for what counts as a click. */
	const double normalStep = maxStep(N - 4800, 4800);
	const double switchStep = maxStep(SWITCH - 2, 8);

	/* A short window right at the switch as well as the 20 ms one. A delay line
	   starting empty is silent for exactly as long as its read offset - twelve
	   milliseconds for the tape - which a 20 ms average dilutes into something
	   that looks like an ordinary level change. Measured over the first 12 ms
	   it is unmistakable. */
	const double firstMs = rms(SWITCH, 576);
	const double firstDb = 20.0 * log10((firstMs > 1e-12 ? firstMs : 1e-12) / before);

	printf("\n%s\n", name);
	printf("      level after the switch %+.2f dB over 20 ms, %+.2f dB over the"
	       " first 12 ms, step %.4f vs %.4f normal\n",
	       dropDb, firstDb, switchStep, normalStep);

	char buf[128];
	snprintf(buf, sizeof(buf), "%s: no gap when switched on (dB)", name);
	/* Effects legitimately change the level - a shelf or a compressor should.
	   Six dB is far more than any of these ask for at their defaults, and a
	   dropout is far more than six. */
	check(buf, dropDb, -6.0, 6.0);

	snprintf(buf, sizeof(buf), "%s: nor in the first 12 ms of it (dB)", name);
	check(buf, firstDb, -6.0, 6.0);

	snprintf(buf, sizeof(buf), "%s: no click when switched on", name);
	check(buf, switchStep / normalStep, 0.0, 3.0);
}

/* ---- the six, at their shipped defaults ----------------------------------- */

static void tapeSetup(JamesDSPLib *j)
{ TapeSetParam(j, 25.0f, 30.0f, 35.0f, -20.0f, 3.0f, 100.0f); }

static void exciterSetup(JamesDSPLib *j)
{ ExciterSetParam(j, 150.0f, 900.0f, 4500.0f, 30.0f, 12.0f, 18.0f, 35.0f,
                  EXCITER_TUBE, 6.0f, 100.0f); }

static void lowEndSetup(JamesDSPLib *j)
{ LowEndSetParam(j, 30.0f, 90.0f, 3.0f, 300.0f, -3.0f, 100.0f); }

static void transientSetup(JamesDSPLib *j)
{ TransientSetParam(j, 200.0f, 3000.0f, 45.0f, -20.0f, 30.0f, -15.0f,
                    0.0f, 0.0f, 6.0f, 100.0f); }

static void imagingSetup(JamesDSPLib *j)
{ ImagingSetParam(j, 120.0f, 250.0f, 1500.0f, 6000.0f, 1.0f, 1.15f, 1.6f, 100.0f); }

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
}

/* The other half of the same question. Switching a card OFF is not symmetric
   with switching it on: the chain simply stops calling Process, so the output
   reverts to dry between one sample and the next with nothing able to smooth
   it. Whether that is audible depends entirely on how far the effect's output
   had drifted from the dry signal - which for a delay line is as far as it is
   possible to get, since the two are at different points in the waveform. */
static void trialOff(const char *name, Fn setup, Fn enable, Fn disable,
                     void (*run)(JamesDSPLib *, size_t), int (*stillOn)(JamesDSPLib *))
{
	prepare();
	setup(&g_lib);
	fillTone();
	enable(&g_lib);

	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
	run(&g_lib, SWITCH);

	disable(&g_lib);

	/* The chain calls Process for exactly as long as the flag is set, so the
	   harness has to as well - otherwise an effect that asks to be kept alive
	   while it fades out would be measured as though it had been cut off, which
	   is the very thing being tested. Blocks rather than one long call, because
	   the flag is only re-read between them. */
	size_t pos = SWITCH;
	while (pos < N && stillOn(&g_lib))
	{
		const size_t blk = (N - pos) < 256 ? (N - pos) : 256;
		g_lib.tmpBuffer[0] = bufL + pos;
		g_lib.tmpBuffer[1] = bufR + pos;
		run(&g_lib, blk);
		pos += blk;
	}

	const double normalStep = maxStep(SWITCH - 4800, 4700);
	/* The worst step anywhere across the switch-off AND any fade that follows
	   it - a fade that ends in a step has only moved the click later. */
	const double switchStep = maxStep(SWITCH - 2, 1500);

	/* Judged as a fraction of the programme peak, not as a multiple of the
	   signal's own slope.

	   Every effect steps a little when it is bypassed instantly, because its
	   output has drifted from the dry signal - by a filter's group delay for
	   most of these, which at 200 Hz is a fraction of a period. That is
	   inherent to instant bypass and is true of all thirty-odd effects in this
	   app, so a ratio against the local slope condemns any effect with a
	   filter in it and says nothing about whether the result is audible.

	   What determines audibility is how much of the waveform appears out of
	   nowhere in one sample. The tape was jumping twelve milliseconds along a
	   five-millisecond period - two and a half periods, so wet and dry were
	   entirely uncorrelated - and injected 80% of full scale. A group delay
	   injects a few percent. Fifteen percent sits well above every filter here
	   and well below anything that decorrelated. */
	const double peak = 0.5;
	const double frac = switchStep / peak * 100.0;

	printf("\n%s (switched off)\n", name);
	printf("      step %.4f = %.1f%% of peak (local slope %.4f)\n",
	       switchStep, frac, normalStep);

	char buf[128];
	snprintf(buf, sizeof(buf), "%s: no click when switched off (%% of peak)", name);
	check(buf, frac, 0.0, 15.0);
}

/* Whether the chain would still be calling this effect. */
static int tapeOn(JamesDSPLib *j)      { return j->tapeEnabled; }
static int exciterOn(JamesDSPLib *j)   { return j->exciterEnabled; }
static int lowEndOn(JamesDSPLib *j)    { return j->lowEndEnabled; }
static int transientOn(JamesDSPLib *j) { return j->transientEnabled; }
static int imagingOn(JamesDSPLib *j)   { return j->imagingEnabled; }
static int dynEqOn(JamesDSPLib *j)     { return j->dynamicEqEnabled; }

int main(void)
{
	printf("== switching a card on during playback ==\n");
	trial("vintage tape", tapeSetup, TapeEnable, TapeProcess);
	trial("exciter", exciterSetup, ExciterEnable, ExciterProcess);
	trial("low end", lowEndSetup, LowEndEnable, LowEndProcess);
	trial("impact", transientSetup, TransientEnable, TransientProcess);
	trial("stereo imaging", imagingSetup, ImagingEnable, ImagingProcess);
	trial("dynamic EQ", dynEqSetup, DynamicEqEnable, DynamicEqProcess);

	printf("\n== switching a card off during playback ==\n");
	trialOff("vintage tape", tapeSetup, TapeEnable, TapeDisable, TapeProcess, tapeOn);
	trialOff("exciter", exciterSetup, ExciterEnable, ExciterDisable, ExciterProcess, exciterOn);
	trialOff("low end", lowEndSetup, LowEndEnable, LowEndDisable, LowEndProcess, lowEndOn);
	trialOff("impact", transientSetup, TransientEnable, TransientDisable, TransientProcess, transientOn);
	trialOff("stereo imaging", imagingSetup, ImagingEnable, ImagingDisable, ImagingProcess, imagingOn);
	trialOff("dynamic EQ", dynEqSetup, DynamicEqEnable, DynamicEqDisable, DynamicEqProcess, dynEqOn);

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
	       failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
