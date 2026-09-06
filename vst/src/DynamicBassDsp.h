// ViPER Dynamic System (dynamic bass) — ported from RootlessViPER4Android
// (jdsp/Effects/vdynbass.c), itself a port of ViPER's DynamicBass.
//
// Two quite different paths, chosen by the lower crossover:
//
//  - At or below 120 Hz it is a simple mono-sum bass lift: lowpass L+R and add
//    it back to both channels. Bass is largely mono anyway, and summing avoids
//    the phase smearing a per-channel filter would introduce down there.
//
//  - Above 120 Hz it runs the four-pole ladder network ViPER uses, splitting
//    each channel into three bands (below, above, and between the two corner
//    frequencies) and recombining them with independent side gains. That is
//    what gives the effect its dynamic character rather than a static shelf.
#pragma once
#include <cmath>
#include <cstring>

class DynamicBassDsp
{
public:
    void setSampleRate(float fs) { mFs = (fs < 8000.0f) ? 48000.0f : fs; apply(); }

    void setParams(float gainPct, float x1, float x2, float y1, float y2,
                   float sideGainXPct, float sideGainYPct)
    {
        mGainPct = gainPct; mX1 = x1; mX2 = x2; mY1 = y1; mY2 = y2;
        mSgx = sideGainXPct; mSgy = sideGainYPct;
        apply();
    }

    void reset()
    {
        mXL = mXR = mYL = mYR = Poles{};
        mLpX1 = mLpX2 = mLpY1 = mLpY2 = 0.0f;
        // Angles are configuration, not state, so restore them after clearing
        apply();
    }

    void processBlock(float* l, float* r, int n)
    {
        if (mLowFreqX <= 120.0f)
        {
            for (int i = 0; i < n; i++)
            {
                float avg = lowpass(l[i] + r[i]);
                l[i] += avg;
                r[i] += avg;
            }
        }
        else
        {
            for (int i = 0; i < n; i++)
            {
                poles(mXL, l[i]);
                poles(mXR, r[i]);
                poles(mYL, mBassGain * mXL.out0);
                poles(mYR, mBassGain * mXR.out0);
                l[i] = mXL.out1 + mYL.out2 + mSideX * mYL.out1 + mSideY * mYL.out0 + mXL.out2;
                r[i] = mXR.out1 + mYR.out2 + mSideX * mYR.out1 + mSideY * mYR.out0 + mXR.out2;
            }
        }
    }

private:
    struct Poles {
        float lowerAngle = 0, upperAngle = 0;
        float in0 = 0, in1 = 0, in2 = 0;
        float x0 = 0, x1 = 0, x2 = 0, x3 = 0;
        float y0 = 0, y1 = 0, y2 = 0, y3 = 0;
        float out0 = 0, out1 = 0, out2 = 0;
    };

    void apply()
    {
        mBassGain = (mGainPct * 20.0f + 100.0f) * 0.01f;
        mQPeak = (mBassGain - 1.0f) / 20.0f * 1600.0f;
        if (mQPeak > 1600.0f) mQPeak = 1600.0f;
        mSideX = mSgx * 0.01f;
        mSideY = mSgy * 0.01f;
        mLowFreqX = mX1;

        setPoles(mXL, mX1, mX2); setPoles(mXR, mX1, mX2);
        setPoles(mYL, mY1, mY2); setPoles(mYR, mY1, mY2);
        setLowpass(55.0f, mQPeak / 666.0f + 0.5f);
    }

    void setPoles(Poles& p, float lower, float upper)
    {
        p.lowerAngle = lower * 3.14159265358979f / mFs;
        p.upperAngle = upper * 3.14159265358979f / mFs;
    }

    static void poles(Poles& p, float s)
    {
        float oldest = p.in2;
        p.in2 = p.in1; p.in1 = p.in0; p.in0 = s;
        p.x0 += p.lowerAngle * (s - p.x0);
        p.x1 += p.lowerAngle * (p.x0 - p.x1);
        p.x2 += p.lowerAngle * (p.x1 - p.x2);
        p.x3 += p.lowerAngle * (p.x2 - p.x3);
        p.y0 += p.upperAngle * (s - p.y0);
        p.y1 += p.upperAngle * (p.y0 - p.y1);
        p.y2 += p.upperAngle * (p.y1 - p.y2);
        p.y3 += p.upperAngle * (p.y2 - p.y3);
        p.out0 = p.x3;
        p.out1 = oldest - p.y3;
        p.out2 = p.y3 - p.x3;
    }

    void setLowpass(float freq, float q)
    {
        float x = (freq * 2.0f * 3.14159265358979f) / mFs;
        float sinX = std::sin(x), cosX = std::cos(x);
        float y = sinX / (q * 2.0f);
        float z = (1.0f - cosX) * 0.5f;
        float a0 = y + 1.0f;
        mLpB0 = z / a0; mLpB1 = (1.0f - cosX) / a0; mLpB2 = z / a0;
        mLpA1 = (cosX * -2.0f) / a0; mLpA2 = (1.0f - y) / a0;
        mLpX1 = mLpX2 = mLpY1 = mLpY2 = 0.0f;
    }

    float lowpass(float s)
    {
        float out = s*mLpB0 + mLpX1*mLpB1 + mLpX2*mLpB2 - mLpY1*mLpA1 - mLpY2*mLpA2;
        mLpY2 = mLpY1; mLpY1 = out;
        mLpX2 = mLpX1; mLpX1 = s;
        return out;
    }

    float mFs = 48000.0f;
    float mGainPct = 33, mX1 = 1000, mX2 = 6200, mY1 = 50, mY2 = 90, mSgx = 30, mSgy = 10;
    float mBassGain = 1, mQPeak = 0, mSideX = 0.3f, mSideY = 0.1f, mLowFreqX = 1000;
    float mLpB0 = 0, mLpB1 = 0, mLpB2 = 0, mLpA1 = 0, mLpA2 = 0;
    float mLpX1 = 0, mLpX2 = 0, mLpY1 = 0, mLpY2 = 0;
    Poles mXL, mXR, mYL, mYR;
};
