// Maximiser: a lookahead limiter built to be pushed, rather than the safety
// clamp the output stage already provides.
//
// The output limiter exists to stop the signal leaving the rails. This exists
// to make the signal loud and still leave it intact, which is a different job
// and wants different controls: how much gain goes in, where the ceiling sits,
// how the reduction is shared between clean gain and saturation, whether
// transients are allowed through before the reduction lands, and whether the
// peak being measured is the sample peak or the one that appears between
// samples once a converter reconstructs the waveform.
//
// Four algorithms, differing in how the gain is smoothed rather than in what
// they measure, because that is where limiters actually differ audibly.
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../jdsp_header.h"

#define MAX_MASK (MAXR_BUFLEN - 1)

// ------------------------------------------------------------ peak window
//
// A sliding-window maximum over the lookahead, kept in a monotonic deque:
// indices whose values are already beaten are dropped as they arrive, so the
// front is always the window maximum and the whole thing costs amortised O(1)
// per sample. The alternative, rescanning the window, is O(n) per sample and
// at 1024 samples of lookahead that is not affordable on an audio thread.
// Every index here is unsigned and compared by difference rather than by
// magnitude. The sample counter runs for as long as the effect is enabled -
// twelve hours of playback overflows a signed int, and signed overflow is
// undefined, so a limiter that had been running all day would be free to do
// anything at all. Unsigned wraparound is defined, and the differences stay
// small, so the comparisons keep working across the wrap.
static inline void maxrPush(Maximizer *m, int ch, float v, unsigned idx, unsigned window)
{
	unsigned *dq = m->dq[ch];
	float *dv = m->dqVal[ch];
	unsigned head = m->dqHead[ch], tail = m->dqTail[ch];

	while (tail != head && dv[(tail - 1) & MAX_MASK] <= v)
		tail--;
	dq[tail & MAX_MASK] = idx;
	dv[tail & MAX_MASK] = v;
	tail++;

	// Drop anything that has fallen out of the window behind us.
	while (tail != head && (idx - dq[head & MAX_MASK]) >= window)
		head++;

	m->dqHead[ch] = head;
	m->dqTail[ch] = tail;
}

static inline float maxrWindowMax(Maximizer *m, int ch)
{
	if (m->dqTail[ch] == m->dqHead[ch])
		return 0.0f;
	return m->dqVal[ch][m->dqHead[ch] & MAX_MASK];
}

// ---------------------------------------------------------------- params

void MaximizerSetParam(JamesDSPLib *jdsp,
	int mode, float gainDb, float ceilingDb, float releaseMs,
	float characterPct, float transientPct, int truePeak,
	float stereoLinkPct, int oversample)
{
	Maximizer *m = &jdsp->maximizer;
	float fs = (float)jdsp->fs;
	if (fs < 8000.0f) fs = 48000.0f;
	m->fs = fs;

	if (mode < 0 || mode >= MAXR_MODE_COUNT) mode = MAXR_MODE_TRANSPARENT;
	m->mode = mode;

	if (gainDb < 0.0f) gainDb = 0.0f;
	if (gainDb > 24.0f) gainDb = 24.0f;
	m->inGain = powf(10.0f, gainDb / 20.0f);

	if (ceilingDb > 0.0f) ceilingDb = 0.0f;
	if (ceilingDb < -12.0f) ceilingDb = -12.0f;
	m->ceiling = powf(10.0f, ceilingDb / 20.0f);

	if (releaseMs < 1.0f) releaseMs = 1.0f;
	if (releaseMs > 1000.0f) releaseMs = 1000.0f;

	if (characterPct < 0.0f) characterPct = 0.0f;
	if (characterPct > 100.0f) characterPct = 100.0f;
	m->character = characterPct * 0.01f;

	if (transientPct < 0.0f) transientPct = 0.0f;
	if (transientPct > 100.0f) transientPct = 100.0f;
	m->transient = transientPct * 0.01f;

	m->truePeak = truePeak ? 1 : 0;

	if (stereoLinkPct < 0.0f) stereoLinkPct = 0.0f;
	if (stereoLinkPct > 100.0f) stereoLinkPct = 100.0f;
	m->stereoLink = stereoLinkPct * 0.01f;

	if (oversample < 0) oversample = 0;
	if (oversample > 2) oversample = 2;
	m->osRequest = oversample;

	// Lookahead is how long the limiter has to bring the gain down before the
	// peak arrives; longer is smoother and later. The modes want different
	// amounts, and the transient control shortens it further - letting the
	// leading edge through is exactly what "transient" means here.
	float lookMs;
	switch (mode)
	{
	case MAXR_MODE_PUNCHY:      lookMs = 1.5f; break;
	case MAXR_MODE_WARM:        lookMs = 4.0f; break;
	case MAXR_MODE_AGGRESSIVE:  lookMs = 0.7f; break;
	case MAXR_MODE_TRANSPARENT:
	default:                    lookMs = 3.0f; break;
	}
	lookMs *= (1.0f - m->transient * 0.75f);

	int look = (int)(lookMs * 0.001f * fs);
	if (look < 4) look = 4;
	if (look > MAXR_BUFLEN - 8) look = MAXR_BUFLEN - 8;
	m->lookahead = look;

	// Attack is spread across the lookahead so the reduction is fully in place
	// by the time the peak reaches the output. Release differs per mode: the
	// short ones are what make a limiter sound punchy, and also what makes it
	// pump, which is the trade the mode names are describing.
	float relScale;
	switch (mode)
	{
	case MAXR_MODE_PUNCHY:      relScale = 0.35f; break;
	case MAXR_MODE_WARM:        relScale = 1.60f; break;
	case MAXR_MODE_AGGRESSIVE:  relScale = 0.20f; break;
	case MAXR_MODE_TRANSPARENT:
	default:                    relScale = 1.00f; break;
	}
	float rel = releaseMs * relScale;
	if (rel < 0.5f) rel = 0.5f;
	m->relCoef = expf(-1.0f / (rel * 0.001f * fs));
	m->attCoef = expf(-1.0f / ((float)look * 0.35f + 1.0f));
}

// --------------------------------------------------------------- process

void MaximizerProcess(JamesDSPLib *jdsp, size_t n)
{
	Maximizer *m = &jdsp->maximizer;
	if (!jdsp->tmpBuffer[0] || !jdsp->tmpBuffer[1])
		return;
	if (!m->buf[0] || !m->buf[1])
		return;

	float upsample[MAXR_OS_MAX];
	const float ceiling = m->ceiling;
	const int look = m->lookahead;

	for (size_t i = 0; i < n; i++)
	{
		float in[2] = { jdsp->tmpBuffer[0][i] * m->inGain,
		                jdsp->tmpBuffer[1][i] * m->inGain };
		float gain[2];

		for (int ch = 0; ch < 2; ch++)
		{
			// The peak that matters is not always one of the samples. A
			// converter reconstructs a continuous waveform through them, and
			// its highest point can sit between two samples that are both
			// under the ceiling - which then clips in the DAC, after every
			// meter in the chain has reported it safe. Measuring on an
			// upsampled copy is what catches those.
			float peak = fabsf(in[ch]);
			if (m->truePeak && m->osFactor > 1)
			{
				oversample_stepupSmp(&m->smp[ch], in[ch], upsample);
				for (int j = 0; j < m->osFactor; j++)
				{
					float a = fabsf(upsample[j]);
					if (a > peak) peak = a;
				}
			}

			maxrPush(m, ch, peak, m->widx, look);

			// Store the input so it can be read back delayed by the lookahead.
			m->buf[ch][m->widx & MAX_MASK] = in[ch];

			float windowPeak = maxrWindowMax(m, ch);
			float target = (windowPeak > ceiling && windowPeak > 1e-9f)
				? ceiling / windowPeak : 1.0f;

			// Downward movement uses the attack coefficient so the reduction
			// arrives smoothly rather than as a step, which would be heard as
			// a click; upward movement is the release.
			float g = m->gainState[ch];
			if (target < g)
				g = target + (g - target) * m->attCoef;
			else
				g = target + (g - target) * m->relCoef;
			m->gainState[ch] = g;
			gain[ch] = g;
		}

		// Stereo link decides whether a peak on one side pulls the other down
		// with it. Fully linked holds the image still and is what mastering
		// limiters do by default; unlinked reduces less overall but lets loud
		// transients on one side pull the image toward the other.
		float linked = gain[0] < gain[1] ? gain[0] : gain[1];
		gain[0] = linked * m->stereoLink + gain[0] * (1.0f - m->stereoLink);
		gain[1] = linked * m->stereoLink + gain[1] * (1.0f - m->stereoLink);

		int rp = (m->widx - look) & MAX_MASK;
		for (int ch = 0; ch < 2; ch++)
		{
			float y = m->buf[ch][rp] * gain[ch];

			// Character trades clean gain reduction for saturation: the same
			// loudness, arrived at by rounding the peaks instead of turning
			// everything down around them. At zero this is inaudible.
			if (m->character > 0.0f)
			{
				float driven = y / (ceiling > 1e-6f ? ceiling : 1e-6f);
				float sat = tanhf(driven * (1.0f + m->character * 2.0f)) * ceiling;
				y = y * (1.0f - m->character) + sat * m->character;
			}

			// Whatever the smoothing let through, the ceiling is absolute -
			// that is the one promise a maximiser has to keep.
			if (y > ceiling) y = ceiling;
			else if (y < -ceiling) y = -ceiling;

			if (!isfinite(y))
			{
				y = 0.0f;
				m->gainState[ch] = 1.0f;
			}
			jdsp->tmpBuffer[ch][i] = y;
		}

		m->widx++;
	}
}

// ------------------------------------------------------------- lifecycle

void MaximizerEnable(JamesDSPLib *jdsp)
{
	Maximizer *m = &jdsp->maximizer;
	if (!jdsp->maximizerEnabled)
	{
		float fs = (float)jdsp->fs;
		if (fs < 8000.0f) fs = 48000.0f;

		for (int ch = 0; ch < 2; ch++)
		{
			if (!m->buf[ch]) m->buf[ch] = (float*)calloc(MAXR_BUFLEN, sizeof(float));
			if (!m->dq[ch]) m->dq[ch] = (int*)calloc(MAXR_BUFLEN, sizeof(int));
			if (!m->dqVal[ch]) m->dqVal[ch] = (float*)calloc(MAXR_BUFLEN, sizeof(float));
		}
		if (!m->buf[0] || !m->buf[1] || !m->dq[0] || !m->dq[1] || !m->dqVal[0] || !m->dqVal[1])
		{
			MaximizerDisable(jdsp);
			return;
		}

		// True-peak detection needs the upsampled copy; 4x resolves inter-
		// sample peaks to within a fraction of a dB, which is as much as the
		// ceiling control can act on anyway.
		m->osFactor = (m->osRequest == 2) ? 4 : (m->osRequest == 1 ? 2 : 1);
		if (m->osFactor > 1)
		{
			oversample_makeSmp(&m->smp[0], m->osFactor);
			oversample_makeSmp(&m->smp[1], m->osFactor);
		}

		m->widx = 0;
		m->dqHead[0] = m->dqTail[0] = 0;
		m->dqHead[1] = m->dqTail[1] = 0;
		m->gainState[0] = m->gainState[1] = 1.0f;
	}
	jdsp->maximizerEnabled = 1;
}

void MaximizerDisable(JamesDSPLib *jdsp)
{
	Maximizer *m = &jdsp->maximizer;
	jdsp->maximizerEnabled = 0;
	for (int ch = 0; ch < 2; ch++)
	{
		if (m->buf[ch]) { free(m->buf[ch]); m->buf[ch] = 0; }
		if (m->dq[ch]) { free(m->dq[ch]); m->dq[ch] = 0; }
		if (m->dqVal[ch]) { free(m->dqVal[ch]); m->dqVal[ch] = 0; }
	}
}
