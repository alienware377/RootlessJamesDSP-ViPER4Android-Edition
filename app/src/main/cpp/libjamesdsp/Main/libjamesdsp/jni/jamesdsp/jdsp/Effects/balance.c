/*
 * Balance: which ear the sound sits in, and two things that belong beside it.
 *
 * Small, but genuinely missing - there was no way to trim one ear against the
 * other, which matters to anyone with uneven hearing, a headphone with a tired
 * driver, or earbuds where one has more wax in it than the other.
 *
 *   Balance   Cuts the opposite side rather than boosting the near one. That
 *             is the classic balance law and it is the right one here: boosting
 *             costs headroom and can clip material that was already close to
 *             full scale, and the limiter downstream would then be working for
 *             a reason the user never asked for.
 *   Swap      Left and right exchanged. Trivial, and the usual fix for a
 *             cable, an adapter or a badly mastered recording.
 *   Mono      Blends the two channels together. Fully mono is the accessible
 *             option for single-sided listening - with a normal stereo mix,
 *             one earbud otherwise loses whatever was panned to the other.
 *
 * Every control is smoothed rather than applied instantly. A balance slider is
 * dragged, not typed, so an unsmoothed gain would step on every touch event -
 * which on a sustained note is a rasp rather than a click, and far more
 * noticeable than the change being asked for.
 */
#include <math.h>
#include <string.h>
#include "../jdsp_header.h"

/* Time constant for the smoothing. Long enough that a fast drag is inaudible,
   short enough that the control still feels immediate. */
#define BALANCE_SMOOTH_MS 25.0f

static void balanceDesign(Balance *b, float fs)
{
	b->smooth = expf(-1.0f / ((BALANCE_SMOOTH_MS * 0.001f) * fs));
	b->fs = fs;
}

void BalanceRefresh(JamesDSPLib *jdsp)
{
	balanceDesign(&jdsp->balance, jdsp->fs > 0.0f ? jdsp->fs : 48000.0f);
}

void BalanceSetParam(JamesDSPLib *jdsp, float balancePct, int swap, float monoPct)
{
	/* Held for the whole update: Process runs on the audio thread under this
	   same lock. See the note in tape.c. */
	jdsp_lock(jdsp);
	Balance *b = &jdsp->balance;
	const float fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;

	if (balancePct < -100.0f) balancePct = -100.0f;
	if (balancePct > 100.0f) balancePct = 100.0f;
	b->balance = balancePct;

	/* Attenuate the far side; never boost. */
	if (balancePct < 0.0f)
	{
		b->targetL = 1.0f;
		b->targetR = 1.0f + balancePct * 0.01f;
	}
	else
	{
		b->targetL = 1.0f - balancePct * 0.01f;
		b->targetR = 1.0f;
	}

	b->swap = swap ? 1 : 0;

	if (monoPct < 0.0f) monoPct = 0.0f;
	if (monoPct > 100.0f) monoPct = 100.0f;
	b->mono = monoPct;
	b->targetMono = monoPct * 0.01f;

	balanceDesign(b, fs);

	b->transparent = fabsf(balancePct) < 1e-6f && !b->swap && monoPct <= 0.0f;
	jdsp_unlock(jdsp);
}

void BalanceProcess(JamesDSPLib *jdsp, size_t n)
{
	Balance *b = &jdsp->balance;
	if (b->transparent)
		return;

	float *left = jdsp->tmpBuffer[0];
	float *right = jdsp->tmpBuffer[1];
	const float c = b->smooth;

	for (size_t i = 0; i < n; i++)
	{
		float l = left[i], r = right[i];

		/* Swap first, so the balance control always means the ear the listener
		   actually hears it in rather than the channel it arrived on. */
		if (b->swap) { const float t = l; l = r; r = t; }

		b->gainL = b->targetL + c * (b->gainL - b->targetL);
		b->gainR = b->targetR + c * (b->gainR - b->targetR);
		b->monoAmt = b->targetMono + c * (b->monoAmt - b->targetMono);

		if (b->monoAmt > 0.0f)
		{
			const float mid = (l + r) * 0.5f;
			l = l + (mid - l) * b->monoAmt;
			r = r + (mid - r) * b->monoAmt;
		}

		left[i] = l * b->gainL;
		right[i] = r * b->gainR;
	}
}

void BalanceEnable(JamesDSPLib *jdsp)
{
	jdsp_lock(jdsp);
	if (jdsp->balanceEnabled)
	{
		jdsp_unlock(jdsp);
		return;
	}
	Balance *b = &jdsp->balance;
	b->fs = jdsp->fs > 0.0f ? jdsp->fs : 48000.0f;
	/* Start at the target rather than at zero, or switching the card on would
	   fade the volume up from silence over the smoothing time. */
	b->gainL = b->targetL;
	b->gainR = b->targetR;
	b->monoAmt = b->targetMono;
	jdsp->balanceEnabled = 1;
	jdsp_unlock(jdsp);
}

void BalanceDisable(JamesDSPLib *jdsp)
{
	jdsp_lock(jdsp);
	jdsp->balanceEnabled = 0;
	jdsp_unlock(jdsp);
}
