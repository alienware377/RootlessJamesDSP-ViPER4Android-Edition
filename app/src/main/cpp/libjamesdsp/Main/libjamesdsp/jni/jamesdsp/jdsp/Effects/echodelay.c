// Stereo echo/delay modelled on the classic "fruity delay 3" layout:
// input level, delay time with smoothing / offset / pitch-preserving retune,
// mono / stereo / ping-pong routing, a filtered feedback path with resonance,
// sample-rate and bit-depth reduction, diffusion, feedback distortion
// (limit or saturate, with knee and symmetry), plus output tone and wet/dry.
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../jdsp_header.h"

#define ECHO_MASK (ECHO_BUFLEN - 1)
#define ECHO_APMASK (ECHO_APLEN - 1)

static inline float echoRead(const float *buf, float pos)
{
	int i0 = (int)pos;
	float fr = pos - (float)i0;
	int i1 = (i0 + 1) & ECHO_MASK;
	return buf[i0 & ECHO_MASK] * (1.0f - fr) + buf[i1] * fr;
}

void EchoDelayUpdateFilter(EchoDelay *e, float cutoffHz)
{
	if (cutoffHz < 20.0f) cutoffHz = 20.0f;
	if (cutoffHz > e->fs * 0.49f) cutoffHz = e->fs * 0.49f;
	float g = tanf(3.14159265358979f * cutoffHz / e->fs);
	float denom = 1.0f + g * (g + e->svfK);
	if (denom < 1e-9f) denom = 1e-9f;
	e->svfA1 = 1.0f / denom;
	e->svfA2 = g * e->svfA1;
	e->svfA3 = g * e->svfA2;
}

void EchoDelaySetParam(JamesDSPLib *jdsp, float inputLevel, float timeMs,
	float smoothingPct, float offsetMs, int keepPitch,
	int model, float stereoPct,
	float feedbackPct, float cutoffHz, float resonance, int filterType,
	float smpRatePct, float bits,
	float modRateHz, float modTimePct, float modCutoffPct,
	float diffusionPct, float spreadPct,
	int distMode, float distLevel, float knee, float symmetry,
	float tonePct, float wetPct, float dryPct)
{
	EchoDelay *e = &jdsp->echoDelay;
	float fs = (float)jdsp->fs;
	if (fs < 8000.0f) fs = 48000.0f;
	e->fs = fs;

	e->inputLevel = inputLevel * 0.01f;

	if (timeMs < 1.0f) timeMs = 1.0f;
	float maxMs = (float)(ECHO_BUFLEN - 8) * 1000.0f / fs;
	if (timeMs > maxMs) timeMs = maxMs;
	e->targetDelay = timeMs * 0.001f * fs;
	if (e->delaySamples <= 0.0f)
		e->delaySamples = e->targetDelay;

	// Smoothing: how quickly the read head chases a new delay time.
	float sm = smoothingPct * 0.01f;
	if (sm < 0.0f) sm = 0.0f;
	if (sm > 1.0f) sm = 1.0f;
	e->smoothCoeff = expf(-1.0f / (fs * (0.0005f + sm * 0.5f)));
	e->keepPitch = keepPitch;
	e->offsetSamples = offsetMs * 0.001f * fs;

	e->model = model;
	e->stereoSpread = stereoPct * 0.01f;

	e->feedback = feedbackPct * 0.0098f;
	if (e->feedback > 0.995f) e->feedback = 0.995f;
	if (e->feedback < 0.0f) e->feedback = 0.0f;

	// Topology-preserving SVF: stable to Nyquist, unlike the Chamberlin form
	if (cutoffHz < 20.0f) cutoffHz = 20.0f;
	if (cutoffHz > fs * 0.49f) cutoffHz = fs * 0.49f;
	e->cutoffHz = cutoffHz;
	float q = 0.5f + resonance * 0.045f;
	if (q < 0.5f) q = 0.5f;
	e->svfK = 1.0f / q;
	e->filterType = filterType;
	EchoDelayUpdateFilter(e, cutoffHz);

	float sr = smpRatePct * 0.01f;
	if (sr < 0.002f) sr = 0.002f;
	if (sr > 1.0f) sr = 1.0f;
	e->srStep = sr;
	if (bits < 1.0f) bits = 1.0f;
	if (bits > 24.0f) bits = 24.0f;
	e->bitLevels = powf(2.0f, bits) * 0.5f;

	e->modRate = modRateHz;
	e->modInc = 2.0f * 3.14159265358979f * modRateHz / fs;
	e->modTime = modTimePct * 0.01f * 0.004f * fs;
	e->modCutoff = modCutoffPct * 0.01f;

	e->diffusion = diffusionPct * 0.01f * 0.75f;
	e->spread = spreadPct * 0.01f;

	e->distMode = distMode;
	e->distLevel = distLevel * 0.01f;
	e->knee = 0.05f + knee * 0.0095f;
	e->symmetry = symmetry * 0.01f;

	float t = tonePct * 0.01f;
	if (t < -1.0f) t = -1.0f;
	if (t > 1.0f) t = 1.0f;
	e->tone = t;
	float toneHz = 700.0f * powf(10.0f, t * 1.1f);
	if (toneHz < 60.0f) toneHz = 60.0f;
	if (toneHz > fs * 0.45f) toneHz = fs * 0.45f;
	e->toneCoeff = expf(-2.0f * 3.14159265358979f * toneHz / fs);

	e->wet = wetPct * 0.01f;
	e->dry = dryPct * 0.01f;
}

static inline float echoSvf(EchoDelay *e, int ch, float x)
{
	if (e->filterType == 3)
		return x;
	float v3 = x - e->svfIc2[ch];
	float v1 = e->svfA1 * e->svfIc1[ch] + e->svfA2 * v3;
	float v2 = e->svfIc2[ch] + e->svfA2 * e->svfIc1[ch] + e->svfA3 * v3;
	e->svfIc1[ch] = 2.0f * v1 - e->svfIc1[ch];
	e->svfIc2[ch] = 2.0f * v2 - e->svfIc2[ch];
	if (e->filterType == 0) return v2;
	if (e->filterType == 1) return x - e->svfK * v1 - v2;
	return v1;
}

/** Limit or saturate, with adjustable knee softness and up/down asymmetry. */
static inline float echoDistort(EchoDelay *e, float x)
{
	if (e->distLevel <= 0.0001f)
		return x;
	float drive = 1.0f + e->distLevel * 8.0f;
	float bias = e->symmetry * 0.5f;
	float y = (x + bias) * drive;
	float out;
	if (e->distMode == 0)
	{
		float k = e->knee;
		float a = fabsf(y);
		if (a <= 1.0f - k)
			out = y;
		else
		{
			float over = a - (1.0f - k);
			float shaped = (1.0f - k) + k * (1.0f - expf(-over / (k + 1e-6f)));
			out = (y < 0.0f) ? -shaped : shaped;
		}
	}
	else
	{
		float k = 0.5f + e->knee * 2.0f;
		out = tanhf(y * k) / tanhf(k);
	}
	return (out / drive) - bias;
}

static inline float echoCrush(EchoDelay *e, float x)
{
	if (e->bitLevels >= 8388608.0f)
		return x;
	return roundf(x * e->bitLevels) / e->bitLevels;
}

void EchoDelayProcess(JamesDSPLib *jdsp, size_t n)
{
	EchoDelay *e = &jdsp->echoDelay;
	size_t i;
	if (!jdsp->tmpBuffer[0] || !jdsp->tmpBuffer[1])
		return;
	if (!e->bufL || !e->bufR)
		return;
	if (e->model == 3)
		return;

	for (i = 0; i < n; i++)
	{
		int w = e->widx;
		float inL = jdsp->tmpBuffer[0][i];
		float inR = jdsp->tmpBuffer[1][i];

		float prev = e->delaySamples;
		e->delaySamples = e->targetDelay +
			(e->delaySamples - e->targetDelay) * e->smoothCoeff;
		if (e->keepPitch && fabsf(e->delaySamples - e->targetDelay) > 1.0f)
		{
			// Jump straight there and crossfade, so retuning the delay does
			// not sweep the pitch of whatever is already in the line
			e->delaySamples = e->targetDelay;
			e->xfade = 1.0f;
			e->xfadePos = prev;
		}

		float lfo = sinf(e->modPhase);
		float modT = lfo * e->modTime;
		if (e->modCutoff > 0.0001f)
			EchoDelayUpdateFilter(e, e->cutoffHz * powf(2.0f, lfo * e->modCutoff * 2.0f));

		float dL = e->delaySamples + modT;
		float dR = e->delaySamples * (1.0f + 0.35f * e->stereoSpread)
			+ e->offsetSamples - modT;
		if (dL < 2.0f) dL = 2.0f;
		if (dR < 2.0f) dR = 2.0f;

		float posL = (float)w - dL; if (posL < 0.0f) posL += ECHO_BUFLEN;
		float posR = (float)w - dR; if (posR < 0.0f) posR += ECHO_BUFLEN;

		float echoL = echoRead(e->bufL, posL);
		float echoR = echoRead(e->bufR, posR);

		if (e->xfade > 0.0f)
		{
			float oldPos = (float)w - e->xfadePos;
			if (oldPos < 0.0f) oldPos += ECHO_BUFLEN;
			float oL = echoRead(e->bufL, oldPos);
			float oR = echoRead(e->bufR, oldPos);
			echoL = echoL * (1.0f - e->xfade) + oL * e->xfade;
			echoR = echoR * (1.0f - e->xfade) + oR * e->xfade;
			e->xfade -= 1.0f / (0.02f * e->fs);
			if (e->xfade < 0.0f) e->xfade = 0.0f;
		}

		if (e->diffusion > 0.0f)
		{
			int aw = w & ECHO_APMASK;
			int dl = e->diffDelay;
			int dr = (int)(e->diffDelay * (1.0f + e->spread * 0.6f));
			if (dr >= ECHO_APLEN) dr = ECHO_APLEN - 1;
			if (dr < 1) dr = 1;
			int pl = aw - dl; if (pl < 0) pl += ECHO_APLEN;
			int pr = aw - dr; if (pr < 0) pr += ECHO_APLEN;
			float apL = e->apL[pl & ECHO_APMASK];
			float apR = e->apR[pr & ECHO_APMASK];
			float xL = echoL + e->diffusion * apL;
			float xR = echoR + e->diffusion * apR;
			e->apL[aw] = xL;
			e->apR[aw] = xR;
			echoL = apL - e->diffusion * xL;
			echoR = apR - e->diffusion * xR;
		}

		echoL = echoSvf(e, 0, echoL);
		echoR = echoSvf(e, 1, echoR);

		if (e->srStep < 0.999f)
		{
			e->srPhase += e->srStep;
			if (e->srPhase >= 1.0f)
			{
				e->srPhase -= 1.0f;
				e->srHoldL = echoL;
				e->srHoldR = echoR;
			}
			echoL = e->srHoldL;
			echoR = e->srHoldR;
		}

		echoL = echoCrush(e, echoL);
		echoR = echoCrush(e, echoR);
		echoL = echoDistort(e, echoL);
		echoR = echoDistort(e, echoR);

		float fbL, fbR;
		if (e->model == 0)
		{
			float m = (echoL + echoR) * 0.5f;
			echoL = m; echoR = m;
			fbL = m; fbR = m;
		}
		else if (e->model == 2)
		{
			fbL = echoR;
			fbR = echoL;
		}
		else
		{
			fbL = echoL;
			fbR = echoR;
		}

		float wrL = inL * e->inputLevel + fbL * e->feedback;
		float wrR = inR * e->inputLevel + fbR * e->feedback;
		if (!isfinite(wrL)) wrL = 0.0f;
		if (!isfinite(wrR)) wrR = 0.0f;
		e->bufL[w] = wrL;
		e->bufR[w] = wrR;

		e->toneZL = echoL + (e->toneZL - echoL) * e->toneCoeff;
		e->toneZR = echoR + (e->toneZR - echoR) * e->toneCoeff;
		float toneL, toneR;
		if (e->tone > 0.001f)      { toneL = echoL - e->toneZL * (e->tone * 0.9f);
		                             toneR = echoR - e->toneZR * (e->tone * 0.9f); }
		else if (e->tone < -0.001f) { toneL = e->toneZL; toneR = e->toneZR; }
		else                        { toneL = echoL;     toneR = echoR; }

		float outL = inL * e->dry + toneL * e->wet;
		float outR = inR * e->dry + toneR * e->wet;
		jdsp->tmpBuffer[0][i] = isfinite(outL) ? outL : inL;
		jdsp->tmpBuffer[1][i] = isfinite(outR) ? outR : inR;

		e->modPhase += e->modInc;
		if (e->modPhase > 6.28318530717959f)
			e->modPhase -= 6.28318530717959f;
		e->widx = (w + 1) & ECHO_MASK;
	}
}

void EchoDelayEnable(JamesDSPLib *jdsp)
{
	// Under the lock: Process may be running on the audio thread, and it must
	// not observe a half-built set of buffers.
	jdsp_lock(jdsp);
	EchoDelay *e = &jdsp->echoDelay;
	if (!jdsp->echoDelayEnabled)
	{
		// Allocate the large delay lines only while the effect is in use; the
		// engine is instantiated per audio session, so idle buffers multiply.
		if (!e->bufL)
			e->bufL = (float*)calloc(ECHO_BUFLEN, sizeof(float));
		if (!e->bufR)
			e->bufR = (float*)calloc(ECHO_BUFLEN, sizeof(float));
		if (!e->bufL || !e->bufR)
		{
			jdsp->echoDelayEnabled = 0;
			jdsp_unlock(jdsp);
			return;
		}
		memset(e->bufL, 0, ECHO_BUFLEN * sizeof(float));
		memset(e->bufR, 0, ECHO_BUFLEN * sizeof(float));
		memset(e->apL, 0, sizeof(e->apL));
		memset(e->apR, 0, sizeof(e->apR));
		e->widx = 0;
		e->modPhase = 0.0f;
		e->svfIc1[0] = e->svfIc1[1] = 0.0f;
		e->svfIc2[0] = e->svfIc2[1] = 0.0f;
		e->srPhase = 0.0f;
		e->srHoldL = e->srHoldR = 0.0f;
		e->toneZL = e->toneZR = 0.0f;
		e->xfade = 0.0f;
		e->delaySamples = e->targetDelay;
		e->diffDelay = (int)(0.0071f * e->fs);
		if (e->diffDelay < 8 || e->diffDelay >= ECHO_APLEN) e->diffDelay = 331;
	}
	jdsp->echoDelayEnabled = 1;
	jdsp_unlock(jdsp);
}

void EchoDelayDisable(JamesDSPLib *jdsp)
{
	EchoDelay *e = &jdsp->echoDelay;
	jdsp->echoDelayEnabled = 0;
	// Clear the flag first so no further block enters, then take the lock,
	// which waits for any block already inside Process to leave. Freeing
	// without that wait is a use-after-free on the audio thread.
	jdsp_lock(jdsp);
	if (e->bufL) { free(e->bufL); e->bufL = 0; }
	if (e->bufR) { free(e->bufR); e->bufR = 0; }
	jdsp_unlock(jdsp);
}
