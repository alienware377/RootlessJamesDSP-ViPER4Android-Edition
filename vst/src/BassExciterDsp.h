// Psychoacoustic bass exciter — ported verbatim from RootlessViPER4Android
// (jdsp/Effects/bassex.c) so the plugin and the Android app sound identical.
//
// The idea: lowpass to isolate the sub content, rectify it (which generates
// harmonics at multiples of the fundamental), drive and soft-clip that, then
// bandpass around 2.5x the cutoff to keep only the harmonics the ear uses to
// infer a bass note. Small speakers cannot reproduce the fundamental, but the
// harmonics imply it, so the bass is *heard* without needing the low end.
#pragma once
#include <cmath>
#include <cstring>

class BassExciterDsp
{
public:
    void setSampleRate(float fs)
    {
        mFs = (fs < 8000.0f) ? 48000.0f : fs;
        updateBand1();
        updateBand2();
    }

    void setBand1(float cutoffHz, float intensity, float mixPct)
    {
        mCutoff = clamp(cutoffHz, 40.0f, 200.0f);
        mDrive  = 1.0f + intensity * 0.07f;
        mMix    = mixPct * 0.01f;
        updateBand1();
    }

    void setBand2(bool on, float cutoffHz, float intensity, float mixPct)
    {
        mBand2On = on;
        mCutoff2 = clamp(cutoffHz, 30.0f, 200.0f);
        mDrive2  = 1.0f + intensity * 0.07f;
        mMix2    = mixPct * 0.01f;
        updateBand2();
    }

    void reset()
    {
        std::memset(mLpZ,  0, sizeof(mLpZ));
        std::memset(mBpZ,  0, sizeof(mBpZ));
        std::memset(mLp2Z, 0, sizeof(mLp2Z));
        std::memset(mBp2Z, 0, sizeof(mBp2Z));
        mDc[0] = mDc[1] = mDc2[0] = mDc2[1] = 0.0f;
    }

    // ch must be 0 or 1: the exciter keeps per-channel filter state
    float processSample(float x, int ch)
    {
        float sub  = biquad(mLp, mLpZ[ch], x);
        float h    = std::fabs(sub) * mDrive;
        h -= mDc[ch];
        mDc[ch] += h * 0.0005f;              // slow DC tracker, keeps it centred
        h = h / (1.0f + std::fabs(h));       // soft clip
        float out = x + mMix * 2.0f * biquad(mBp, mBpZ[ch], h);

        if (mBand2On)
        {
            float sub2 = biquad(mLp2, mLp2Z[ch], x);
            float h2   = std::fabs(sub2) * mDrive2;
            h2 -= mDc2[ch];
            mDc2[ch] += h2 * 0.0005f;
            h2 = h2 / (1.0f + std::fabs(h2));
            out += mMix2 * 2.0f * biquad(mBp2, mBp2Z[ch], h2);
        }
        return out;
    }

private:
    static float clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    static void design(float* c, float fs, float f0, float q, bool bandpass)
    {
        float w0 = 2.0f * 3.14159265358979f * f0 / fs;
        float cw = std::cos(w0), sw = std::sin(w0);
        float alpha = sw / (2.0f * q);
        float a0 = 1.0f + alpha;
        if (bandpass) { c[0] = alpha / a0; c[1] = 0.0f; c[2] = -alpha / a0; }
        else          { c[0] = ((1.0f - cw) * 0.5f) / a0; c[1] = (1.0f - cw) / a0; c[2] = c[0]; }
        c[3] = (-2.0f * cw) / a0;
        c[4] = (1.0f - alpha) / a0;
    }

    static float biquad(const float* c, float* z, float x)
    {
        float y = c[0]*x + c[1]*z[0] + c[2]*z[1] - c[3]*z[2] - c[4]*z[3];
        z[1] = z[0]; z[0] = x;
        z[3] = z[2]; z[2] = y;
        return y;
    }

    // The bandpass sits at 2.5x the cutoff: that is where the generated
    // harmonics live, and passing only those is what keeps the effect from
    // simply sounding like distortion on the bass.
    void updateBand1() { design(mLp,  mFs, mCutoff,  0.7071f, false);
                         design(mBp,  mFs, mCutoff  * 2.5f, 0.8f, true); }
    void updateBand2() { design(mLp2, mFs, mCutoff2, 0.7071f, false);
                         design(mBp2, mFs, mCutoff2 * 2.5f, 0.8f, true); }

    float mFs = 48000.0f;
    float mCutoff = 100.0f, mDrive = 1.0f + 40.0f * 0.07f, mMix = 0.5f;
    float mCutoff2 = 60.0f, mDrive2 = 1.0f + 40.0f * 0.07f, mMix2 = 0.4f;
    bool  mBand2On = false;
    float mLp[5]{}, mBp[5]{}, mLp2[5]{}, mBp2[5]{};
    float mLpZ[2][4]{}, mBpZ[2][4]{}, mLp2Z[2][4]{}, mBp2Z[2][4]{};
    float mDc[2]{}, mDc2[2]{};
};
