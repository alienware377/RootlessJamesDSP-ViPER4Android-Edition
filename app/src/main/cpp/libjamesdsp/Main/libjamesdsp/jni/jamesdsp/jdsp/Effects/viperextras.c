// ViPER Clarity, Field Surround and AGC — inspired ports of the remaining
// ViperFX effects for the viper-extras fork.
#include <math.h>
#include <string.h>
#include "../jdsp_header.h"

// ---------------- ViPER Clarity ----------------
static void vcShelf(float *c, float fs, float f0, float gainDb)
{
	float A = powf(10.0f, gainDb / 40.0f);
	float w0 = 2.0f * 3.14159265358979f * f0 / fs;
	float cw = cosf(w0);
	float sw = sinf(w0);
	float alpha = sw / 2.0f * sqrtf((A + 1.0f / A) * (1.0f / 0.9f - 1.0f) + 2.0f);
	float a0 = (A + 1.0f) - (A - 1.0f) * cw + 2.0f * sqrtf(A) * alpha;
	c[0] = (A * ((A + 1.0f) + (A - 1.0f) * cw + 2.0f * sqrtf(A) * alpha)) / a0;
	c[1] = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw)) / a0;
	c[2] = (A * ((A + 1.0f) + (A - 1.0f) * cw - 2.0f * sqrtf(A) * alpha)) / a0;
	c[3] = (2.0f * ((A - 1.0f) - (A + 1.0f) * cw)) / a0;
	c[4] = ((A + 1.0f) - (A - 1.0f) * cw - 2.0f * sqrtf(A) * alpha) / a0;
}

static float vcBq(const float *c, float *z, float x)
{
	float y = c[0] * x + c[1] * z[0] + c[2] * z[1] - c[3] * z[2] - c[4] * z[3];
	z[1] = z[0]; z[0] = x;
	z[3] = z[2]; z[2] = y;
	return y;
}

void ViperClaritySetParam(JamesDSPLib *jdsp, int mode, float gainDb)
{
	ViperClarity *vc = &jdsp->viperClarity;
	float fs = (float)jdsp->fs;
	if (fs < 8000.0f)
		fs = 48000.0f;
	if (gainDb < 0.0f) gainDb = 0.0f;
	if (gainDb > 14.0f) gainDb = 14.0f;
	vc->mode = mode;
	// Natural: derivative sharpening strength. The transfer is
	// 1 + sharp * (1 - z^-1), which reaches 1 + 2 * sharp at Nyquist, so this
	// scale is what decides whether the mode is audible at all. At the old
	// 0.02 the shipped gain gave 0.07 - a third of a decibel at 8kHz, which
	// is why Natural sounded like nothing.
	vc->sharp = gainDb * 0.10f;
	// OZone / XHiFi: high shelf at 4.5k
	vcShelf(vc->shelf, fs, 4500.0f, gainDb * (mode == 2 ? 0.8f : 0.5f));
}

void ViperClarityProcess(JamesDSPLib *jdsp, size_t n)
{
	ViperClarity *vc = &jdsp->viperClarity;
	size_t i;
	int c;
	if (!jdsp->tmpBuffer[0] || !jdsp->tmpBuffer[1])
		return;
	for (i = 0; i < n; i++)
	{
		for (c = 0; c < 2; c++)
		{
			float x = jdsp->tmpBuffer[c][i];
			float y = x;
			if (vc->mode == 0 || vc->mode == 1)
			{
				// noise sharpening: boost sample-to-sample transitions
				y = x + vc->sharp * (x - vc->prev[c]);
				vc->prev[c] = x;
			}
			if (vc->mode == 1 || vc->mode == 2)
				y = vcBq(vc->shelf, vc->shelfZ[c], y);
			jdsp->tmpBuffer[c][i] = y;
		}
	}
}

void ViperClarityEnable(JamesDSPLib *jdsp)
{
	if (!jdsp->viperClarityEnabled)
	{
		memset(jdsp->viperClarity.shelfZ, 0, sizeof(jdsp->viperClarity.shelfZ));
		jdsp->viperClarity.prev[0] = jdsp->viperClarity.prev[1] = 0.0f;
	}
	jdsp->viperClarityEnabled = 1;
}

void ViperClarityDisable(JamesDSPLib *jdsp)
{
	jdsp->viperClarityEnabled = 0;
}

// ---------------- Field Surround ----------------
void FieldSurroundSetParam(JamesDSPLib *jdsp, float strengthPct, float midImagePct)
{
	FieldSurround *f = &jdsp->fieldSurround;
	f->sideGain = 1.0f + strengthPct * 0.02f;   // 0..100 -> 1..3
	f->midGain = 0.5f + midImagePct * 0.01f;    // 0..100 -> 0.5..1.5
}

void FieldSurroundProcess(JamesDSPLib *jdsp, size_t n)
{
	FieldSurround *f = &jdsp->fieldSurround;
	size_t i;
	if (!jdsp->tmpBuffer[0] || !jdsp->tmpBuffer[1])
		return;
	for (i = 0; i < n; i++)
	{
		float l = jdsp->tmpBuffer[0][i];
		float r = jdsp->tmpBuffer[1][i];
		float mid = (l + r) * 0.5f * f->midGain;
		float side = (l - r) * 0.5f * f->sideGain;
		jdsp->tmpBuffer[0][i] = mid + side;
		jdsp->tmpBuffer[1][i] = mid - side;
	}
}

void FieldSurroundEnable(JamesDSPLib *jdsp)
{
	jdsp->fieldSurroundEnabled = 1;
}

void FieldSurroundDisable(JamesDSPLib *jdsp)
{
	jdsp->fieldSurroundEnabled = 0;
}

// ---------------- Auto Gain Control ----------------
void AgcSetParam(JamesDSPLib *jdsp, float targetPct, float maxBoostDb)
{
	Agc *a = &jdsp->agc;
	float fs = (float)jdsp->fs;
	if (fs < 8000.0f)
		fs = 48000.0f;
	a->target = targetPct * 0.01f;
	if (a->target < 0.05f) a->target = 0.05f;
	a->maxGain = powf(10.0f, maxBoostDb / 20.0f);
	// slow envelope (~300ms) and very slow gain glide (~1.5s)
	a->envCoef = 1.0f - expf(-1.0f / (0.3f * fs));
	a->gainCoef = 1.0f - expf(-1.0f / (1.5f * fs));
	if (a->gain < 1.0f)
		a->gain = 1.0f;
}

void AgcProcess(JamesDSPLib *jdsp, size_t n)
{
	Agc *a = &jdsp->agc;
	size_t i;
	if (!jdsp->tmpBuffer[0] || !jdsp->tmpBuffer[1])
		return;
	for (i = 0; i < n; i++)
	{
		float l = jdsp->tmpBuffer[0][i];
		float r = jdsp->tmpBuffer[1][i];
		float mag = fabsf(l);
		float magR = fabsf(r);
		if (magR > mag)
			mag = magR;
		a->env += a->envCoef * (mag - a->env);
		if (a->env > 0.002f)
		{
			float desired = a->target / a->env;
			if (desired > a->maxGain) desired = a->maxGain;
			if (desired < 0.2f) desired = 0.2f;
			a->gain += a->gainCoef * (desired - a->gain);
		}
		jdsp->tmpBuffer[0][i] = l * a->gain;
		jdsp->tmpBuffer[1][i] = r * a->gain;
	}
}

void AgcEnable(JamesDSPLib *jdsp)
{
	if (!jdsp->agcEnabled)
	{
		jdsp->agc.env = 0.0f;
		jdsp->agc.gain = 1.0f;
	}
	jdsp->agcEnabled = 1;
}

void AgcDisable(JamesDSPLib *jdsp)
{
	jdsp->agcEnabled = 0;
}
