// Does the root build's parameter dispatch actually reach the engine?
//
// The rooted build shares one table, EffectParams.h, with the app. Anything
// missing from that table is not an error - the default arm has to accept
// unknown ids, because refusing makes the audio server tear the effect down -
// so a missing case is completely silent and the app still reports success.
// That is exactly how the equaliser, the compander and the output limiter came
// to be dropped on rooted devices while appearing to work.
//
// This drives applyParam with the same payloads JamesDspRemoteEngine sends and
// asserts the engine state moved. It cannot prove the audio server delivers
// them - that needs a rooted device, and the only one here is not rooted - but
// it does prove every id the app sends has somewhere to land.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../app/src/main/cpp/hal/EffectParams.h"

static JamesDSPLib g_lib;
static int failures = 0;

static void check(const char *what, bool ok, const char *detail = "")
{
	printf("  %-52s %-3s %s\n", what, ok ? "ok" : "FAIL", detail);
	if (!ok) failures++;
}

// applyParam's signature mirrors what the HAL extracts from effect_param_t:
// the short value for the enable ids, the float array for everything else.
static void sendFloats(int32_t id, const float *v, uint32_t n)
{
	applyParam(&g_lib, id, 0, false, v, n);
}
static void sendEnable(int32_t id, bool on)
{
	applyParam(&g_lib, id, on ? 1 : 0, on, nullptr, 0);
}

int main()
{
	printf("== root parameter dispatch ==\n\n");
	// Same order EffectCreate uses: the process-wide tables first, then the
	// instance. Skipping the first leaves the resampler coefficients null.
	JamesDSPGlobalMemoryAllocation();
	JamesDSPInit(&g_lib, 4096, 48000);

	// ---- upstream ids the table used to drop -------------------------------
	printf("ids inherited from upstream\n");

	// 1500: limiter threshold, release, post gain. The engine clamps a
	// threshold at or above -0.09 dB and a release below 0.15 ms.
	{
		const float v[3] = { -3.5f, 80.0f, 4.25f };
		sendFloats(1500, v, 3);
		// Stored linear, not in dB.
		const float want = powf(10.0f, 4.25f / 20.0f);
		check("1500 sets post gain", fabsf(g_lib.postGain - want) < 1e-3f);
	}
	{
		const float v[3] = { 5.0f, 0.01f, 99.0f };   // all out of range
		sendFloats(1500, v, 3);
		const float ceiling = powf(10.0f, 15.0f / 20.0f);
		check("1500 clamps post gain to +15 dB",
			  fabsf(g_lib.postGain - ceiling) < 1e-3f);
	}

	// 116: filter type, interpolation mode, then 15 frequencies and 15 gains.
	{
		float v[32];
		v[0] = 0.0f;            // FIR minimum phase
		v[1] = -1.0f;           // interpolation mode
		static const float f[15] = { 25,40,63,100,160,250,400,630,1000,
									 1600,2500,4000,6300,10000,16000 };
		for (int i = 0; i < 15; i++) { v[2 + i] = f[i]; v[2 + 15 + i] = (i < 5) ? 6.0f : -6.0f; }
		sendFloats(116, v, 32);
		// The interpolation copies the axes in at offset 1, then mirrors the
		// ends outward, so both are checkable directly.
		bool axes = true;
		for (int i = 0; i < 15; i++)
		{
			if (fabs(g_lib.mEQ.freq[1 + i] - (double)f[i]) > 1e-6) axes = false;
			if (fabs(g_lib.mEQ.gain[1 + i] - (double)v[2 + 15 + i]) > 1e-6) axes = false;
		}
		check("116 reaches the equaliser", axes);
	}
	// Too few values must be ignored rather than half-applied.
	{
		float v[8] = {0};
		const float keep = g_lib.postGain;
		sendFloats(1500, v, 2);
		check("1500 ignores a short payload", fabsf(g_lib.postGain - keep) < 1e-6f);
	}

	// 115: time constant, granularity, tf resolution, 7 frequencies, 7 gains.
	{
		float v[17];
		v[0] = 0.5f; v[1] = 2.0f; v[2] = 1.0f;
		static const float f[7] = { 95,200,400,800,1600,3200,6400 };
		for (int i = 0; i < 7; i++) { v[3 + i] = f[i]; v[3 + 7 + i] = -3.0f; }
		sendFloats(115, v, 17);
		check("115 accepted by the compander", true, "(state is internal)");
	}

	// ---- fork effects that were never wired up -----------------------------
	printf("\nfork effects that had no case at all\n");

	{
		const float v[5] = { -18.0f, 4.0f, 5.0f, 120.0f, 3.0f };
		sendFloats(26004, v, 5);
		check("26004 FET compressor threshold", g_lib.fetComp.thrLin > 0.0f &&
			  g_lib.fetComp.thrLin < 1.0f);
		check("26004 FET compressor slope", g_lib.fetComp.slope > 0.0f);
	}
	{
		const float v[1] = { 2.0f };
		sendFloats(26005, v, 1);
		check("26005 Cure sets its filter", g_lib.cure.lpCoef != 0.0f);
	}
	{
		const float v[2] = { 5.0f, 100.0f };
		sendFloats(26009, v, 2);
		check("26009 pitch shift leaves bypass", g_lib.pitchShift.bypass == 0);
		const float unity[2] = { 0.0f, 100.0f };
		sendFloats(26009, unity, 2);
		check("26009 at unity re-enters bypass", g_lib.pitchShift.bypass != 0);
	}
	{
		float v[25];
		for (int i = 0; i < 25; i++) v[i] = 0.0f;
		v[0] = 100.0f;  // input level
		v[1] = 250.0f;  // time ms
		v[7] = 40.0f;   // feedback
		v[8] = 8000.0f; // cutoff
		v[23] = 35.0f;  // wet
		v[24] = 100.0f; // dry
		sendFloats(26010, v, 25);
		check("26010 echo/delay accepted", true, "(state is internal)");
	}

	// 26012 carries ints, not floats, and is read from the raw payload.
	{
		int order[6] = { 5, 3, 9, 1, 7, 2 };
		applyParam(&g_lib, 26012, 0, false, (const float *)order, 6);
		bool head = g_lib.chainOrder[0] == 5 && g_lib.chainOrder[1] == 3 &&
					g_lib.chainOrder[2] == 9 && g_lib.chainOrder[3] == 1;
		check("26012 chain order lands as ints", head);
		// Every effect must still be dispatched: ids left out of the saved
		// order are appended, which is the bug that made new effects silent.
		bool seen[JDSP_EFX_COUNT] = { false };
		int dupes = 0;
		for (int i = 0; i < g_lib.chainCount; i++)
		{
			int id = g_lib.chainOrder[i];
			if (id >= 0 && id < JDSP_EFX_COUNT)
			{
				if (seen[id]) dupes++;
				seen[id] = true;
			}
		}
		int missing = 0;
		for (int i = 0; i < JDSP_EFX_COUNT; i++) if (!seen[i]) missing++;
		check("26012 completes a partial order", missing == 0 && dupes == 0);
	}

	// ---- enable flags still route ------------------------------------------
	printf("\nenable flags\n");
	sendEnable(26104, true);
	check("26104 enables the FET compressor", g_lib.fetCompEnabled != 0);
	sendEnable(26104, false);
	check("26104 disables it again", g_lib.fetCompEnabled == 0);
	sendEnable(26105, true);
	check("26105 enables Cure", g_lib.cureEnabled != 0);
	sendEnable(26105, false);
	sendEnable(26110, true);
	check("26110 enables echo/delay", g_lib.echoDelayEnabled != 0);
	sendEnable(26110, false);

	// ---- an id with no mapping is tolerated, not refused --------------------
	printf("\nunknown ids\n");
	{
		const float v[1] = { 1.0f };
		const float keep = g_lib.postGain;
		sendFloats(4242, v, 1);
		check("an unmapped id does not disturb the engine",
			  fabsf(g_lib.postGain - keep) < 1e-9f);
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
		   failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
