// Multiband distortion.
//
// A band-select filter chooses what gets distorted, the distortion stage
// mangles it, and the result is either put back in place of the band it came
// from or added on top of the original.
//
// The band-select filter is the same parametric band list the equaliser
// editor produces - a cascade of biquads - so the graph the user draws is
// literally the filter feeding the distortion. Peaking and shelf bands shape
// what goes in; low-pass and high-pass bands select a range outright, which is
// what makes this multiband rather than a single global saturator.
//
// The split routing has a property worth stating, because it is what makes the
// effect safe to leave enabled: out = (x - b) + f(b), so when f is the identity
// the output is exactly x, whatever the filter did. The residual does not have
// to be a phase-perfect complement of the band for the null to hold; it holds
// by construction.
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "../jdsp_header.h"

#define MBD_CHMASK (MBD_CHORUS_BUFLEN - 1)

// ---------------------------------------------------------------- shapers

// Every shaper takes a signal already scaled by drive and returns something
// bounded, so the stage cannot run away no matter what the band filter hands
// it. `shape` is 0..1 and means something different per model - it is the
// "character" control, documented per case.
static inline float mbdShape(int model, float x, float shape)
{
	switch (model)
	{
	case MBD_MODEL_SOFT:
	default:
		return tanhf(x);

	case MBD_MODEL_HARD:
	{
		// shape softens the corner: near 0 this is a true hard clip, near 1
		// the last stretch before the rail is rounded off. The round is a
		// quadratic joining slope 1 where it starts to slope 0 where it ends,
		// so there is no discontinuity in the curve or its first derivative
		// - a kink in either is audible as a buzz that does not belong to the
		// distortion being asked for.
		float knee = 0.02f + shape * 0.4f;
		float t = 1.0f - knee;
		float a = fabsf(x);
		float s = (x < 0.0f) ? -1.0f : 1.0f;
		if (a <= t)
			return x;
		if (a >= t + 2.0f * knee)
			return s * (t + knee);
		float u = a - t;
		return s * (t + u - (u * u) / (4.0f * knee));
	}

	case MBD_MODEL_TUBE:
		// Asymmetric, so it generates even harmonics - the "warm" ones.
		// shape leans the asymmetry further.
		if (x >= 0.0f)
			return x / (1.0f + (0.6f + shape * 0.5f) * x);
		return x / (1.0f - (0.4f + shape * 0.4f) * x);

	case MBD_MODEL_OVERDRIVE:
	{
		// The classic piecewise cubic. shape moves where the cubic region
		// starts, so low values stay clean longer before breaking up.
		float t = 0.20f + shape * 0.25f;
		float a = fabsf(x);
		float s = (x < 0.0f) ? -1.0f : 1.0f;
		if (a < t)
			return 2.0f * x;
		if (a < 2.0f * t)
		{
			float u = 2.0f - a / t;
			return s * (3.0f - u * u) / 3.0f;
		}
		return s;
	}

	case MBD_MODEL_FOLD:
	{
		// Wavefolder: instead of clipping at the rails the signal turns
		// around, which is where the metallic upper harmonics come from.
		// shape sets how many times it may fold.
		//
		// Done by reduction into one period of the triangle rather than by
		// iterating the reflection. An iterative fold only walks the value
		// back by two per pass, so a signal the drive stage has pushed to 25
		// is still at 13 after six passes - it escapes, and the harness
		// caught it doing exactly that. The reduction is exact for any input.
		float folds = 1.0f + shape * 3.0f;
		float y = x * folds;
		y = y - 4.0f * floorf((y + 2.0f) * 0.25f);
		if (y > 1.0f) y = 2.0f - y;
		else if (y < -1.0f) y = -2.0f - y;
		return y;
	}

	case MBD_MODEL_FUZZ:
	{
		// Hard, gated, asymmetric. shape is how abruptly it saturates.
		float k = 2.0f + shape * 8.0f;
		float s = (x < 0.0f) ? -1.0f : 1.0f;
		float a = fabsf(x);
		float y = s * (1.0f - expf(-a * k));
		// Squash the negative half harder for the asymmetry fuzz circuits
		// have, which is a large part of why they sound the way they do.
		if (x < 0.0f) y *= 0.7f + shape * 0.2f;
		return y;
	}

	case MBD_MODEL_RECTIFY:
	{
		// Blends toward a full-wave rectifier, which doubles the perceived
		// pitch of whatever it is fed. Useful on a low band for octave-up
		// weight; unpleasant on a wide one, which is the point of choosing
		// a range.
		//
		// The rectifier runs on the saturated signal, not the raw one:
		// |x| * 2 - 1 grows without bound as the drive rises, so rectifying
		// before limiting hands the rest of the chain a signal fifty times
		// the rails.
		float t = tanhf(x);
		float r = fabsf(t) * 2.0f - 1.0f;
		return t * (1.0f - shape) + r * shape;
	}

	case MBD_MODEL_CRUSH:
		// Bit and rate reduction happen in the loop, where the state lives.
		// Soft-limit here so a crushed signal still cannot exceed the rails.
		return tanhf(x);
	}
}

// ---------------------------------------------------------------- helpers

static void mbdBiquadCoeffs(float fs, int type, float freq, float gainDb, float q,
	float *b0, float *b1, float *b2, float *a1, float *a2)
{
	// Robert Bristow-Johnson's cookbook, matching BiquadUtils.kt on the app
	// side so the curve drawn on the graph is the curve the engine applies.
	if (freq < 10.0f) freq = 10.0f;
	if (freq > fs * 0.48f) freq = fs * 0.48f;
	if (q < 0.05f) q = 0.05f;
	if (q > 40.0f) q = 40.0f;

	double A = pow(10.0, gainDb / 40.0);
	double w = 2.0 * M_PI * (double)freq / (double)fs;
	double sw = sin(w), cw = cos(w);
	double alpha = sw / (2.0 * (double)q);
	double B0, B1, B2, A0, A1, A2;

	switch (type)
	{
	case MBD_FILTER_LOW_SHELF:
	{
		double sq = 2.0 * sqrt(A) * alpha;
		B0 = A * ((A + 1.0) - (A - 1.0) * cw + sq);
		B1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
		B2 = A * ((A + 1.0) - (A - 1.0) * cw - sq);
		A0 = (A + 1.0) + (A - 1.0) * cw + sq;
		A1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
		A2 = (A + 1.0) + (A - 1.0) * cw - sq;
		break;
	}
	case MBD_FILTER_HIGH_SHELF:
	{
		double sq = 2.0 * sqrt(A) * alpha;
		B0 = A * ((A + 1.0) + (A - 1.0) * cw + sq);
		B1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
		B2 = A * ((A + 1.0) + (A - 1.0) * cw - sq);
		A0 = (A + 1.0) - (A - 1.0) * cw + sq;
		A1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
		A2 = (A + 1.0) - (A - 1.0) * cw - sq;
		break;
	}
	case MBD_FILTER_LOW_PASS:
		B0 = (1.0 - cw) * 0.5;
		B1 = 1.0 - cw;
		B2 = (1.0 - cw) * 0.5;
		A0 = 1.0 + alpha;
		A1 = -2.0 * cw;
		A2 = 1.0 - alpha;
		break;
	case MBD_FILTER_HIGH_PASS:
		B0 = (1.0 + cw) * 0.5;
		B1 = -(1.0 + cw);
		B2 = (1.0 + cw) * 0.5;
		A0 = 1.0 + alpha;
		A1 = -2.0 * cw;
		A2 = 1.0 - alpha;
		break;
	case MBD_FILTER_PEAKING:
	default:
		B0 = 1.0 + alpha * A;
		B1 = -2.0 * cw;
		B2 = 1.0 - alpha * A;
		A0 = 1.0 + alpha / A;
		A1 = -2.0 * cw;
		A2 = 1.0 - alpha / A;
		break;
	}

	if (fabs(A0) < 1e-12) A0 = 1e-12;
	*b0 = (float)(B0 / A0);
	*b1 = (float)(B1 / A0);
	*b2 = (float)(B2 / A0);
	*a1 = (float)(A1 / A0);
	*a2 = (float)(A2 / A0);
}

static inline float mbdBiquadRun(MultibandDist *m, int ch, int idx, float x)
{
	// Transposed direct form II: fewer state updates and better behaved in
	// single precision than the direct form.
	float y = m->b0[idx] * x + m->z1[ch][idx];
	m->z1[ch][idx] = m->b1[idx] * x - m->a1[idx] * y + m->z2[ch][idx];
	m->z2[ch][idx] = m->b2[idx] * x - m->a2[idx] * y;
	return y;
}

// ---------------------------------------------------------------- params

void MultibandDistSetBands(JamesDSPLib *jdsp, const float *bands, int count)
{
	MultibandDist *m = &jdsp->multibandDist;
	float fs = (float)jdsp->fs;
	if (fs < 8000.0f) fs = 48000.0f;

	if (!bands || count <= 0)
	{
		m->numBands = 0;
		return;
	}
	if (count > MBD_MAX_BANDS) count = MBD_MAX_BANDS;

	for (int i = 0; i < count; i++)
	{
		const float *b = bands + i * 4;
		mbdBiquadCoeffs(fs, (int)b[3], b[0], b[1], b[2],
			&m->b0[i], &m->b1[i], &m->b2[i], &m->a1[i], &m->a2[i]);
	}
	// Only publish the new count once every coefficient behind it is written,
	// so the audio thread can never read a slot that is half updated.
	m->numBands = count;
}

void MultibandDistSetParam(JamesDSPLib *jdsp,
	int routing, int model,
	float drivePct, float biasPct, float shapePct,
	float bits, float downsamplePct,
	float tonePct, float bandGainPct,
	float chorusRateHz, float chorusDepthMs, float chorusFeedbackPct,
	float chorusSpreadPct, int chorusVoices, float chorusMixPct,
	float mixPct)
{
	MultibandDist *m = &jdsp->multibandDist;
	float fs = (float)jdsp->fs;
	if (fs < 8000.0f) fs = 48000.0f;
	m->fs = fs;

	m->routing = (routing == MBD_ROUTING_PARALLEL) ? MBD_ROUTING_PARALLEL : MBD_ROUTING_SPLIT;
	if (model < 0 || model >= MBD_MODEL_COUNT) model = MBD_MODEL_SOFT;
	m->model = model;

	// Drive is a percentage on the dial and a gain in the engine.
	//
	// Unity gain alone does not make the stage transparent, because the
	// shapers are curves rather than lines - tanh at unity already costs a
	// few percent, which the harness measured as a tenth of full scale of
	// deviation. So the shaper is also faded in over the bottom of the range:
	// at 0 the band passes through untouched and the split routing nulls
	// exactly, and by 10 it is fully engaged. Without this the effect can be
	// switched on but never switched off short of disabling the card.
	if (drivePct < 0.0f) drivePct = 0.0f;
	if (drivePct > 100.0f) drivePct = 100.0f;
	m->drive = powf(10.0f, (drivePct * 0.01f) * 1.5f);
	m->shaperBlend = drivePct * 0.1f;
	if (m->shaperBlend > 1.0f) m->shaperBlend = 1.0f;

	if (biasPct < -100.0f) biasPct = -100.0f;
	if (biasPct > 100.0f) biasPct = 100.0f;
	m->bias = biasPct * 0.005f;

	if (shapePct < 0.0f) shapePct = 0.0f;
	if (shapePct > 100.0f) shapePct = 100.0f;
	m->shape = shapePct * 0.01f;

	if (bits < 1.0f) bits = 1.0f;
	if (bits > 16.0f) bits = 16.0f;
	m->bitStep = 1.0f / powf(2.0f, bits - 1.0f);

	if (downsamplePct < 0.0f) downsamplePct = 0.0f;
	if (downsamplePct > 100.0f) downsamplePct = 100.0f;
	// 0% holds nothing, 100% holds for 32 samples. Kept modest because the
	// aliasing this makes is the point, and past that it is just noise.
	m->holdLen = 1.0f + downsamplePct * 0.31f;

	if (tonePct < 0.0f) tonePct = 0.0f;
	if (tonePct > 100.0f) tonePct = 100.0f;
	m->tilt = tonePct * 0.02f - 1.0f;

	if (bandGainPct < 0.0f) bandGainPct = 0.0f;
	if (bandGainPct > 200.0f) bandGainPct = 200.0f;
	m->bandGain = bandGainPct * 0.01f;

	if (chorusRateHz < 0.01f) chorusRateHz = 0.01f;
	if (chorusRateHz > 10.0f) chorusRateHz = 10.0f;
	m->chInc = chorusRateHz / fs;

	if (chorusDepthMs < 0.0f) chorusDepthMs = 0.0f;
	if (chorusDepthMs > 25.0f) chorusDepthMs = 25.0f;
	m->chDepth = chorusDepthMs * 0.001f * fs;

	// Base delay plus modulation has to stay inside the ring, whatever the
	// sample rate - at 192kHz the buffer is a much shorter span of time.
	m->chBase = 0.012f * fs;
	float span = (float)(MBD_CHORUS_BUFLEN - 8);
	if (m->chBase + m->chDepth > span)
	{
		float over = (m->chBase + m->chDepth) - span;
		if (m->chDepth >= over) m->chDepth -= over;
		else { m->chDepth = 0.0f; m->chBase = span; }
	}

	if (chorusFeedbackPct < 0.0f) chorusFeedbackPct = 0.0f;
	if (chorusFeedbackPct > 90.0f) chorusFeedbackPct = 90.0f;
	m->chFeedback = chorusFeedbackPct * 0.01f;

	if (chorusSpreadPct < 0.0f) chorusSpreadPct = 0.0f;
	if (chorusSpreadPct > 100.0f) chorusSpreadPct = 100.0f;
	m->chSpread = chorusSpreadPct * 0.005f;

	if (chorusVoices < 1) chorusVoices = 1;
	if (chorusVoices > MBD_CHORUS_VOICES) chorusVoices = MBD_CHORUS_VOICES;
	m->chVoices = chorusVoices;

	if (chorusMixPct < 0.0f) chorusMixPct = 0.0f;
	if (chorusMixPct > 100.0f) chorusMixPct = 100.0f;
	m->chMix = chorusMixPct * 0.01f;

	if (mixPct < 0.0f) mixPct = 0.0f;
	if (mixPct > 100.0f) mixPct = 100.0f;
	m->mix = mixPct * 0.01f;

	// One-pole corner for the tilt, and the DC blocker that keeps bias from
	// walking the output away from zero.
	float tc = 2.0f * (float)M_PI * 900.0f / fs;
	m->toneA = tc / (1.0f + tc);
	m->dcR = 1.0f - (126.0f / fs);

	// When every control that could colour the band is at its neutral point
	// the whole stage is skipped, rather than run and cancelled. Two reasons
	// this is worth a flag. It costs nothing on a card the user has enabled
	// but not yet dialled in, which is the state it ships in. And it is the
	// only way the null is exact: the oversampling filters, the DC blocker
	// and the tilt each leave a trace even when asked to do nothing, so
	// "cancelled out" would still be audible on a null test.
	m->transparent = (m->mix <= 0.0f) ||
		(m->shaperBlend <= 0.0f && m->chMix <= 0.0001f &&
		 fabsf(m->tilt) < 1e-6f && fabsf(m->bandGain - 1.0f) < 1e-6f &&
		 fabsf(m->bias) < 1e-9f && m->routing == MBD_ROUTING_SPLIT);
}

// ---------------------------------------------------------------- process

void MultibandDistProcess(JamesDSPLib *jdsp, size_t n)
{
	MultibandDist *m = &jdsp->multibandDist;
	if (!jdsp->tmpBuffer[0] || !jdsp->tmpBuffer[1])
		return;
	if (m->transparent)
		return;

	float *chBuf[2] = { m->chBufL, m->chBufR };
	int haveChorus = (m->chBufL && m->chBufR && m->chMix > 0.0001f);
	float upsample[MBD_OS_MAX];

	for (size_t i = 0; i < n; i++)
	{
		float in[2] = { jdsp->tmpBuffer[0][i], jdsp->tmpBuffer[1][i] };
		float out[2];

		for (int ch = 0; ch < 2; ch++)
		{
			float x = in[ch];

			// 1. Band select. With no bands the whole signal goes in, which
			//    is the sensible reading of "no range chosen".
			float b = x;
			for (int k = 0; k < m->numBands; k++)
				b = mbdBiquadRun(m, ch, k, b);

			// 2. Distort. Oversampled where the sample rate leaves room for
			//    the harmonics this generates; without it the upper ones fold
			//    back down as aliasing, which is what makes cheap distortion
			//    sound gritty in the wrong way.
			float d;
			float driven = b * m->drive + m->bias;
			if (m->osFactor > 1)
			{
				oversample_stepupSmp(&m->smpUp[ch], driven, upsample);
				for (int j = 0; j < m->osFactor; j++)
					upsample[j] = upsample[j] * (1.0f - m->shaperBlend) +
						mbdShape(m->model, upsample[j], m->shape) * m->shaperBlend;
				d = oversample_stepdownSmpFloat(&m->smpDown[ch], upsample);
			}
			else
			{
				d = driven * (1.0f - m->shaperBlend) +
					mbdShape(m->model, driven, m->shape) * m->shaperBlend;
			}

			// 3. Bit and rate reduction, which need their own state and so
			//    cannot live in the shaper.
			if (m->model == MBD_MODEL_CRUSH)
			{
				m->shPhase[ch] += 1.0f;
				if (m->shPhase[ch] >= m->holdLen)
				{
					m->shPhase[ch] -= m->holdLen;
					float q = m->bitStep;
					m->shHold[ch] = floorf(d / q + 0.5f) * q;
				}
				d = m->shHold[ch];
			}

			// 4. Remove the DC the bias put there, before it reaches anything
			//    that integrates it.
			float dc = d - m->dcX[ch] + m->dcR * m->dcY[ch];
			m->dcX[ch] = d;
			m->dcY[ch] = dc;
			d = dc;

			// 5. Tone: a tilt around a fixed corner, neutral in the middle of
			//    the dial rather than at one end.
			m->toneZ[ch] += m->toneA * (d - m->toneZ[ch]);
			float lp = m->toneZ[ch];
			d = lp * (1.0f - m->tilt) + (d - lp) * (1.0f + m->tilt);

			// 6. Chorus on the distorted band only. Distortion flattens the
			//    movement out of a signal, and this is what puts some back.
			if (haveChorus)
			{
				float wet = 0.0f;
				float phase = m->chPhase + (ch ? m->chSpread : 0.0f);
				for (int v = 0; v < m->chVoices; v++)
				{
					float p = phase + (float)v / (float)m->chVoices;
					p -= floorf(p);
					float lfo = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * p);
					float delay = m->chBase + m->chDepth * lfo;
					float rp = (float)m->chPos[ch] - delay;
					while (rp < 0.0f) rp += (float)MBD_CHORUS_BUFLEN;
					int i0 = (int)rp;
					float fr = rp - (float)i0;
					float s0 = chBuf[ch][i0 & MBD_CHMASK];
					float s1 = chBuf[ch][(i0 + 1) & MBD_CHMASK];
					wet += s0 + (s1 - s0) * fr;
				}
				wet /= (float)m->chVoices;
				chBuf[ch][m->chPos[ch]] = d + wet * m->chFeedback;
				m->chPos[ch] = (m->chPos[ch] + 1) & MBD_CHMASK;
				d = d * (1.0f - m->chMix) + wet * m->chMix;
			}

			d *= m->bandGain;

			// 7. Put it back. Split substitutes the band; parallel adds on
			//    top, which is what a saturator-on-a-send does.
			float processed = (m->routing == MBD_ROUTING_SPLIT) ? (x - b) + d : x + d;
			out[ch] = x * (1.0f - m->mix) + processed * m->mix;

			if (!isfinite(out[ch]))
			{
				// Nothing above should be able to produce this, but a single
				// bad sample must not be able to poison the state it feeds
				// back into, so reset rather than propagate.
				out[ch] = 0.0f;
				m->dcX[ch] = m->dcY[ch] = m->toneZ[ch] = 0.0f;
				for (int k = 0; k < MBD_MAX_BANDS; k++)
					m->z1[ch][k] = m->z2[ch][k] = 0.0f;
				if (chBuf[ch]) memset(chBuf[ch], 0, MBD_CHORUS_BUFLEN * sizeof(float));
			}
		}

		m->chPhase += m->chInc;
		if (m->chPhase >= 1.0f) m->chPhase -= 1.0f;

		jdsp->tmpBuffer[0][i] = out[0];
		jdsp->tmpBuffer[1][i] = out[1];
	}
}

// ---------------------------------------------------------------- lifecycle

void MultibandDistEnable(JamesDSPLib *jdsp)
{
	MultibandDist *m = &jdsp->multibandDist;
	if (!jdsp->multibandDistEnabled)
	{
		float fs = (float)jdsp->fs;
		if (fs < 8000.0f) fs = 48000.0f;

		if (!m->chBufL)
			m->chBufL = (float*)calloc(MBD_CHORUS_BUFLEN, sizeof(float));
		if (!m->chBufR)
			m->chBufR = (float*)calloc(MBD_CHORUS_BUFLEN, sizeof(float));
		if (!m->chBufL || !m->chBufR)
		{
			// Refuse to run half allocated rather than dereference a null on
			// the audio thread.
			if (m->chBufL) { free(m->chBufL); m->chBufL = 0; }
			if (m->chBufR) { free(m->chBufR); m->chBufR = 0; }
			jdsp->multibandDistEnabled = 0;
			return;
		}

		// 2x is enough to push the first order of folded harmonics above
		// anything audible at 44.1/48k; above 96k the headroom is already
		// there and the extra cost buys nothing.
		m->osFactor = (fs < 96000.0f) ? 2 : 1;
		if (m->osFactor > 1)
		{
			oversample_makeSmp(&m->smpUp[0], m->osFactor);
			oversample_makeSmp(&m->smpUp[1], m->osFactor);
			oversample_makeSmp(&m->smpDown[0], m->osFactor);
			oversample_makeSmp(&m->smpDown[1], m->osFactor);
		}

		memset(m->z1, 0, sizeof(m->z1));
		memset(m->z2, 0, sizeof(m->z2));
		memset(m->dcX, 0, sizeof(m->dcX));
		memset(m->dcY, 0, sizeof(m->dcY));
		memset(m->toneZ, 0, sizeof(m->toneZ));
		memset(m->shHold, 0, sizeof(m->shHold));
		memset(m->shPhase, 0, sizeof(m->shPhase));
		m->chPos[0] = m->chPos[1] = 0;
		m->chPhase = 0.0f;
	}
	jdsp->multibandDistEnabled = 1;
}

void MultibandDistDisable(JamesDSPLib *jdsp)
{
	MultibandDist *m = &jdsp->multibandDist;
	jdsp->multibandDistEnabled = 0;
	if (m->chBufL) { free(m->chBufL); m->chBufL = 0; }
	if (m->chBufR) { free(m->chBufR); m->chBufR = 0; }
}
