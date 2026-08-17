// Checks the differential surround after moving its ring buffers off the
// library struct and onto the heap, and confirms 50ms is actually reachable at
// every sample rate rather than being silently clamped.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

// 50ms at 192kHz is 9600 samples, so the window has to be comfortably longer
// than the longest delay under test or the impulse simply never arrives inside
// it - which the first run of this reported as a clamp.
#define N 16384

static JamesDSPLib g_lib;
static float bufL[N], bufR[N];

static void prepare(float fs)
{
	memset(&g_lib, 0, sizeof(g_lib));
	g_lib.fs = fs;
	g_lib.tmpBuffer[0] = bufL;
	g_lib.tmpBuffer[1] = bufR;
}

// A single impulse, so the delay can be measured by finding where it lands.
static void impulse(void)
{
	memset(bufL, 0, sizeof(bufL));
	memset(bufR, 0, sizeof(bufR));
	bufL[0] = 1.0f;
	bufR[0] = 1.0f;
}

static int measure(const char *label, float fs, float askMs)
{
	prepare(fs);
	DiffSurroundEnable(&g_lib);
	DiffSurroundSetParam(&g_lib, 0.0f, askMs);
	impulse();
	DiffSurroundProcess(&g_lib, N);

	int at = -1;
	float best = 0.0f;
	for (int i = 0; i < N; i++)
	{
		if (!isfinite(bufR[i])) { printf("%-32s *** NON-FINITE ***\n", label); return 1; }
		if (fabsf(bufR[i]) > best) { best = fabsf(bufR[i]); at = i; }
	}
	float gotMs = (float)at * 1000.0f / fs;
	int bad = fabsf(gotMs - askMs) > 0.05f;
	printf("%-32s asked %5.1f ms, got %5.2f ms (sample %4d)  %s\n",
		label, askMs, gotMs, at, bad ? "*** CLAMPED ***" : "ok");
	DiffSurroundDisable(&g_lib);
	return bad;
}

int main(void)
{
	int fail = 0;

	printf("== 50ms must be reachable at every rate ==\n");
	float rates[] = { 44100.0f, 48000.0f, 96000.0f, 192000.0f };
	for (int r = 0; r < 4; r++)
	{
		char label[48];
		snprintf(label, sizeof(label), "%.0f Hz", rates[r]);
		fail |= measure(label, rates[r], 50.0f);
	}

	printf("\n== the rest of the range ==\n");
	float asks[] = { 0.0f, 1.0f, 10.0f, 25.0f, 40.0f };
	for (int a = 0; a < 5; a++)
	{
		char label[48];
		snprintf(label, sizeof(label), "%.0f ms at 48kHz", asks[a]);
		fail |= measure(label, 48000.0f, asks[a]);
	}

	printf("\n== processing while disabled must not touch the buffers ==\n");
	{
		prepare(48000.0f);
		// Never enabled, so the rings are null: the guard has to catch it.
		impulse();
		DiffSurroundProcess(&g_lib, N);
		int changed = 0;
		for (int i = 1; i < N; i++) if (bufL[i] != 0.0f) changed++;
		printf("%-32s samples changed: %d  %s\n", "disabled passthrough", changed,
			changed ? "*** WROTE ANYWAY ***" : "ok");
		fail |= (changed != 0);
	}

	printf("\n== enable, disable, enable again ==\n");
	{
		prepare(48000.0f);
		for (int k = 0; k < 4; k++)
		{
			DiffSurroundEnable(&g_lib);
			DiffSurroundSetParam(&g_lib, 5.0f, 20.0f);
			impulse();
			DiffSurroundProcess(&g_lib, N);
			DiffSurroundDisable(&g_lib);
		}
		// A double disable must not free twice.
		DiffSurroundDisable(&g_lib);
		printf("%-32s survived\n", "four cycles + double disable");
	}

	printf("\n== a rate change between set and enable ==\n");
	{
		// Ask for the maximum at a low rate, then bring the engine up at a high
		// one: the stored delay is in samples, so without a re-clamp on enable
		// the read pointer would sit outside the ring.
		prepare(8000.0f);
		DiffSurroundSetParam(&g_lib, 0.0f, 50.0f);
		g_lib.fs = 192000.0f;
		DiffSurroundEnable(&g_lib);
		impulse();
		DiffSurroundProcess(&g_lib, N);
		int bad = 0;
		for (int i = 0; i < N; i++) if (!isfinite(bufR[i])) bad++;
		printf("%-32s non-finite: %d  %s\n", "8k set, 192k enable", bad, bad ? "*** BAD ***" : "ok");
		fail |= bad;
		DiffSurroundDisable(&g_lib);
	}

	printf("\n%s\n", fail ? "FAILURES PRESENT" : "all checks passed");
	return fail ? 1 : 0;
}
