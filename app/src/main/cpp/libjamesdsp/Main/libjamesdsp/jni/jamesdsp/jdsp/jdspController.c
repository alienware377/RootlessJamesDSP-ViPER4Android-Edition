#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include "Effects/eel2/dr_flac.h"
#include "Effects/eel2/ns-eel.h"
#include "jdsp_header.h"
#define TAG "EffectDSPMain"

#ifdef __ANDROID_API__
#if __ANDROID_API__ < __ANDROID_API_J_MR2__
double log2(double x) {
    return (log(x)*1.4426950408889634);
}
#endif
#endif

// Range: 0x800000, 0x7fffff
int32_t i32_from_p24_big_endian(const uint8_t *packed24)
{
	return (packed24[2] << 8) | (packed24[1] << 16) | (packed24[0] << 24);
}
int32_t i32_from_p24_little_endian(const uint8_t *packed24)
{
	return (packed24[0] << 8) | (packed24[1] << 16) | (packed24[2] << 24);
}
void p24_from_i32_big_endian(int32_t ival, uint8_t *packed24)
{
	uint8_t *dst = packed24;
	*dst++ = ival >> 16;
	*dst++ = ival >> 8;
	*dst++ = ival;
}
void p24_from_i32_little_endian(int32_t ival, uint8_t *packed24)
{
	uint8_t *dst = packed24;
	*dst++ = ival;
	*dst++ = ival >> 8;
	*dst++ = ival >> 16;
}
int32_t clamp24_from_float(float f)
{
	static const float scale = (float)(1 << 23);
	float limpos = 0x7fffff / scale;
	float limneg = -0x800000 / scale;
	if (f <= limneg)
		return -0x800000;
	else if (f >= limpos)
		return 0x7fffff;
	f *= scale;
	/* integer conversion is through truncation (though int to float is not).
	 * ensure that we round to nearest, ties away from 0.
	 */
	return f > 0 ? f + 0.5f : f - 0.5f;
}
double crossPlatformCurTime(void)
{
#ifdef WIN32
	DWORD t;

	t = GetTickCount();
	return (double)t * 0.001;
#else
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec * 0.000001;
#endif
}
double getProcTime(int flen, int num, double dur)
{
	int pos;
	double t_start, t_diff;
	int counter = 0.0;
	double proc_time;

	int xlen = 2048 * 2048;
	float *x = (float *)malloc(sizeof(float) * xlen);
	float lin = (float)pow(10.0, -100.0 / 20.0);	// 0.00001 = -100dB
	float mul = (float)pow(lin, 1.0 / (double)xlen);
	x[0] = 1.0;
	for (int n = 1; n < xlen; n++)
		x[n] = -mul * x[n - 1];

	int hlen = flen * num;
	float *h = (float *)malloc(sizeof(float) * hlen);
	lin = (float)pow(10.0, -60.0 / 20.0);	// 0.001 = -60dB
	mul = (float)pow(lin, 1.0 / (double)hlen);
	h[0] = 1.0;
	for (int n = 1; n < hlen; n++)
		h[n] = mul * h[n - 1];

	float *y = (float *)malloc(sizeof(float) * flen);

	FFTConvolver1x1 filter;
	FFTConvolver1x1Init(&filter);
	FFTConvolver1x1LoadImpulseResponse(&filter, flen, h, hlen);

	t_diff = 0.0;
	t_start = crossPlatformCurTime();
	pos = 0;
	while (t_diff < dur)
	{
		FFTConvolver1x1Process(&filter, &x[pos], y, flen);
		pos += flen;
		if (pos >= xlen)
			pos = 0;
		counter++;
		t_diff = crossPlatformCurTime() - t_start;
	}
	FFTConvolver1x1Free(&filter);
	free(x);
	free(h);
	free(y);
	return t_diff / (double)counter;
}
char benchmarkEnable = 0;
char benchmarkCompletionFlag = 0;
double convbench_c0[MAX_BENCHMARK] = { 2.4999999e-05f,4.9999999e-05f,9.9999997e-05f,0.0002f,0.0005f,0.001f,0.0021f,0.0057f,0.0126f,0.0293f };
double convbench_c1[MAX_BENCHMARK] = { 3.1249999e-06f,6.2499998e-06f,1.2500000e-05f,2.4999999e-05f,4.9999999e-05f,9.9999997e-05f,0.0002f,0.0004f,0.0009f,0.0019f };
void JamesDSP_Load_benchmark(double *_c0, double *_c1)
{
	memcpy(convbench_c0, _c0, sizeof(convbench_c0));
	memcpy(convbench_c1, _c1, sizeof(convbench_c1));
	benchmarkCompletionFlag = 1;
	benchmarkEnable = 1;
}
void JamesDSP_Save_benchmark(double *_c0, double *_c1)
{
	memcpy(_c0, convbench_c0, sizeof(convbench_c0));
	memcpy(_c1, convbench_c1, sizeof(convbench_c1));
}
void JamesDSP_Start_benchmark()
{
    benchmarkEnable = 1;

	const int sflen_start = 64;
	int s, m;
	int sflen_best = sflen_start;
	int mflen_best = 256;
	int counter = 0;
	double _c0[MAX_BENCHMARK];
	double _c1[MAX_BENCHMARK];
	for (s = 0; s < MAX_BENCHMARK; s++)
	{
		int sflen = sflen_start << s;
		int num = 1;
		double tau_1 = getProcTime(sflen, num, 1.0);
		counter++;
		num = 16;
		double tau_16 = getProcTime(sflen, num, 1.0);
		counter++;
		_c1[s] = (tau_16 - tau_1) / 15.0;
		_c0[s] = tau_1 - convbench_c1[s];
	}
	JamesDSP_Load_benchmark(_c0, _c1);
#ifdef DEBUG
	__android_log_print(ANDROID_LOG_INFO, TAG, "Benchmark done");
#endif
}
void *convBench(void *arg)
{
	size_t sleepMs = 30000; // 30 second after library being loaded
#ifdef _WIN32
	Sleep(sleepMs);
#else
	usleep(sleepMs * 1000ULL);
#endif
#ifdef DEBUG
	__android_log_print(ANDROID_LOG_INFO, TAG, "Benchmark start");
#endif
	JamesDSP_Start_benchmark();
	pthread_exit(NULL);
	return 0;
}
int selectConvPartitions(JamesDSPLib *jdsp, unsigned int impulseLengthActual, unsigned int *seg2Len)
{
	double tau_s, tau_m;
	int mflen_best = 256;
	int num_s, num_m;
	int latency = jdsp->blockSize;
	int type_best, begin_m, end_m;
	int s = (int)log2(latency) - 6;
	if (s < 0)
	{
		printf("Latency must not smaller than 64\n");
		return 1;
	}
	double cpu_load;
	double cpu_load_best = 1e12;
	// performance prediction with 2 segment lengths
	begin_m = 1;
	end_m = MAX_BENCHMARK - s;
	for (int m = begin_m; m < end_m; m++)
	{
		int mflen = latency << m;
		num_s = 2 * mflen / latency;
		num_m = (int)ceil((impulseLengthActual - num_s * latency) / (double)mflen);
		if (num_m < 1)
			num_m = 1;
		tau_s = convbench_c0[s] + convbench_c1[s] * num_s;
		tau_m = convbench_c0[s + m] + convbench_c1[s + m] * num_m;
		cpu_load = 400.0 * (tau_s * mflen / latency + tau_m) * jdsp->fs / (double)mflen;
		if (cpu_load < cpu_load_best)
		{
			cpu_load_best = cpu_load;
			mflen_best = mflen;
			type_best = 2;
		}
	}
	// performance prediction with 1 segment length
	num_s = (int)ceil(impulseLengthActual / (double)latency);
	tau_s = convbench_c0[s] + convbench_c1[s] * num_s;
	cpu_load = 400.0 * tau_s * jdsp->fs / (double)latency;
	if (cpu_load < cpu_load_best)
	{
		cpu_load_best = cpu_load;
		type_best = 1;
	}
	*seg2Len = mflen_best;
	return type_best;
}
// This memory - the EEL tables and the resampler coefficient tables - is
// process-wide rather than per instance, but setup and teardown were both
// unguarded. The app defers the native free onto a timer while the service is
// already building the replacement engine, so every restart briefly has two
// instances alive: the outgoing one's teardown frees the coefficients the
// incoming one is in the middle of generating, and the generator then writes
// into freed memory and faults on the reload afterwards. Counted, and
// serialised as well, because two generators must not overlap each other
// either - generating starts by freeing whatever was there.
static pthread_mutex_t globalMemMutex = PTHREAD_MUTEX_INITIALIZER;
static int globalMemRefCount = 0;

void JamesDSPGlobalMemoryAllocation()
{
	// Stays outside the guard: the benchmark result is per instance, and a
	// second engine must not inherit the first one's completed flag.
	benchmarkCompletionFlag = 0;
	pthread_mutex_lock(&globalMemMutex);
	if (globalMemRefCount++ == 0)
	{
		NSEEL_start();
#ifdef JAMESDSP_REFERENCE_IMPL
		pthread_t benchmarkThread;
		pthread_create(&benchmarkThread, NULL, convBench, 0);
#endif
	}
	pthread_mutex_unlock(&globalMemMutex);
}
void JamesDSPGlobalMemoryDeallocation()
{
	pthread_mutex_lock(&globalMemMutex);
	// An unmatched deallocation is deliberately a no-op rather than a second
	// free: the system-effect path allocates and never deallocates.
	if (globalMemRefCount > 0 && --globalMemRefCount == 0)
		NSEEL_quit();
	pthread_mutex_unlock(&globalMemMutex);
}
unsigned int next_pow_2(unsigned int x)
{
	if (x <= 1) return 1;
	int power = 2;
	x--;
	while (x >>= 1) power <<= 1;
	return power;
}
void JamesDSPReallocateBlock(JamesDSPLib *jdsp, size_t n)
{
	// Init buffer
	float *tmp1 = jdsp->tmpBuffer[0];
	float *tmp2 = jdsp->tmpBuffer[1];
	float *tmp3 = jdsp->tmpBuffer[2];
	float *tmp4 = jdsp->tmpBuffer[3];
	float *tmp5 = jdsp->tmpBuffer[4];
	float *tmp6 = jdsp->tmpBuffer[5];
	double ratio = (double)jdsp->fs / (double)jdsp->trueSampleRate;
	jdsp->blockSizeMax = n;
	unsigned int maxDecimatedLength = (unsigned int)ceil(jdsp->blockSizeMax * ratio);
	if (!jdsp->enableASRC)
		maxDecimatedLength = 0;
	unsigned int maxInterpolatedLength = (unsigned int)ceil(maxDecimatedLength / ratio);
	jdsp->pw2BlockMemSize = next_pow_2(maxInterpolatedLength);
	if (!jdsp->enableASRC)
		jdsp->pw2BlockMemSize = 0;
	size_t ctMemBlk = jdsp->blockSizeMax * 2 + maxInterpolatedLength * 2 + jdsp->pw2BlockMemSize * 2;
	jdsp->tmpBuffer[0] = (float *)malloc(ctMemBlk * sizeof(float));
	jdsp->tmpBuffer[1] = jdsp->tmpBuffer[0] + jdsp->blockSizeMax;
	jdsp->tmpBuffer[2] = jdsp->tmpBuffer[1] + jdsp->blockSizeMax;
	jdsp->tmpBuffer[3] = jdsp->tmpBuffer[2] + maxInterpolatedLength;
	jdsp->tmpBuffer[4] = jdsp->tmpBuffer[3] + maxInterpolatedLength;
	jdsp->tmpBuffer[5] = jdsp->tmpBuffer[4] + jdsp->pw2BlockMemSize;
	if (tmp1)
		free(tmp1);
}
void JamesDSPRefreshConvolutions(JamesDSPLib *jdsp, char refreshAll)
{
    // Temporary(?) bug fix when benchmarks are off
   if(!benchmarkEnable) {
#ifdef DEBUG
        __android_log_print(ANDROID_LOG_INFO, TAG, "ignoring call to JamesDSPRefreshConvolutions(...) because benchmarks are disabled");
#endif
        return;
    }

#ifdef DEBUG
	__android_log_print(ANDROID_LOG_INFO, TAG, "Buffer size changed, update convolution object to maximize performance");
#endif
	if (jdsp->impulseResponseStorage.impulseResponse)
		Convolver1DLoadImpulseResponse(jdsp, jdsp->impulseResponseStorage.impulseResponse, jdsp->impulseResponseStorage.impChannels, jdsp->impulseResponseStorage.impulseLengthActual, 0);
	jdsp->crossfeedForceRefresh = 1;
	CrossfeedEnable(jdsp, jdsp->crossfeedEnabled);
	if (refreshAll)
	{
		jdsp->arbMagForceRefresh = 1;
		ArbitraryResponseEqualizerEnable(jdsp, jdsp->arbitraryMagEnabled);
		jdsp->equalizerForceRefresh = 1;
		MultimodalEqualizerEnable(jdsp, jdsp->equalizerEnabled);
	}
}
void jdsp_lock(JamesDSPLib *jdsp)
{
	if (jdsp->isMutexSuccess)
		pthread_mutex_lock(&jdsp->m_in_processing);
}
void jdsp_unlock(JamesDSPLib *jdsp)
{
	if (jdsp->isMutexSuccess)
		pthread_mutex_unlock(&jdsp->m_in_processing);
}
// Default processing order, matching the classic fixed chain
static const int jdspDefaultChain[] =
{
	JDSP_EFX_TUBE, JDSP_EFX_COMPRESSOR, JDSP_EFX_PITCHSHIFT, JDSP_EFX_FETCOMP,
	// Attack shaping belongs with the dynamics, before anything spatial.
	JDSP_EFX_TRANSIENT,
	JDSP_EFX_DIFFSURROUND, JDSP_EFX_BASSBOOST, JDSP_EFX_VDYNBASS,
	JDSP_EFX_VIPERBASS, JDSP_EFX_BASSEX,
	// Tidy the bottom after the bass processors have had their say, then add
	// harmonics to what is left.
	JDSP_EFX_LOWEND, JDSP_EFX_EXCITER,
	JDSP_EFX_EQUALIZER,
	JDSP_EFX_ARBITRARYMAG,
	// Corrective EQ sits with the rest of the EQ block.
	JDSP_EFX_DYNAMICEQ,
	JDSP_EFX_CONVOLVER, JDSP_EFX_DDC,
	JDSP_EFX_LIVEPROG, JDSP_EFX_LIVEPROG2, JDSP_EFX_LIVEPROG3,
	JDSP_EFX_LIVEPROG4, JDSP_EFX_CROSSFEED, JDSP_EFX_CURE,
	// Width before the surround effects, so they act on the intended image.
	JDSP_EFX_IMAGING,
	JDSP_EFX_STEREOWIDE, JDSP_EFX_FIELDSURROUND, JDSP_EFX_HPSURROUND,
	JDSP_EFX_SPECTRUMEXT, JDSP_EFX_CLARITY, JDSP_EFX_AGC,
	JDSP_EFX_SPEAKEROPT, JDSP_EFX_REVERB, JDSP_EFX_VREVERB, JDSP_EFX_ECHODELAY, JDSP_EFX_MULTIBANDDIST,
	// Colour last, but still ahead of the limiter.
	JDSP_EFX_TAPE,
	JDSP_EFX_MAXIMIZER
};

void JamesDSPResetChainOrder(JamesDSPLib *jdsp)
{
	char seen[JDSP_EFX_COUNT];
	memset(seen, 0, sizeof(seen));
	int count = (int)(sizeof(jdspDefaultChain) / sizeof(jdspDefaultChain[0]));
	if (count > JDSP_EFX_MAX)
		count = JDSP_EFX_MAX;
	int written = 0;
	for (int i = 0; i < count; i++)
	{
		const int id = jdspDefaultChain[i];
		if (id < 0 || id >= JDSP_EFX_COUNT || seen[id])
			continue;
		seen[id] = 1;
		jdsp->chainOrder[written++] = id;
	}
	/* Anything the list above forgot is appended, exactly as a saved order is
	   completed. The same reasoning was already written down for saved orders
	   and not applied here, and this is where it actually bit: the six effects
	   added most recently were never put in the default list, so every install
	   that had not customised its chain ran without them. They were enabled,
	   configured and receiving their parameters - simply never dispatched,
	   which from the user's side is indistinguishable from six broken cards.

	   Appending rather than asserting because a chain missing an effect is
	   worse than a chain with one in an unconsidered place; the position can be
	   argued about afterwards, silence cannot be noticed. */
	for (int id = 0; id < JDSP_EFX_COUNT && written < JDSP_EFX_MAX; id++)
	{
		if (!seen[id])
			jdsp->chainOrder[written++] = id;
	}
	jdsp->chainCount = written;
}

void JamesDSPSetChainOrder(JamesDSPLib *jdsp, const int *order, int count)
{
	if (!order || count <= 0)
	{
		JamesDSPResetChainOrder(jdsp);
		return;
	}
	if (count > JDSP_EFX_MAX)
		count = JDSP_EFX_MAX;
	char seen[JDSP_EFX_COUNT];
	memset(seen, 0, sizeof(seen));
	int written = 0;
	for (int i = 0; i < count && written < JDSP_EFX_MAX; i++)
	{
		int id = order[i];
		if (id < 0 || id >= JDSP_EFX_COUNT)
			continue;
		if (seen[id])
			continue;
		seen[id] = 1;
		jdsp->chainOrder[written++] = id;
	}
	// Anything the caller left out is appended in declaration order, so the
	// chain is always complete.
	//
	// This is not defensive tidying. The order is persisted by id, so a list
	// saved by an older build cannot mention an effect that did not exist when
	// it was written - and passing that list through verbatim left every newly
	// added effect enabled, configured, receiving its parameters, and never
	// dispatched. From the user's side that is indistinguishable from the
	// effect being broken, and it applied silently to every effect added after
	// the first time they arranged the chain.
	for (int id = 0; id < JDSP_EFX_COUNT && written < JDSP_EFX_MAX; id++)
	{
		if (!seen[id])
			jdsp->chainOrder[written++] = id;
	}
	if (!written)
	{
		JamesDSPResetChainOrder(jdsp);
		return;
	}
	jdsp->chainCount = written;
}

// Runs a single effect if it is enabled. Convolver, DDC and Liveprog touch
// state that can be swapped from another thread, so they take the lock.
static void jdspDispatchEffect(JamesDSPLib *jdsp, int id, size_t n)
{
	switch (id)
	{
	case JDSP_EFX_TUBE:
		if (jdsp->tubeEnabled) VacuumTubeProcess(jdsp, n);
		break;
	case JDSP_EFX_COMPRESSOR:
		if (jdsp->compEnabled) CompressorProcess(jdsp, n);
		break;
	case JDSP_EFX_PITCHSHIFT:
		jdsp_lock(jdsp);
		if (jdsp->pitchShiftEnabled) PitchShiftProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_FETCOMP:
		if (jdsp->fetCompEnabled) FetCompProcess(jdsp, n);
		break;
	case JDSP_EFX_DIFFSURROUND:
		jdsp_lock(jdsp);
		if (jdsp->diffSurroundEnabled) DiffSurroundProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_BASSBOOST:
		if (jdsp->bassBoostEnabled) BassBoostProcess(jdsp, n);
		break;
	case JDSP_EFX_VDYNBASS:
		if (jdsp->vdynBassEnabled) VDynBassProcess(jdsp, n);
		break;
	case JDSP_EFX_VIPERBASS:
		if (jdsp->viperBassEnabled) ViperBassProcess(jdsp, n);
		break;
	case JDSP_EFX_BASSEX:
		if (jdsp->bassExEnabled) BassExciterProcess(jdsp, n);
		break;
	case JDSP_EFX_EQUALIZER:
		if (jdsp->equalizerEnabled) MultimodalEqualizerProcess(jdsp, n);
		break;
	case JDSP_EFX_ARBITRARYMAG:
		if (jdsp->arbitraryMagEnabled) ArbitraryResponseEqualizerProcess(jdsp, n);
		break;
	case JDSP_EFX_CONVOLVER:
		jdsp_lock(jdsp);
		if (jdsp->convolverEnabled && jdsp->conv.process)
			jdsp->conv.process(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_DDC:
		jdsp_lock(jdsp);
		if (jdsp->ddcEnabled) DDCProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_LIVEPROG:
		jdsp_lock(jdsp);
		if (jdsp->liveprogEnabled) LiveProgProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_LIVEPROG2:
	case JDSP_EFX_LIVEPROG3:
	case JDSP_EFX_LIVEPROG4:
	{
		int slot = id - JDSP_EFX_LIVEPROG;
		jdsp_lock(jdsp);
		if (slot >= 1 && slot <= JDSP_LIVEPROG_EXTRA &&
			jdsp->liveprogExtraEnabled[slot - 1])
			LiveProgProcessSlot(jdsp, slot, n);
		jdsp_unlock(jdsp);
		break;
	}
	case JDSP_EFX_CROSSFEED:
		if (jdsp->crossfeedEnabled) CrossfeedProcess(jdsp, n);
		break;
	case JDSP_EFX_CURE:
		if (jdsp->cureEnabled) CureProcess(jdsp, n);
		break;
	case JDSP_EFX_STEREOWIDE:
		if (jdsp->sterEnhEnabled) StereoEnhancementProcess(jdsp, n);
		break;
	case JDSP_EFX_FIELDSURROUND:
		if (jdsp->fieldSurroundEnabled) FieldSurroundProcess(jdsp, n);
		break;
	case JDSP_EFX_HPSURROUND:
		jdsp_lock(jdsp);
		if (jdsp->hpSurroundEnabled) HpSurroundProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_SPECTRUMEXT:
		if (jdsp->spectrumExtEnabled) SpectrumExtensionProcess(jdsp, n);
		break;
	case JDSP_EFX_CLARITY:
		if (jdsp->viperClarityEnabled) ViperClarityProcess(jdsp, n);
		break;
	case JDSP_EFX_AGC:
		if (jdsp->agcEnabled) AgcProcess(jdsp, n);
		break;
	case JDSP_EFX_SPEAKEROPT:
		if (jdsp->speakerOptEnabled) SpeakerOptProcess(jdsp, n);
		break;
	case JDSP_EFX_REVERB:
		if (jdsp->reverbEnabled) ReverbProcess(jdsp, n);
		break;
	case JDSP_EFX_VREVERB:
		jdsp_lock(jdsp);
		if (jdsp->vreverbEnabled) VReverbProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_ECHODELAY:
		jdsp_lock(jdsp);
		if (jdsp->echoDelayEnabled) EchoDelayProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_MULTIBANDDIST:
		jdsp_lock(jdsp);
		if (jdsp->multibandDistEnabled) MultibandDistProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_MAXIMIZER:
		jdsp_lock(jdsp);
		if (jdsp->maximizerEnabled) MaximizerProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_DYNAMICEQ:
		jdsp_lock(jdsp);
		if (jdsp->dynamicEqEnabled) DynamicEqProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_IMAGING:
		jdsp_lock(jdsp);
		if (jdsp->imagingEnabled) ImagingProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_TRANSIENT:
		jdsp_lock(jdsp);
		if (jdsp->transientEnabled) TransientProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_LOWEND:
		jdsp_lock(jdsp);
		if (jdsp->lowEndEnabled) LowEndProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_EXCITER:
		jdsp_lock(jdsp);
		if (jdsp->exciterEnabled) ExciterProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	case JDSP_EFX_TAPE:
		jdsp_lock(jdsp);
		if (jdsp->tapeEnabled) TapeProcess(jdsp, n);
		jdsp_unlock(jdsp);
		break;
	default:
		break;
	}
}

// Process
void JamesDSPProcess(JamesDSPLib *jdsp, size_t n)
{
	// Run the effects in the user-defined order
	for (int chainIndex = 0; chainIndex < jdsp->chainCount; chainIndex++)
		jdspDispatchEffect(jdsp, jdsp->chainOrder[chainIndex], n);
	// Output
	for (size_t i = 0; i < n; i++)
	{
		float xL = jdsp->tmpBuffer[0][i] * jdsp->postGain;
		float xR = jdsp->tmpBuffer[1][i] * jdsp->postGain;
		if (jdsp->limiter.mode == 2)
		{
			// Limiter off
			jdsp->tmpBuffer[0][i] = xL;
			jdsp->tmpBuffer[1][i] = xR;
		}
		else if (jdsp->limiter.mode == 1)
		{
			// Soft saturation (loudness maximizer style)
			float th = jdsp->limiter.threshold;
			jdsp->tmpBuffer[0][i] = th * tanhf(xL / th);
			jdsp->tmpBuffer[1][i] = th * tanhf(xR / th);
		}
		else if (jdsp->limiter.mode == 3)
		{
			/* ViPER classic: the original V4A SoftwareLimiter behaviour.
			   A 256-sample lookahead lets the smoothed attack duck just in
			   time; the windowed maximum keeps low frequencies from rippling
			   the gain; the final clamp hard-catches the residue, which is
			   the subtle transient "bite" V4A was known for. */
			JLimiter *lim = &jdsp->limiter;
			float r1 = fabsf(xL);
			float r2 = fabsf(xR);
			float m = r1 > r2 ? r1 : r2;
			int leaf = 256 + lim->vIdx;
			lim->vTree[leaf] = m;
			for (leaf >>= 1; leaf >= 1; leaf >>= 1)
			{
				float a = lim->vTree[leaf << 1];
				float b = lim->vTree[(leaf << 1) | 1];
				lim->vTree[leaf] = a > b ? a : b;
			}
			float wmax = lim->vTree[1];
			float gate = lim->threshold;
			float target = (wmax > gate) ? (gate / wmax) : 1.0f;
			lim->vAtt = target * 0.0999f + lim->vAtt * 0.8999f;
			float rel = lim->vGain + (1.0f - lim->relCoef) * (1.0f - lim->vGain);
			lim->vGain = lim->vAtt < rel ? lim->vAtt : rel;
			int rd = (lim->vIdx + 1) & 255;
			float dL = lim->vLook[0][rd];
			float dR = lim->vLook[1][rd];
			lim->vLook[0][lim->vIdx] = xL;
			lim->vLook[1][lim->vIdx] = xR;
			lim->vIdx = rd;
			float oL = dL * lim->vGain;
			float oR = dR * lim->vGain;
			float om = fabsf(oL) > fabsf(oR) ? fabsf(oL) : fabsf(oR);
			if (om >= gate)
			{
				float dm = fabsf(dL) > fabsf(dR) ? fabsf(dL) : fabsf(dR);
				float g2 = (dm > 1e-9f) ? (gate / dm) : 1.0f;
				lim->vGain = g2;
				oL = dL * g2;
				oR = dR * g2;
			}
			jdsp->tmpBuffer[0][i] = oL;
			jdsp->tmpBuffer[1][i] = oR;
		}
		else
		{
			float rect1 = fabsf(xL);
			float rect2 = fabsf(xR);
			float maxLR = max(rect1, rect2);
			if (maxLR < jdsp->limiter.threshold)
				maxLR = jdsp->limiter.threshold;
			if (maxLR > jdsp->limiter.envOverThreshold)
				jdsp->limiter.envOverThreshold = maxLR;
			else
				jdsp->limiter.envOverThreshold = maxLR + jdsp->limiter.relCoef * (jdsp->limiter.envOverThreshold - maxLR);
			float gR = jdsp->limiter.threshold / jdsp->limiter.envOverThreshold;
			jdsp->tmpBuffer[0][i] = xL * gR;
			jdsp->tmpBuffer[1][i] = xR * gR;
		}
	}
}
void JamesDSPProcessCheckBenchmarkReady(JamesDSPLib *jdsp, size_t n)
{
	// Run the effects in the user-defined order
	for (int chainIndex = 0; chainIndex < jdsp->chainCount; chainIndex++)
		jdspDispatchEffect(jdsp, jdsp->chainOrder[chainIndex], n);
	// Output
	for (size_t i = 0; i < n; i++)
	{
		float xL = jdsp->tmpBuffer[0][i] * jdsp->postGain;
		float xR = jdsp->tmpBuffer[1][i] * jdsp->postGain;
		if (jdsp->limiter.mode == 2)
		{
			// Limiter off
			jdsp->tmpBuffer[0][i] = xL;
			jdsp->tmpBuffer[1][i] = xR;
		}
		else if (jdsp->limiter.mode == 1)
		{
			// Soft saturation (loudness maximizer style)
			float th = jdsp->limiter.threshold;
			jdsp->tmpBuffer[0][i] = th * tanhf(xL / th);
			jdsp->tmpBuffer[1][i] = th * tanhf(xR / th);
		}
		else if (jdsp->limiter.mode == 3)
		{
			/* ViPER classic: the original V4A SoftwareLimiter behaviour.
			   A 256-sample lookahead lets the smoothed attack duck just in
			   time; the windowed maximum keeps low frequencies from rippling
			   the gain; the final clamp hard-catches the residue, which is
			   the subtle transient "bite" V4A was known for. */
			JLimiter *lim = &jdsp->limiter;
			float r1 = fabsf(xL);
			float r2 = fabsf(xR);
			float m = r1 > r2 ? r1 : r2;
			int leaf = 256 + lim->vIdx;
			lim->vTree[leaf] = m;
			for (leaf >>= 1; leaf >= 1; leaf >>= 1)
			{
				float a = lim->vTree[leaf << 1];
				float b = lim->vTree[(leaf << 1) | 1];
				lim->vTree[leaf] = a > b ? a : b;
			}
			float wmax = lim->vTree[1];
			float gate = lim->threshold;
			float target = (wmax > gate) ? (gate / wmax) : 1.0f;
			lim->vAtt = target * 0.0999f + lim->vAtt * 0.8999f;
			float rel = lim->vGain + (1.0f - lim->relCoef) * (1.0f - lim->vGain);
			lim->vGain = lim->vAtt < rel ? lim->vAtt : rel;
			int rd = (lim->vIdx + 1) & 255;
			float dL = lim->vLook[0][rd];
			float dR = lim->vLook[1][rd];
			lim->vLook[0][lim->vIdx] = xL;
			lim->vLook[1][lim->vIdx] = xR;
			lim->vIdx = rd;
			float oL = dL * lim->vGain;
			float oR = dR * lim->vGain;
			float om = fabsf(oL) > fabsf(oR) ? fabsf(oL) : fabsf(oR);
			if (om >= gate)
			{
				float dm = fabsf(dL) > fabsf(dR) ? fabsf(dL) : fabsf(dR);
				float g2 = (dm > 1e-9f) ? (gate / dm) : 1.0f;
				lim->vGain = g2;
				oL = dL * g2;
				oR = dR * g2;
			}
			jdsp->tmpBuffer[0][i] = oL;
			jdsp->tmpBuffer[1][i] = oR;
		}
		else
		{
			float rect1 = fabsf(xL);
			float rect2 = fabsf(xR);
			float maxLR = max(rect1, rect2);
			if (maxLR < jdsp->limiter.threshold)
				maxLR = jdsp->limiter.threshold;
			if (maxLR > jdsp->limiter.envOverThreshold)
				jdsp->limiter.envOverThreshold = maxLR;
			else
				jdsp->limiter.envOverThreshold = maxLR + jdsp->limiter.relCoef * (jdsp->limiter.envOverThreshold - maxLR);
			float gR = jdsp->limiter.threshold / jdsp->limiter.envOverThreshold;
			jdsp->tmpBuffer[0][i] = xL * gR;
			jdsp->tmpBuffer[1][i] = xR * gR;
		}
	}
	if (benchmarkCompletionFlag == 1)
	{
#ifdef DEBUG
		__android_log_print(ANDROID_LOG_INFO, TAG, "Benchmark flag == 1, refreshing convolutions");
#endif
		JamesDSPRefreshConvolutions(jdsp, 0);
		jdsp->processInternal = JamesDSPProcess;
	}
}
size_t iabs(size_t value)
{
	return value < 0 ? 0 - value : value;
}
void sample_ratio(unsigned long long numerator, unsigned long long denominator, unsigned long long *num, unsigned long long *denom)
{
	unsigned long long largest = numerator > denominator ? numerator : denominator;
	unsigned long long gcd = 0; // greatest common divisor
	for (unsigned long long i = 2; i <= largest; i++) // initializes i to 2, if i is less that or equal to the largest, then increment the i counter.
		if (numerator % i == 0 && denominator % i == 0) //if the i's remainder equals 0 and the denominator's remainder equals 0,
			gcd = i; // then the greatest common denominator is assigned the value of the i.
	if (gcd != 0) // if the greatest common denominator does not equal 0, the divide both the numerator and the denominator by the greatest common denominator.
	{
		*num = numerator / gcd;
		*denom = denominator / gcd;
	}
	else
	{
		*num = numerator;
		*denom = denominator;
	}
}
void RingBuffer_Init(RingBuffer *fifo)
{
	fifo->in = fifo->out = 0;
}
uint32_t RingBuffering(RingBuffer *fifo, const float *buffer, const float *in, uint32_t lenIn, float *out, uint32_t lenOut, uint32_t size)
{
	lenIn = min(lenIn, size - (fifo->in - fifo->out));
	// First put the data starting from fifo->in to buffer end
	uint32_t l_In = min(lenIn, size - (fifo->in & (size - 1)));
	memcpy(buffer + (fifo->in & (size - 1)), in, l_In * sizeof(float));
	// Then put the rest (if any) at the beginning of the buffer
	memcpy(buffer, in + l_In, (lenIn - l_In) * sizeof(float));
	fifo->in += lenIn;
	lenOut = min(lenOut, fifo->in - fifo->out);
	unsigned int lenOut_2 = min(lenOut, fifo->in - fifo->out);
	// First get the data from fifo->out until the end of the buffer
	uint32_t l_Out = min(lenOut, size - (fifo->out & (size - 1)));
	memcpy(out, buffer + (fifo->out & (size - 1)), l_Out * sizeof(float));
	// Then get the rest (if any) from the beginning of the buffer
	memcpy(out + l_Out, buffer, (lenOut - l_Out) * sizeof(float));
	fifo->out += lenOut;
	if (fifo->in > size || fifo->out > size)
	{
		fifo->in = fifo->in - size;
		fifo->out = fifo->out - size;
	}
	return lenOut;
}
uint32_t RingBufferingStereo(RingBuffer *fifo[2], const float *buffer1, const float *buffer2, const float *in1, const float *in2, uint32_t lenIn, float *out1, float *out2, uint32_t lenOut, uint32_t size)
{
	lenIn = min(lenIn, size - (fifo[0]->in - fifo[0]->out));
	// First put the data starting from fifo[0]->in to buffer end
	uint32_t l_In = min(lenIn, size - (fifo[0]->in & (size - 1)));
	memcpy(buffer1 + (fifo[0]->in & (size - 1)), in1, l_In * sizeof(float));
	memcpy(buffer2 + (fifo[0]->in & (size - 1)), in2, l_In * sizeof(float));
	// Then put the rest (if any) at the beginning of the buffer
	memcpy(buffer1, in1 + l_In, (lenIn - l_In) * sizeof(float));
	memcpy(buffer2, in2 + l_In, (lenIn - l_In) * sizeof(float));
	fifo[0]->in += lenIn;
	lenOut = min(lenOut, fifo[0]->in - fifo[0]->out);
	unsigned int lenOut_2 = min(lenOut, fifo[0]->in - fifo[0]->out);
	// First get the data from fifo[0]->out until the end of the buffer
	uint32_t l_Out = min(lenOut, size - (fifo[0]->out & (size - 1)));
	memcpy(out1, buffer1 + (fifo[0]->out & (size - 1)), l_Out * sizeof(float));
	memcpy(out2, buffer2 + (fifo[0]->out & (size - 1)), l_Out * sizeof(float));
	// Then get the rest (if any) from the beginning of the buffer
	memcpy(out1 + l_Out, buffer1, (lenOut - l_Out) * sizeof(float));
	memcpy(out2 + l_Out, buffer2, (lenOut - l_Out) * sizeof(float));
	fifo[0]->out += lenOut;
	if (fifo[0]->in > size || fifo[0]->out > size)
	{
		fifo[0]->in = fifo[0]->in - size;
		fifo[0]->out = fifo[0]->out - size;
	}
	return lenOut;
}
void InitIntegerASRCHandler(IntegerASRCHandler *asrc, unsigned long long workingFs, unsigned long long inFs, unsigned int polyphaseFIRTaps, char minphase, SRCResampler *inst1, SRCResampler *inst2)
{
	double ratio = (double)workingFs / (double)inFs;
	unsigned int minimumInputBufLen = (unsigned int)ceil((double)inFs / (double)workingFs);
	asrc->calculatedLatencyWholeSystem = minimumInputBufLen;
	unsigned long long num, denom;
	sample_ratio(workingFs, inFs, &num, &denom);
	if ((workingFs == num && inFs == denom) || num > 2000)
		minphase = 0;
	unsigned int maxDecimatedLength = (unsigned int)ceil(asrc->calculatedLatencyWholeSystem * ratio);
	if (!inst1)
		psrc_generate(&asrc->polyphaseDecimator, num, denom, polyphaseFIRTaps, 0.99, minphase);
	else
		psrc_clone(&asrc->polyphaseDecimator, inst1);
	if (!inst2)
		psrc_generate(&asrc->polyphaseInterpolator, denom, num, polyphaseFIRTaps, 0.99, minphase);
	else
		psrc_clone(&asrc->polyphaseInterpolator, inst2);
	//
	unsigned int maxInterpolatedLength = (unsigned int)ceil(maxDecimatedLength / ratio);
	RingBuffer_Init(&asrc->intermediateRing);
}
void FreeIntegerASRCHandler(IntegerASRCHandler *asrc)
{
	psrc_free(&asrc->polyphaseDecimator);
	psrc_free(&asrc->polyphaseInterpolator);
}
unsigned int DoASRC_fwd(JamesDSPLib *jdsp, size_t n)
{
	//unsigned int curDecimatedLen = psrc_filt(&jdsp->asrc[0].polyphaseDecimator, jdsp->tmpBuffer[0], n, jdsp->tmpBuffer[2]);
	//curDecimatedLen = psrc_filt(&jdsp->asrc[1].polyphaseDecimator, jdsp->tmpBuffer[1], n, jdsp->tmpBuffer[3]);
	SRCResampler *ptr[2] = { &jdsp->asrc[0].polyphaseDecimator, &jdsp->asrc[1].polyphaseDecimator };
	unsigned int curDecimatedLen = psrc_filt_stereo(ptr, jdsp->tmpBuffer[0], jdsp->tmpBuffer[1], n, jdsp->tmpBuffer[2], jdsp->tmpBuffer[3]);
	for (unsigned int i = 0; i < curDecimatedLen; i++)
	{
		jdsp->tmpBuffer[0][i] = jdsp->tmpBuffer[2][i];
		jdsp->tmpBuffer[1][i] = jdsp->tmpBuffer[3][i];
	}
	return curDecimatedLen;
}
void DoASRC_bwd(JamesDSPLib *jdsp, unsigned int curDecimatedLen, size_t n)
{
	//unsigned int curInterpolatedLen = psrc_filt(&jdsp->asrc[0].polyphaseInterpolator, jdsp->tmpBuffer[0], curDecimatedLen, jdsp->tmpBuffer[2]);
	//curInterpolatedLen = psrc_filt(&jdsp->asrc[1].polyphaseInterpolator, jdsp->tmpBuffer[1], curDecimatedLen, jdsp->tmpBuffer[3]);
	SRCResampler *ptr[2] = { &jdsp->asrc[0].polyphaseInterpolator, &jdsp->asrc[1].polyphaseInterpolator };
	unsigned int curInterpolatedLen = psrc_filt_stereo(ptr, jdsp->tmpBuffer[0], jdsp->tmpBuffer[1], curDecimatedLen, jdsp->tmpBuffer[2], jdsp->tmpBuffer[3]);
	//unsigned int dequeued = RingBuffering(&jdsp->asrc[0].intermediateRing, jdsp->tmpBuffer[4], jdsp->tmpBuffer[2], curInterpolatedLen, jdsp->tmpBuffer[0], n, jdsp->pw2BlockMemSize);
	//dequeued = RingBuffering(&jdsp->asrc[1].intermediateRing, jdsp->tmpBuffer[5], jdsp->tmpBuffer[3], curInterpolatedLen, jdsp->tmpBuffer[1], n, jdsp->pw2BlockMemSize);
	RingBuffer *ptr2[2] = { &jdsp->asrc[0].intermediateRing, &jdsp->asrc[1].intermediateRing };
	unsigned int dequeued = RingBufferingStereo(ptr2, jdsp->tmpBuffer[4], jdsp->tmpBuffer[5], jdsp->tmpBuffer[2], jdsp->tmpBuffer[3], curInterpolatedLen, jdsp->tmpBuffer[0], jdsp->tmpBuffer[1], n, jdsp->pw2BlockMemSize);
	const int howManyItemsLeft1 = (int)jdsp->asrc[0].intermediateRing.in - (int)jdsp->asrc[0].intermediateRing.out;
	const int howManyItemsLeft2 = (int)jdsp->asrc[0].intermediateRing.in - (int)jdsp->asrc[0].intermediateRing.out;
}
void pint16(JamesDSPLib *jdsp, int16_t *x1, int16_t *x2, int16_t *y1, int16_t *y2, size_t n)
{
	if (jdsp->blockSizeMax < n)
		JamesDSPReallocateBlock(jdsp, n);
	if (jdsp->blockSize != n)
	{
		jdsp->blockSize = n;
		JamesDSPRefreshConvolutions(jdsp, 1);
	}
	static const float scale = (float)(1UL << 15UL);
	static const float offset = (float)(3 << (22 - 15));
	/* zero = (0x10f << 22) =  0x43c00000 (not directly used) */
	static const int32_t limneg = (0x10f << 22) /*zero*/ - 32768; /* 0x43bf8000 */
	static const int32_t limpos = (0x10f << 22) /*zero*/ + 32767; /* 0x43c07fff */
	union {
		float f;
		int32_t i;
	} u;
	for (size_t i = 0; i < n; i++)
	{
		jdsp->tmpBuffer[0][i] = x1[i] / scale;
		jdsp->tmpBuffer[1][i] = x2[i] / scale;
	}
	if (jdsp->enableASRC)
	{
		unsigned int curDecimatedLen = DoASRC_fwd(jdsp, n);
		jdsp->processInternal(jdsp, curDecimatedLen);
		DoASRC_bwd(jdsp, curDecimatedLen, n);
	}
	else
		jdsp->processInternal(jdsp, n);
	for (size_t i = 0; i < n; i++)
	{
		u.f = jdsp->tmpBuffer[0][i] + offset;
		if (u.i < limneg)
			u.i = -32768;
		else if (u.i > limpos)
			u.i = 32767;
		y1[i] = u.i;
		u.f = jdsp->tmpBuffer[1][i] + offset;
		if (u.i < limneg)
			u.i = -32768;
		else if (u.i > limpos)
			u.i = 32767;
		y2[i] = u.i;
	}
}
void pint16Multiplexed(JamesDSPLib *jdsp, int16_t *x, int16_t *y, size_t n)
{
	if (jdsp->blockSizeMax < n)
		JamesDSPReallocateBlock(jdsp, n);
	if (jdsp->blockSize != n)
	{
		jdsp->blockSize = n;
		JamesDSPRefreshConvolutions(jdsp, 1);
	}
	static const float offset = (float)(3 << (22 - 15));
	/* zero = (0x10f << 22) =  0x43c00000 (not directly used) */
	static const int32_t limneg = (0x10f << 22) /*zero*/ - 32768; /* 0x43bf8000 */
	static const int32_t limpos = (0x10f << 22) /*zero*/ + 32767; /* 0x43c07fff */
	union {
		float f;
		int32_t i;
	} u;
	for (size_t i = 0; i < n; i++)
	{
		jdsp->tmpBuffer[0][i] = x[i << 1] * 0.000030517578125f;
		jdsp->tmpBuffer[1][i] = x[(i << 1) + 1] * 0.000030517578125f;
	}
	if (jdsp->enableASRC)
	{
		unsigned int curDecimatedLen = DoASRC_fwd(jdsp, n);
		jdsp->processInternal(jdsp, curDecimatedLen);
		DoASRC_bwd(jdsp, curDecimatedLen, n);
	}
	else
		jdsp->processInternal(jdsp, n);
	for (size_t i = 0; i < n; i++)
	{
		u.f = jdsp->tmpBuffer[0][i] + offset;
		if (u.i < limneg)
			u.i = -32768;
		else if (u.i > limpos)
			u.i = 32767;
		y[i << 1] = (int16_t)u.i;
		u.f = jdsp->tmpBuffer[1][i] + offset;
		if (u.i < limneg)
			u.i = -32768;
		else if (u.i > limpos)
			u.i = 32767;
		y[(i << 1) + 1] = (int16_t)u.i;
	}
}
void pint32(JamesDSPLib *jdsp, int32_t *x1, int32_t *x2, int32_t *y1, int32_t *y2, size_t n)
{
	if (jdsp->blockSizeMax < n)
		JamesDSPReallocateBlock(jdsp, n);
	if (jdsp->blockSize != n)
	{
		jdsp->blockSize = n;
		JamesDSPRefreshConvolutions(jdsp, 1);
	}
	static const float scale = (float)(1UL << 31UL);
	for (size_t i = 0; i < n; i++)
	{
		jdsp->tmpBuffer[0][i] = (float)((double)x1[i] / scale);
		jdsp->tmpBuffer[1][i] = (float)((double)x2[i] / scale);
	}
	if (jdsp->enableASRC)
	{
		unsigned int curDecimatedLen = DoASRC_fwd(jdsp, n);
		jdsp->processInternal(jdsp, curDecimatedLen);
		DoASRC_bwd(jdsp, curDecimatedLen, n);
	}
	else
		jdsp->processInternal(jdsp, n);
	float f;
	for (size_t i = 0; i < n; i++)
	{
		if (jdsp->tmpBuffer[0][i] <= -1.0f)
			y1[i] = INT32_MIN;
		else if (jdsp->tmpBuffer[0][i] >= 1.0f)
			y1[i] = INT32_MAX;
		else
		{
			f = jdsp->tmpBuffer[0][i] * scale;
			y1[i] = (int32_t)(f > 0 ? f + 0.5f : f - 0.5f);
		}
		if (jdsp->tmpBuffer[1][i] <= -1.0f)
			y2[i] = INT32_MIN;
		else if (jdsp->tmpBuffer[1][i] >= 1.0f)
			y2[i] = INT32_MAX;
		else
		{
			f = jdsp->tmpBuffer[1][i] * scale;
			y2[i] = (int32_t)(f > 0 ? f + 0.5f : f - 0.5f);
		}
	}
}
void pint32Multiplexed(JamesDSPLib *jdsp, int32_t *x, int32_t *y, size_t n)
{
	if (jdsp->blockSizeMax < n)
		JamesDSPReallocateBlock(jdsp, n);
	if (jdsp->blockSize != n)
	{
		jdsp->blockSize = n;
		JamesDSPRefreshConvolutions(jdsp, 1);
	}
	static const float scale = (float)(1UL << 31UL);
	for (size_t i = 0; i < n; i++)
	{
		jdsp->tmpBuffer[0][i] = (float)((double)x[i << 1] / scale);
		jdsp->tmpBuffer[1][i] = (float)((double)x[(i << 1) + 1] / scale);
	}
	if (jdsp->enableASRC)
	{
		unsigned int curDecimatedLen = DoASRC_fwd(jdsp, n);
		jdsp->processInternal(jdsp, curDecimatedLen);
		DoASRC_bwd(jdsp, curDecimatedLen, n);
	}
	else
		jdsp->processInternal(jdsp, n);
	float f;
	for (size_t i = 0; i < n; i++)
	{
		if (jdsp->tmpBuffer[0][i] <= -1.0f)
			y[i << 1] = INT32_MIN;
		else if (jdsp->tmpBuffer[0][i] >= 1.0f)
			y[i << 1] = INT32_MAX;
		else
		{
			f = jdsp->tmpBuffer[0][i] * scale;
			y[i << 1] = (int32_t)(f > 0 ? f + 0.5f : f - 0.5f);
		}
		if (jdsp->tmpBuffer[1][i] <= -1.0f)
			y[(i << 1) + 1] = INT32_MIN;
		else if (jdsp->tmpBuffer[1][i] >= 1.0f)
			y[(i << 1) + 1] = INT32_MAX;
		else
		{
			f = jdsp->tmpBuffer[1][i] * scale;
			y[(i << 1) + 1] = (int32_t)(f > 0 ? f + 0.5f : f - 0.5f);
		}
	}
}
void pint8_24(JamesDSPLib *jdsp, int32_t *x1, int32_t *x2, int32_t *y1, int32_t *y2, size_t n)
{
	if (jdsp->blockSizeMax < n)
		JamesDSPReallocateBlock(jdsp, n);
	if (jdsp->blockSize != n)
	{
		jdsp->blockSize = n;
		JamesDSPRefreshConvolutions(jdsp, 1);
	}
	static const float scale = (float)(1 << 23);
	float limpos = 0x7fffff / scale;
	float limneg = -0x800000 / scale;
	for (size_t i = 0; i < n; i++)
	{
		jdsp->tmpBuffer[0][i] = (float)((double)x1[i] / scale);
		jdsp->tmpBuffer[1][i] = (float)((double)x2[i] / scale);
	}
	if (jdsp->enableASRC)
	{
		unsigned int curDecimatedLen = DoASRC_fwd(jdsp, n);
		jdsp->processInternal(jdsp, curDecimatedLen);
		DoASRC_bwd(jdsp, curDecimatedLen, n);
	}
	else
		jdsp->processInternal(jdsp, n);
	float f;
	for (size_t i = 0; i < n; i++)
	{
		f = jdsp->tmpBuffer[0][i] * scale;
		y1[i] = (int32_t)(f > 0 ? f + 0.5f : f - 0.5f);
		f = jdsp->tmpBuffer[1][i] * scale;
		y2[i] = (int32_t)(f > 0 ? f + 0.5f : f - 0.5f);
	}
}
void pint8_24Multiplexed(JamesDSPLib *jdsp, int32_t *x, int32_t *y, size_t n)
{
	if (jdsp->blockSizeMax < n)
		JamesDSPReallocateBlock(jdsp, n);
	if (jdsp->blockSize != n)
	{
		jdsp->blockSize = n;
		JamesDSPRefreshConvolutions(jdsp, 1);
	}
	static const float scale = (float)(1 << 23);
	float limpos = 0x7fffff / scale;
	float limneg = -0x800000 / scale;
	for (size_t i = 0; i < n; i++)
	{
		jdsp->tmpBuffer[0][i] = (float)((double)x[i << 1] / scale);
		jdsp->tmpBuffer[1][i] = (float)((double)x[(i << 1) + 1] / scale);
	}
	if (jdsp->enableASRC)
	{
		unsigned int curDecimatedLen = DoASRC_fwd(jdsp, n);
		jdsp->processInternal(jdsp, curDecimatedLen);
		DoASRC_bwd(jdsp, curDecimatedLen, n);
	}
	else
		jdsp->processInternal(jdsp, n);
	float f;
	for (size_t i = 0; i < n; i++)
	{
		f = jdsp->tmpBuffer[0][i] * scale;
		y[i << 1] = (int32_t)(f > 0 ? f + 0.5f : f - 0.5f);
		f = jdsp->tmpBuffer[1][i] * scale;
		y[(i << 1) + 1] = (int32_t)(f > 0 ? f + 0.5f : f - 0.5f);
	}
}
void pintp24(JamesDSPLib *jdsp, uint8_t *x1, uint8_t *x2, uint8_t *y1, uint8_t *y2, size_t n)
{
	if (jdsp->blockSizeMax < n)
		JamesDSPReallocateBlock(jdsp, n);
	if (jdsp->blockSize != n)
	{
		jdsp->blockSize = n;
		JamesDSPRefreshConvolutions(jdsp, 1);
	}
	static const float scale = 1.0f / (float)(1UL << 31);
	for (size_t i = 0; i < n; i++)
	{
		jdsp->tmpBuffer[0][i] = jdsp->i32_from_p24(x1 + i * 3) * scale;
		jdsp->tmpBuffer[1][i] = jdsp->i32_from_p24(x2 + i * 3) * scale;
	}
	if (jdsp->enableASRC)
	{
		unsigned int curDecimatedLen = DoASRC_fwd(jdsp, n);
		jdsp->processInternal(jdsp, curDecimatedLen);
		DoASRC_bwd(jdsp, curDecimatedLen, n);
	}
	else
		jdsp->processInternal(jdsp, n);
	for (size_t i = 0; i < n; i++)
	{
		jdsp->p24_from_i32(clamp24_from_float(jdsp->tmpBuffer[0][i]), y1 + i * 3);
		jdsp->p24_from_i32(clamp24_from_float(jdsp->tmpBuffer[1][i]), y2 + i * 3);
	}
}
void pintp24Multiplexed(JamesDSPLib *jdsp, uint8_t *x, uint8_t *y, size_t n)
{
	if (jdsp->blockSizeMax < n)
		JamesDSPReallocateBlock(jdsp, n);
	if (jdsp->blockSize != n)
	{
		jdsp->blockSize = n;
		JamesDSPRefreshConvolutions(jdsp, 1);
	}
	static const float scale = 1.0f / (float)(1UL << 31);
	for (size_t i = 0; i < n; i++)
	{
		jdsp->tmpBuffer[0][i] = jdsp->i32_from_p24(x + (i << 1) * 3) * scale;
		jdsp->tmpBuffer[1][i] = jdsp->i32_from_p24(x + ((i << 1) + 1) * 3) * scale;
	}
	if (jdsp->enableASRC)
	{
		unsigned int curDecimatedLen = DoASRC_fwd(jdsp, n);
		jdsp->processInternal(jdsp, curDecimatedLen);
		DoASRC_bwd(jdsp, curDecimatedLen, n);
	}
	else
		jdsp->processInternal(jdsp, n);
	for (size_t i = 0; i < n; i++)
	{
		jdsp->p24_from_i32(clamp24_from_float(jdsp->tmpBuffer[0][i]), y + (i << 1) * 3);
		jdsp->p24_from_i32(clamp24_from_float(jdsp->tmpBuffer[1][i]), y + ((i << 1) + 1) * 3);
	}
}
void pfloat32(JamesDSPLib *jdsp, float *x1, float *x2, float *y1, float *y2, size_t n)
{
	if (jdsp->blockSizeMax < n)
		JamesDSPReallocateBlock(jdsp, n);
	if (jdsp->blockSize != n)
	{
		jdsp->blockSize = n;
		JamesDSPRefreshConvolutions(jdsp, 1);
	}
	for (size_t i = 0; i < n; i++)
	{
		jdsp->tmpBuffer[0][i] = x1[i];
		jdsp->tmpBuffer[1][i] = x2[i];
	}
	if (jdsp->enableASRC)
	{
		unsigned int curDecimatedLen = DoASRC_fwd(jdsp, n);
		jdsp->processInternal(jdsp, curDecimatedLen);
		DoASRC_bwd(jdsp, curDecimatedLen, n);
	}
	else
		jdsp->processInternal(jdsp, n);
	for (size_t i = 0; i < n; i++)
	{
		y1[i] = jdsp->tmpBuffer[0][i];
		y2[i] = jdsp->tmpBuffer[1][i];
	}
}
void pfloat32Multiplexed(JamesDSPLib *jdsp, float *x, float *y, size_t n)
{
	if (jdsp->blockSizeMax < n)
		JamesDSPReallocateBlock(jdsp, n);
	if (jdsp->blockSize != n)
	{
		jdsp->blockSize = n;
		JamesDSPRefreshConvolutions(jdsp, 1);
	}
	for (size_t i = 0; i < n; i++)
	{
		jdsp->tmpBuffer[0][i] = x[i << 1];
		jdsp->tmpBuffer[1][i] = x[(i << 1) + 1];
	}
	if (jdsp->enableASRC)
	{
		unsigned int curDecimatedLen = DoASRC_fwd(jdsp, n);
		jdsp->processInternal(jdsp, curDecimatedLen);
		DoASRC_bwd(jdsp, curDecimatedLen, n);
	}
	else
		jdsp->processInternal(jdsp, n);
	for (size_t i = 0; i < n; i++)
	{
		y[i << 1] = jdsp->tmpBuffer[0][i];
		y[(i << 1) + 1] = jdsp->tmpBuffer[1][i];
	}
}
extern void JamesDSPOfflineResampling(float const *in, float *out, size_t lenIn, size_t lenOut, int channels, double src_ratio);
// Binary blobs
extern const int hrtfLenPerChannel;
extern const int hrtfFs;
extern const int compressedLen_jdspImp;
extern const unsigned char jdspImp[7172];
extern const int compressedLen_CCConv;
extern const double ccconv1Gain;
extern const double ccconv2Gain;
extern const double ccconv3Gain;
extern const double ccconv4Gain;
extern const unsigned char CCConv[202119];
void JamesDSPRefreshBlob(JamesDSPLib *jdsp, double targetFs)
{
	const int channelsBlobsShort = 4;
	double ratio = targetFs / (double)hrtfFs;
	int outLen = (int)ceil(hrtfLenPerChannel * ratio);
	int i;
	if (jdsp->blobsCh1[0])
	{
		for (i = 0; i < 3; i++)
		{
			free(jdsp->blobsCh1[i]);
			free(jdsp->blobsCh2[i]);
			free(jdsp->blobsCh3[i]);
			free(jdsp->blobsCh4[i]);
		}
	}
	for (i = 0; i < 3; i++)
	{
		jdsp->blobsCh1[i] = (float *)malloc(outLen * sizeof(float));
		jdsp->blobsCh2[i] = (float *)malloc(outLen * sizeof(float));
		jdsp->blobsCh3[i] = (float *)malloc(outLen * sizeof(float));
		jdsp->blobsCh4[i] = (float *)malloc(outLen * sizeof(float));
	}
	jdsp->blobsResampledLen = outLen;
	float *tmpBuf = (float *)malloc(outLen * channelsBlobsShort * sizeof(float));
	memset(tmpBuf, 0, outLen * channelsBlobsShort * sizeof(float));

	drflac *pFlac = drflac_open_memory((void *)jdspImp, compressedLen_jdspImp, 0);
	size_t totalSmps = pFlac->totalPCMFrameCount * (size_t)pFlac->channels;
	float *pFrameImpulse = (float *)malloc(totalSmps * sizeof(float));
	size_t sampleCount = pFlac->totalPCMFrameCount;
	drflac_read_pcm_frames_f32(pFlac, totalSmps, pFrameImpulse);
	size_t copySize = (hrtfLenPerChannel << 2) * sizeof(float);
	float *CorredHRTF_Surround1 = (float *)malloc(copySize);
	memcpy(CorredHRTF_Surround1, pFrameImpulse, copySize);
	float *CorredHRTF_Surround2 = (float *)malloc(copySize);
	memcpy(CorredHRTF_Surround2, pFrameImpulse + (hrtfLenPerChannel << 2), copySize);
	float *CorredHRTFCrossfeed = (float *)malloc(copySize);
	memcpy(CorredHRTFCrossfeed, pFrameImpulse + ((hrtfLenPerChannel << 2) << 1), copySize);
	drflac_close(pFlac);
	free(pFrameImpulse);

	float *ptr1[4] = { jdsp->blobsCh1[0], jdsp->blobsCh2[0], jdsp->blobsCh3[0], jdsp->blobsCh4[0] };
	JamesDSPOfflineResampling(CorredHRTFCrossfeed, tmpBuf, hrtfLenPerChannel, outLen, channelsBlobsShort, ratio);
	free(CorredHRTFCrossfeed);
	channel_splitFloat(tmpBuf, outLen, ptr1, channelsBlobsShort);
	float *ptr2[4] = { jdsp->blobsCh1[1], jdsp->blobsCh2[1], jdsp->blobsCh3[1], jdsp->blobsCh4[1] };
	memset(tmpBuf, 0, outLen * channelsBlobsShort * sizeof(float));
	JamesDSPOfflineResampling(CorredHRTF_Surround1, tmpBuf, hrtfLenPerChannel, outLen, channelsBlobsShort, ratio);
	free(CorredHRTF_Surround1);
	channel_splitFloat(tmpBuf, outLen, ptr2, channelsBlobsShort);
	float *ptr3[4] = { jdsp->blobsCh1[2], jdsp->blobsCh2[2], jdsp->blobsCh3[2], jdsp->blobsCh4[2] };
	memset(tmpBuf, 0, outLen * channelsBlobsShort * sizeof(float));
	JamesDSPOfflineResampling(CorredHRTF_Surround2, tmpBuf, hrtfLenPerChannel, outLen, channelsBlobsShort, ratio);
	free(CorredHRTF_Surround2);
	channel_splitFloat(tmpBuf, outLen, ptr3, channelsBlobsShort);
	free(tmpBuf);

	pFlac = drflac_open_memory((void *)CCConv, compressedLen_CCConv, 0);
	ratio = targetFs / (double)(pFlac->sampleRate);
	totalSmps = pFlac->totalPCMFrameCount * (size_t)pFlac->channels;
	pFrameImpulse = (float *)malloc(totalSmps * sizeof(float));
	sampleCount = pFlac->totalPCMFrameCount;
	drflac_read_pcm_frames_f32(pFlac, totalSmps, pFrameImpulse);
	drflac_close(pFlac);
	outLen = (int)ceil(sampleCount * ratio);
	if (jdsp->hrtfblobsResampled[0])
		for (i = 0; i < 4; i++)
			free(jdsp->hrtfblobsResampled[i]);
	for (i = 0; i < 4; i++)
		jdsp->hrtfblobsResampled[i] = (float *)malloc(outLen * sizeof(float));
	jdsp->frameLenSVirResampled = outLen;
	tmpBuf = (float *)malloc(outLen * channelsBlobsShort * sizeof(float));
	memset(tmpBuf, 0, outLen * channelsBlobsShort * sizeof(float));
	JamesDSPOfflineResampling(pFrameImpulse, tmpBuf, sampleCount, outLen, channelsBlobsShort, ratio);
	free(pFrameImpulse);
	channel_splitFloat(tmpBuf, outLen, jdsp->hrtfblobsResampled, channelsBlobsShort);
	free(tmpBuf);
	for (i = 0; i < outLen; i++)
	{
		jdsp->hrtfblobsResampled[0][i] = (float)((double)jdsp->hrtfblobsResampled[0][i] * ccconv1Gain);
		jdsp->hrtfblobsResampled[1][i] = (float)((double)jdsp->hrtfblobsResampled[1][i] * ccconv2Gain);
		jdsp->hrtfblobsResampled[2][i] = (float)((double)jdsp->hrtfblobsResampled[2][i] * ccconv3Gain);
		jdsp->hrtfblobsResampled[3][i] = (float)((double)jdsp->hrtfblobsResampled[3][i] * ccconv4Gain);
	}
}
// Init JamesDSP
void JamesDSPInit(JamesDSPLib *jdsp, int n, float sample_rate)
{
	memset(jdsp, 0, sizeof(JamesDSPLib));
	// Endianness detection
	unsigned int x = 1;
	if ((((char *)&x)[0]) == 1)
	{
		jdsp->i32_from_p24 = i32_from_p24_little_endian;
		jdsp->p24_from_i32 = p24_from_i32_little_endian;
	}
	else
	{
		jdsp->i32_from_p24 = i32_from_p24_big_endian;
		jdsp->p24_from_i32 = p24_from_i32_big_endian;
	}
	//
	if (pthread_mutex_init(&jdsp->m_in_processing, NULL) != 0)
		jdsp->isMutexSuccess = 0;
	else
		jdsp->isMutexSuccess = 1;
	// Init buffer
	jdsp->blockSize = n;
	jdsp->blockSizeMax = n;
	// Function pointer
	jdsp->processInt16Deinterleaved = pint16;
	jdsp->processInt32Deinterleaved = pint32;
	jdsp->processFloatDeinterleaved = pfloat32;
	jdsp->processInt16Multiplexd = pint16Multiplexed;
	jdsp->processInt32Multiplexd = pint32Multiplexed;
	jdsp->processFloatMultiplexd = pfloat32Multiplexed;
	jdsp->processInt8_24Multiplexd = pint8_24Multiplexed;
	jdsp->processInt24PackedMultiplexd = pintp24Multiplexed;
	jdsp->processInt8_24Deinterleaved = pint8_24;
	jdsp->processInt24PackedDeinterleaved = pintp24;
	jdsp->processInternal = JamesDSPProcessCheckBenchmarkReady;
	//
	const unsigned int asrc_taps = 64;
	char isminphase = 1;
	if (sample_rate < 44100.0f || sample_rate > 48000.0f)
	{
		jdsp->enableASRC = 1;
		int roundedRate = (int)(sample_rate);
		jdsp->trueSampleRate = sample_rate;
		if (((roundedRate % 48000 == 0) || (48000 % roundedRate == 0)) && roundedRate != 48000)
			jdsp->fs = 48000;
		else if (((roundedRate % 44100 == 0) || (44100 % roundedRate == 0)) && roundedRate != 44100)
			jdsp->fs = 44100;
		else
			jdsp->fs = 48000;
		InitIntegerASRCHandler(&jdsp->asrc[0], (unsigned long long)jdsp->fs, (unsigned long long)jdsp->trueSampleRate, asrc_taps, isminphase, 0, 0);
		InitIntegerASRCHandler(&jdsp->asrc[1], (unsigned long long)jdsp->fs, (unsigned long long)jdsp->trueSampleRate, asrc_taps, isminphase, &jdsp->asrc[0].polyphaseDecimator, &jdsp->asrc[0].polyphaseInterpolator);
		double ratio = (double)jdsp->fs / (double)jdsp->trueSampleRate;
		unsigned int maxDecimatedLength = (unsigned int)ceil(n * ratio);
		unsigned int maxInterpolatedLength = (unsigned int)ceil(maxDecimatedLength / ratio);
		jdsp->pw2BlockMemSize = next_pow_2(maxInterpolatedLength);
		size_t ctMemBlk = jdsp->blockSizeMax * 2 + maxInterpolatedLength * 2 + jdsp->pw2BlockMemSize * 2;
		jdsp->tmpBuffer[0] = (float *)malloc(ctMemBlk * sizeof(float));
		jdsp->tmpBuffer[1] = jdsp->tmpBuffer[0] + jdsp->blockSizeMax;
		jdsp->tmpBuffer[2] = jdsp->tmpBuffer[1] + jdsp->blockSizeMax;
		jdsp->tmpBuffer[3] = jdsp->tmpBuffer[2] + maxInterpolatedLength;
		jdsp->tmpBuffer[4] = jdsp->tmpBuffer[3] + maxInterpolatedLength;
		jdsp->tmpBuffer[5] = jdsp->tmpBuffer[4] + jdsp->pw2BlockMemSize;
	}
	else
	{
		size_t ctMemBlk = jdsp->blockSizeMax * 2;
		jdsp->tmpBuffer[0] = (float *)malloc(ctMemBlk * sizeof(float));
		jdsp->tmpBuffer[1] = jdsp->tmpBuffer[0] + jdsp->blockSizeMax;
		jdsp->trueSampleRate = sample_rate;
		jdsp->fs = sample_rate;
		jdsp->enableASRC = 0;
	}
	// Init IO control
	JLimiterInit(jdsp);
	jdsp->bassExEnabled = 0;
	BassExciterSetParam(jdsp, 100.0f, 40.0f, 50.0f);
	jdsp->spectrumExtEnabled = 0;
	SpectrumExtensionSetParam(jdsp, 7600.0f, 45.0f);
	BassExciterSetParam2(jdsp, 0, 60.0f, 40.0f, 40.0f);
	jdsp->vdynBassEnabled = 0;
	VDynBassSetParam(jdsp, 33.0f, 1000.0f, 6200.0f, 50.0f, 90.0f, 30.0f, 10.0f);
	jdsp->diffSurroundEnabled = 0;
	DiffSurroundSetParam(jdsp, 0.0f, 10.0f);
	jdsp->viperClarityEnabled = 0;
	ViperClaritySetParam(jdsp, 0, 6.0f);
	jdsp->fieldSurroundEnabled = 0;
	FieldSurroundSetParam(jdsp, 30.0f, 50.0f);
	jdsp->agcEnabled = 0;
	AgcSetParam(jdsp, 30.0f, 12.0f);
	jdsp->hpSurroundEnabled = 0;
	HpSurroundSetParam(jdsp, 60.0f, 30.0f);
	jdsp->fetCompEnabled = 0;
	FetCompSetParam(jdsp, -18.0f, 4.0f, 5.0f, 120.0f, 0.0f);
	jdsp->cureEnabled = 0;
	CureSetParam(jdsp, 0);
	jdsp->viperBassEnabled = 0;
	ViperBassSetParam(jdsp, 0, 76.0f, 6.0f);
	jdsp->vreverbEnabled = 0;
	VReverbSetParam(jdsp, 0, 50.0f, 50.0f, 100.0f, 20.0f, 45.0f, 70.0f,
		30.0f, 50.0f, 50.0f, 30.0f, 100.0f);
	jdsp->speakerOptEnabled = 0;
	SpeakerOptSetParam(jdsp, 60.0f);
	jdsp->pitchShiftEnabled = 0;
	PitchShiftSetParam(jdsp, 0.0f, 100.0f);
	jdsp->echoDelayEnabled = 0;
	EchoDelaySetParam(jdsp, 100.0f, 350.0f, 20.0f, 0.0f, 0,
		1, 50.0f,
		40.0f, 12000.0f, 10.0f, 0,
		100.0f, 24.0f,
		0.0f, 0.0f, 0.0f,
		0.0f, 0.0f,
		1, 0.0f, 50.0f, 0.0f,
		0.0f, 35.0f, 100.0f);
	jdsp->diffSurround.bufL = 0;
	jdsp->diffSurround.bufR = 0;
	jdsp->maximizerEnabled = 0;
	jdsp->maximizer.buf[0] = 0;
	jdsp->maximizer.buf[1] = 0;
	jdsp->maximizer.dq[0] = 0;
	jdsp->maximizer.dq[1] = 0;
	jdsp->maximizer.dqVal[0] = 0;
	jdsp->maximizer.dqVal[1] = 0;
	MaximizerSetParam(jdsp, MAXR_MODE_TRANSPARENT, 6.0f, -0.3f, 200.0f,
		0.0f, 0.0f, 1, 100.0f, 1, MAXR_CLIP_SMOOTH);
	jdsp->multibandDistEnabled = 0;
	jdsp->multibandDist.chBufL = 0;
	jdsp->multibandDist.chBufR = 0;
	jdsp->multibandDist.numBands = 0;
	MultibandDistSetParam(jdsp, MBD_ROUTING_SPLIT, MBD_MODEL_SOFT,
		35.0f, 0.0f, 50.0f,
		16.0f, 0.0f,
		50.0f, 100.0f,
		0.6f, 6.0f, 0.0f,
		50.0f, 2, 0.0f,
		100.0f);
	jdsp->dynamicEqEnabled = 0;
	jdsp->dynamicEq.numBands = 0;
	{
		// freq, Q, threshold dB, ratio, attack ms, release ms, range dB, mode
		const float defaults[3 * DYNEQ_VALUES_PER_BAND] = {
			180.0f, 1.0f, -22.0f, 3.0f, 15.0f, 150.0f, -6.0f, (float)DYNEQ_MODE_COMPRESS,
			3200.0f, 1.4f, -26.0f, 3.0f,  3.0f,  80.0f, -5.0f, (float)DYNEQ_MODE_COMPRESS,
			6800.0f, 3.0f, -30.0f, 4.0f,  1.0f,  40.0f, -8.0f, (float)DYNEQ_MODE_COMPRESS
		};
		DynamicEqSetBands(jdsp, defaults, 3);
	}
	DynamicEqSetParam(jdsp, 100.0f, MS_MODE_STEREO);
	jdsp->imagingEnabled = 0;
	// Bass folded to mono, mids left alone, top opened up: audible on any
	// stereo material without being a trick.
	ImagingSetParam(jdsp, 120.0f, 250.0f, 1500.0f, 6000.0f,
		1.0f, 1.15f, 1.6f, 100.0f);
	jdsp->transientEnabled = 0;
	// Punch added low and low-mid, a little of the room taken off the tail,
	// top left alone so cymbals are not sharpened.
	jdsp->tapeEnabled = 0;
	// A worn but working machine: audible wobble, gentle saturation, the head
	// bump that makes tape masters sound weightier than the mix did.
	TapeSetParam(jdsp, 25.0f, 30.0f, 35.0f, -20.0f, 3.0f, 100.0f);
	jdsp->exciterEnabled = 0;
	// Weight underneath, a touch through the mids, air on top - and a valve
	// character so the added harmonics are even rather than edgy.
	ExciterSetParam(jdsp, 150.0f, 900.0f, 4500.0f,
		30.0f, 12.0f, 18.0f, 35.0f,
		EXCITER_TUBE, 6.0f, 100.0f);
	jdsp->lowEndEnabled = 0;
	// Rumble gone, a little body added, the thick region eased back.
	LowEndSetParam(jdsp, 30.0f, 90.0f, 3.0f, 300.0f, -2.5f, 100.0f);
	TransientSetParam(jdsp, 200.0f, 3000.0f,
		45.0f, -20.0f,
		30.0f, -15.0f,
		0.0f, 0.0f,
		9.0f, 100.0f);
	for (int i = 0; i < JDSP_LIVEPROG_EXTRA; i++)
		jdsp->liveprogExtraEnabled[i] = 0;
	JamesDSPResetChainOrder(jdsp);
	JLimiterSetCoefficients(jdsp, -0.1, 60.0);
	jdsp->postGain = 1.0f;
	// Init effect
	LiveProgConstructor(jdsp);
	CompressorConstructor(jdsp);
	CompressorDisable(jdsp);
	BassBoostConstructor(jdsp);
	BassBoostSetParam(jdsp, 5.0f);
	BassBoostDisable(jdsp);
	Reverb_SetParam(jdsp, SF_REVERB_PRESET_PLATEHIGH);
	ReverbDisable(jdsp);
	StereoEnhancementConstructor(jdsp);
	StereoEnhancementSetParam(jdsp, 0.6f);
	StereoEnhancementDisable(jdsp);
	VacuumTubeSetGain(jdsp, 2.0);
	VacuumTubeDisable(jdsp);
	LiveProgDisable(jdsp);
	DDCConstructor(jdsp);
	DDCDisable(jdsp);
	CrossfeedConstructor(jdsp);
	CrossfeedChangeMode(jdsp, 5);
	CrossfeedDisable(jdsp);
	Convolver1DConstructor(jdsp);
	ArbitraryResponseEqualizerConstructor(jdsp);
	ArbitraryResponseEqualizerDisable(jdsp);
	MultimodalEqualizerConstructor(jdsp);
	MultimodalEqualizerDisable(jdsp);
	// Init binary blobs, random number generator
	jdsp->rndstate[0] = time(0);
	for (int i = 0; i < 3; i++)
	{
		jdsp->blobsCh1[i] = 0;
		jdsp->blobsCh2[i] = 0;
		jdsp->blobsCh3[i] = 0;
		jdsp->blobsCh4[i] = 0;
		for (int j = 0; j < ((n > 1) ? (n / (i + 1)) : (128)); j++)
			randXorshift(jdsp->rndstate);
	}
	JamesDSPRefreshBlob(jdsp, (double)jdsp->fs);
	jdsp->rndstate[1] = (uint64_t)(randXorshift(jdsp->rndstate) * 2.0);
#ifdef DEBUG
	__android_log_print(ANDROID_LOG_INFO, TAG, "Printing benchmark data start");
	for (int s = 0; s < MAX_BENCHMARK; s++)
		__android_log_print(ANDROID_LOG_INFO, TAG, "%1.7lf,%1.7lf", convbench_c0[s], convbench_c1[s]);
	__android_log_print(ANDROID_LOG_INFO, TAG, "Printing benchmark data end");
#endif
}
void JamesDSPSetPostGain(JamesDSPLib *jdsp, double pGaindB)
{
	if (pGaindB < -15.0f)
		pGaindB = -15.0f;
	if (pGaindB > 15.0f)
		pGaindB = 15.0f;
	jdsp->postGain = db2magf(pGaindB);
}
int JamesDSPGetMutexStatus(JamesDSPLib *jdsp)
{
	return jdsp->isMutexSuccess;
}
void JamesDSPSetSampleRate(JamesDSPLib *jdsp, float new_sample_rate, int forceRefresh)
{
	if (jdsp->trueSampleRate == new_sample_rate)
		return;
	jdsp->trueSampleRate = new_sample_rate;
	jdsp_lock(jdsp);
	if (jdsp->enableASRC)
	{
		FreeIntegerASRCHandler(&jdsp->asrc[0]);
		FreeIntegerASRCHandler(&jdsp->asrc[1]);
	}
	const unsigned int asrc_taps = 32;
	char isminphase = 1;
	if (new_sample_rate < 44100.0f || new_sample_rate > 48000.0f)
	{
		jdsp->enableASRC = 1;
		int roundedRate = (int)(new_sample_rate);
		if (((roundedRate % 48000 == 0) || (48000 % roundedRate == 0)) && roundedRate != 48000)
			jdsp->fs = 48000;
		else if (((roundedRate % 44100 == 0) || (44100 % roundedRate == 0)) && roundedRate != 44100)
			jdsp->fs = 44100;
		else
			jdsp->fs = 48000;
		InitIntegerASRCHandler(&jdsp->asrc[0], (unsigned long long)jdsp->fs, (unsigned long long)jdsp->trueSampleRate, asrc_taps, isminphase, 0, 0);
		InitIntegerASRCHandler(&jdsp->asrc[1], (unsigned long long)jdsp->fs, (unsigned long long)jdsp->trueSampleRate, asrc_taps, isminphase, &jdsp->asrc[0].polyphaseDecimator, &jdsp->asrc[0].polyphaseInterpolator);
		double ratio = (double)jdsp->fs / (double)jdsp->trueSampleRate;
		unsigned int maxDecimatedLength = (unsigned int)ceil(jdsp->blockSizeMax * ratio);
		unsigned int maxInterpolatedLength = (unsigned int)ceil(maxDecimatedLength / ratio);
		jdsp->pw2BlockMemSize = next_pow_2(maxInterpolatedLength);
		if (jdsp->tmpBuffer[0])
			free(jdsp->tmpBuffer[0]);
		size_t ctMemBlk = jdsp->blockSizeMax * 2 + maxInterpolatedLength * 2 + jdsp->pw2BlockMemSize * 2;
		jdsp->tmpBuffer[0] = (float *)malloc(ctMemBlk * sizeof(float));
		jdsp->tmpBuffer[1] = jdsp->tmpBuffer[0] + jdsp->blockSizeMax;
		jdsp->tmpBuffer[2] = jdsp->tmpBuffer[1] + jdsp->blockSizeMax;
		jdsp->tmpBuffer[3] = jdsp->tmpBuffer[2] + maxInterpolatedLength;
		jdsp->tmpBuffer[4] = jdsp->tmpBuffer[3] + maxInterpolatedLength;
		jdsp->tmpBuffer[5] = jdsp->tmpBuffer[4] + jdsp->pw2BlockMemSize;
	}
	else
	{
		jdsp->enableASRC = 0;
		jdsp->fs = jdsp->trueSampleRate;
	}
	JamesDSPRefreshBlob(jdsp, jdsp->fs);
	if (forceRefresh)
	{
		BassBoostSetParam(jdsp, jdsp->dbb.maxGain);
		jdsp->ddcForceRefresh = 1;
		DDCEnable(jdsp, jdsp->ddcEnabled);
		jdsp->crossfeedForceRefresh = 1;
		CrossfeedEnable(jdsp, jdsp->crossfeedEnabled);
		jdsp->arbMagForceRefresh = 1;
		ArbitraryResponseEqualizerEnable(jdsp, jdsp->arbitraryMagEnabled);
		jdsp->equalizerForceRefresh = 1;
		MultimodalEqualizerEnable(jdsp, jdsp->equalizerEnabled);
		jdsp->compForceRefresh = 1;
		CompressorEnable(jdsp, jdsp->compEnabled);
		StereoEnhancementRefresh(jdsp);
	}
	/* Outside the forceRefresh guard on purpose. Nothing in this app ever
	   passes forceRefresh, so anything inside that branch is never redesigned -
	   and these six cache every coefficient they use, so left alone they would
	   go on filtering at the rate they were last set up for. Moving between
	   44.1k and 48k puts every corner frequency out by about nine percent, and
	   for the tape it also mistimes the wow and flutter oscillators.

	   It matters more at startup than mid-session: preferences are pushed in
	   before the first stream arrives, so without this the settings are
	   designed against the 48k fallback and then simply stay there for the
	   whole of a 44.1k stream.

	   These take no lock of their own - it is held here, and it is not
	   recursive. */
	TapeRefresh(jdsp);
	ExciterRefresh(jdsp);
	LowEndRefresh(jdsp);
	TransientRefresh(jdsp);
	ImagingRefresh(jdsp);
	DynamicEqRefresh(jdsp);
	jdsp_unlock(jdsp);
}
void JamesDSPReleaseEffectBuffers(JamesDSPLib *jdsp)
{
	EchoDelayDisable(jdsp);
	MultibandDistDisable(jdsp);
	MaximizerDisable(jdsp);
	DiffSurroundDisable(jdsp);
	VReverbDisable(jdsp);
	PitchShiftDisable(jdsp);
	HpSurroundDisable(jdsp);
}

void JamesDSPFree(JamesDSPLib *jdsp)
{
	JamesDSPReleaseEffectBuffers(jdsp);
	jdsp_lock(jdsp);
	StereoEnhancementDestructor(jdsp);
	CompressorDestructor(jdsp);
	LiveProgDestructor(jdsp);
	DDCDestructor(jdsp);
	CrossfeedDestructor(jdsp);
	Convolver1DDestructor(jdsp);
	ArbitraryResponseEqualizerDestructor(jdsp);
	MultimodalEqualizerDestructor(jdsp);
	if (jdsp->tmpBuffer[0])
		free(jdsp->tmpBuffer[0]);
	int i;
	if (jdsp->blobsCh1[0])
	{
		for (i = 0; i < 3; i++)
		{
			free(jdsp->blobsCh1[i]);
			free(jdsp->blobsCh2[i]);
			free(jdsp->blobsCh3[i]);
			free(jdsp->blobsCh4[i]);
		}
	}
	if (jdsp->hrtfblobsResampled[0])
	{
		for (i = 0; i < 4; i++)
			free(jdsp->hrtfblobsResampled[i]);
	}
	if (jdsp->impulseResponseStorage.impulseResponse)
		free(jdsp->impulseResponseStorage.impulseResponse);
	if (jdsp->isMutexSuccess)
		pthread_mutex_destroy(&jdsp->m_in_processing);
	if (jdsp->enableASRC)
	{
		FreeIntegerASRCHandler(&jdsp->asrc[0]);
		FreeIntegerASRCHandler(&jdsp->asrc[1]);
	}
	jdsp_unlock(jdsp);
}