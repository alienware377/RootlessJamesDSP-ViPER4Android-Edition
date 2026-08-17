// Proves that an effect is dispatched even when the caller's saved chain order
// predates it.
//
// The order is persisted by id. A list written before an effect existed cannot
// mention it, and passing that list through verbatim left the effect enabled,
// configured, and never run - which is what "I hear no difference" turned out
// to mean for two effects in a row.
//
// What is asserted here is the composition of the chain, not the audio. The
// dispatcher is static, and each effect's own harness already proves it changes
// the signal once it is reached; what was missing was any check that it IS
// reached. This links the real jdspController.c against generated stubs, so it
// tests the shipped function rather than a copy of it.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jdsp_header.h"

static JamesDSPLib g_lib;

static int inChain(JamesDSPLib *j, int id)
{
	for (int i = 0; i < j->chainCount; i++)
		if (j->chainOrder[i] == id) return 1;
	return 0;
}

int main(void)
{
	int fail = 0;

	// An order exactly as an older build would have saved it: every effect that
	// existed at the time, and nothing after it.
	int legacy[29];
	for (int i = 0; i < 29; i++) legacy[i] = i;

	printf("== a saved order that predates the newest effects ==\n");
	{
		memset(&g_lib, 0, sizeof(g_lib));
		g_lib.fs = 48000.0f;
		JamesDSPSetChainOrder(&g_lib, legacy, 29);

		printf("  chain length %d, effects declared %d\n",
			g_lib.chainCount, (int)JDSP_EFX_COUNT);

		int missing = 0;
		for (int id = 0; id < JDSP_EFX_COUNT; id++)
		{
			if (!inChain(&g_lib, id))
			{
				printf("  *** id %d never dispatched\n", id);
				missing++;
			}
		}
		printf("  %-42s %s\n", "every declared effect is in the chain",
			missing ? "*** NO ***" : "yes");
		fail |= missing != 0;

		printf("  %-42s %s\n", "multiband distortion dispatched",
			inChain(&g_lib, JDSP_EFX_MULTIBANDDIST) ? "yes" : "*** NO ***");
		printf("  %-42s %s\n", "maximiser dispatched",
			inChain(&g_lib, JDSP_EFX_MAXIMIZER) ? "yes" : "*** NO ***");

		// The order the user actually arranged still has to be respected for
		// the effects they arranged.
		int ordered = 1;
		for (int i = 0; i < 29; i++)
			if (g_lib.chainOrder[i] != legacy[i]) ordered = 0;
		printf("  %-42s %s\n", "saved order preserved", ordered ? "yes" : "*** REORDERED ***");
		fail |= !ordered;
	}

	printf("\n== a reordered saved list keeps its arrangement ==\n");
	{
		// The user's own arrangement, shuffled and short.
		int mine[5] = { 9, 0, 24, 11, 3 };
		memset(&g_lib, 0, sizeof(g_lib));
		g_lib.fs = 48000.0f;
		JamesDSPSetChainOrder(&g_lib, mine, 5);
		int kept = 1;
		for (int i = 0; i < 5; i++)
			if (g_lib.chainOrder[i] != mine[i]) kept = 0;
		printf("  %-42s %s\n", "first five match the saved list", kept ? "yes" : "*** NO ***");
		printf("  %-42s %d\n", "chain length after completion", g_lib.chainCount);
		fail |= !kept || g_lib.chainCount != JDSP_EFX_COUNT;
	}

	printf("\n== duplicates in a saved order must not double a stage ==\n");
	{
		int dupes[6] = { 0, 5, 5, 9, 0, 29 };
		memset(&g_lib, 0, sizeof(g_lib));
		g_lib.fs = 48000.0f;
		JamesDSPSetChainOrder(&g_lib, dupes, 6);
		int counts[JDSP_EFX_COUNT];
		memset(counts, 0, sizeof(counts));
		for (int i = 0; i < g_lib.chainCount; i++) counts[g_lib.chainOrder[i]]++;
		int worst = 0;
		for (int i = 0; i < JDSP_EFX_COUNT; i++) if (counts[i] > worst) worst = counts[i];
		printf("  %-42s %d  %s\n", "most times any effect appears", worst,
			worst == 1 ? "ok" : "*** DUPLICATED ***");
		fail |= worst != 1;
	}

	printf("\n== an empty or absent order falls back to the default chain ==\n");
	{
		memset(&g_lib, 0, sizeof(g_lib));
		g_lib.fs = 48000.0f;
		JamesDSPSetChainOrder(&g_lib, NULL, 0);
		int missing = 0;
		for (int id = 0; id < JDSP_EFX_COUNT; id++)
			if (!inChain(&g_lib, id)) missing++;
		printf("  %-42s %d effects, %d missing  %s\n", "default chain",
			g_lib.chainCount, missing, missing ? "*** INCOMPLETE ***" : "ok");
		fail |= missing != 0;
	}

	printf("\n%s\n", fail ? "FAILURES PRESENT" : "all checks passed");
	return fail ? 1 : 0;
}
