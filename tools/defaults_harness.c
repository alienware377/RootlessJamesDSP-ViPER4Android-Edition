// "Switch the card on and touch nothing - does anything happen?"
//
// Every effect below is run at the values it actually ships with, taken from
// the three places that have to agree: the constructor call in
// jdspController.c, the cache.get default in JamesDspBaseEngine.kt, and
// android:defaultValue in its card XML. If a default changes and this is not
// updated, the test fails, which is the point.
//
// This exists because two effects shipped inaudible-by-default and were
// reported by the user, and every existing test passed both times - they were
// written against values chosen to make the effect look good rather than the
// values a user actually gets.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

#define N 16384

static JamesDSPLib g_lib;
static float bufL[N], bufR[N], ref[N];

static void prepare(float fs)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = fs;
	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
}

// Broadband, with real stereo content so spatial effects have something to
// work on and treble effects have something up there to lift.
static void fillMusic(float amp)
{
	unsigned seed = 12345;
	for (int i = 0; i < N; i++)
	{
		double t = (double)i / 48000.0;
		seed = seed * 1103515245u + 12345u;
		double noise = ((double)((seed >> 16) & 0x7fff) / 16384.0 - 1.0) * 0.10;
		double v = 0.45 * sin(2.0 * M_PI * 70.0 * t)
		         + 0.28 * sin(2.0 * M_PI * 600.0 * t)
		         + 0.18 * sin(2.0 * M_PI * 3000.0 * t)
		         + 0.12 * sin(2.0 * M_PI * 9000.0 * t)
		         + noise;
		bufL[i] = (float)(v * amp);
		bufR[i] = (float)((v * 0.85 + noise * 0.4) * amp);
	}
	memcpy(ref, bufL, sizeof(ref));
}

// Energy of (output - input) against energy of input, in dB. Below about -40
// nothing is going to be noticed; above -20 is plainly a different sound.
static double difference(int skip)
{
	double num = 0.0, den = 0.0;
	for (int i = skip; i < N; i++)
	{
		double d = (double)bufL[i] - ref[i];
		num += d * d;
		den += (double)ref[i] * ref[i];
	}
	return 10.0 * log10((num + 1e-30) / (den + 1e-30));
}

static int report(const char *name, double diff, int wantAudible)
{
	const char *verdict = diff < -40.0 ? "silent"
	                    : diff < -20.0 ? "subtle" : "audible";
	int bad = wantAudible ? (diff < -20.0) : (diff > -60.0);
	printf("  %-30s %7.2f dB  %-8s %s\n", name, diff, verdict,
		bad ? (wantAudible ? "*** NOTHING HAPPENS ***" : "*** SHOULD BE TRANSPARENT ***") : "ok");
	return bad;
}

int main(void)
{
	int fail = 0;

	printf("== effects that should colour the sound on the switch alone ==\n");

	// Spectrum extension: 7600 Hz, 45% (was 15%, which measured a quarter of
	// a decibel above 8kHz).
	{
		prepare(48000.0f);
		SpectrumExtensionSetParam(&g_lib, 7600.0f, 45.0f);
		SpectrumExtensionEnable(&g_lib);
		fillMusic(0.6f);
		SpectrumExtensionProcess(&g_lib, N);
		fail |= report("spectrum extension", difference(256), 1);
		SpectrumExtensionDisable(&g_lib);
	}

	// ViPER clarity: Natural, 6 dB. Natural is a first-difference sharpener,
	// and its coefficient scale is what decides whether it does anything.
	{
		prepare(48000.0f);
		ViperClaritySetParam(&g_lib, 0, 6.0f);
		ViperClarityEnable(&g_lib);
		fillMusic(0.6f);
		ViperClarityProcess(&g_lib, N);
		fail |= report("viper clarity (natural)", difference(256), 1);
		ViperClarityDisable(&g_lib);
	}

	// Vacuum tube: 2 dB. The value matters far less than the fact that it
	// survives Enable at all - see below.
	{
		prepare(48000.0f);
		VacuumTubeSetGain(&g_lib, 2.0);
		VacuumTubeEnable(&g_lib);
		fillMusic(0.6f);
		VacuumTubeProcess(&g_lib, N);
		fail |= report("vacuum tube", difference(512), 1);
		VacuumTubeDisable(&g_lib);
	}

	// ViPER reverb: wet 65 (was 30, whose tail measured 27dB down).
	{
		prepare(48000.0f);
		VReverbSetParam(&g_lib, 0, 50.0f, 50.0f, 100.0f, 20.0f, 45.0f, 70.0f,
			30.0f, 50.0f, 50.0f, 65.0f, 100.0f);
		VReverbEnable(&g_lib);
		fillMusic(0.6f);
		VReverbProcess(&g_lib, N);
		fail |= report("viper reverb", difference(512), 1);
		VReverbDisable(&g_lib);
	}

	printf("\n== effects that should NOT colour the sound on the switch alone ==\n");

	// A pitch shifter set to no shift must be transparent. Defaulting it to a
	// shift would silently detune the user's music, which is worse than doing
	// nothing - but it also must not quietly cost treble, which it did at
	// 44.1kHz before the bypass.
	{
		float rates[2] = { 44100.0f, 48000.0f };
		for (int r = 0; r < 2; r++)
		{
			char label[48];
			prepare(rates[r]);
			PitchShiftSetParam(&g_lib, 0.0f, 100.0f);
			PitchShiftEnable(&g_lib);
			fillMusic(0.6f);
			PitchShiftProcess(&g_lib, N);
			snprintf(label, sizeof(label), "pitch shifter, 0 st, %.0fk", rates[r] / 1000.0f);
			fail |= report(label, difference(0), 0);
			PitchShiftDisable(&g_lib);
		}
	}

	printf("\n== the tube drive knob has to survive being enabled ==\n");
	{
		// The JNI always calls SetGain and then Enable. Enable used to run
		// VTInit, which reset the gain, so the knob was inert across its whole
		// range on both engine paths - the same knob that had already been
		// reported dead once for a different reason.
		double drives[] = { 0.0, 6.0, 12.0, 24.0 };
		double prev = -1.0;
		int monotonic = 1;
		for (int d = 0; d < 4; d++)
		{
			prepare(48000.0f);
			VacuumTubeSetGain(&g_lib, drives[d]);
			VacuumTubeEnable(&g_lib);
			fillMusic(0.6f);
			VacuumTubeProcess(&g_lib, N);
			double diff = difference(512);
			printf("  drive %5.1f dB %20s %7.2f dB\n", drives[d], "", diff);
			if (d > 0 && diff <= prev + 0.5) monotonic = 0;
			prev = diff;
			VacuumTubeDisable(&g_lib);
		}
		printf("  %-30s %s\n", "rises with drive",
			monotonic ? "ok" : "*** KNOB IS INERT ***");
		fail |= !monotonic;
	}

	printf("\n%s\n", fail ? "FAILURES PRESENT" : "all checks passed");
	return fail ? 1 : 0;
}
