#ifndef _EEL_JDSP_HD_H_
#define _EEL_JDSP_HD_H_
#include <stdint.h>
#include "../cpthread.h"
#include "generalDSP/interpolation.h"
#include "Effects/eel2/numericSys/libsamplerate/samplerate.h"
#include "generalDSP/TwoStageFFTConvolver.h"
#include "generalDSP/digitalFilters.h"
#include "Effects/eel2/numericSys/FilterDesign/fdesign.h"
#include "Effects/eel2/eelCommon.h"
#include "generalDSP/ArbFIRGen.h"
#define MAX_BENCHMARK (10)
// Misc
extern double mapVal(double x, double in_min, double in_max, double out_min, double out_max);
extern double mag2dB(double lin);
extern float db2magf(double dB);
extern double db2mag(double dB);
extern void linspace(double *x, int n, double a, double b);
extern void channel_joinFloat(float **chan_buffers, unsigned int num_channels, float *buffer, unsigned int num_frames);
extern void channel_join(double **chan_buffers, unsigned int num_channels, double *buffer, unsigned int num_frames);
extern void channel_split(double *buffer, unsigned int num_frames, double **chan_buffers, unsigned int num_channels);
extern void channel_splitFloat(float *buffer, unsigned int num_frames, float **chan_buffers, unsigned int num_channels);
extern void normalise(float *buffer, int num_samps);
extern unsigned int crc32c(const unsigned char *buf, size_t len);
extern int upper_bound(double *a, int n, double x);
extern int lower_bound(double *a, int n, double x);
extern size_t fast_upper_bound(double *a, size_t n, double x);
extern size_t fast_lower_bound(double *a, size_t n, double x);
extern void fhtsinHalfTblFloat(float *dst, unsigned int n);
extern void LLdiscreteHartleyFloat(float *A, const int nPoints, const float *sinTab);
extern double randXorshift(uint64_t s[2]);
// Misc end
typedef struct
{
	float threshold;
	float relCoef;
	float envOverThreshold;
	int mode; // 0 = peak limiter, 1 = soft saturator, 2 = off, 3 = ViPER classic
	/* ViPER classic (mode 3): faithful re-creation of V4A's SoftwareLimiter -
	   a 256-sample lookahead ring per channel, a tournament tree holding the
	   window maximum, a ~10-sample smoothed attack and a final hard clamp. */
	float vLook[2][256];
	float vTree[512];
	int vIdx;
	float vAtt, vGain;
} JLimiter;
typedef struct
{
	float lp[5];
	float bp[5];
	float lpz[2][4];
	float bpz[2][4];
	float dcState[2];
	float drive;
	float mix;
	float lp2[5];
	float bp2[5];
	float lp2z[2][4];
	float bp2z[2][4];
	float dc2State[2];
	float drive2;
	float mix2;
	int band2;
} BassExciter;
typedef struct
{
	float pre[5];
	float post[5];
	float prez[2][4];
	float postz[2][4];
	float dcState[2];
	float strength;
} SpectrumExtension;
typedef struct
{
	float lowerAngle, upperAngle;
	float in0, in1, in2;
	float x0, x1, x2, x3;
	float y0, y1, y2, y3;
	float out0, out1, out2;
} VPolesFilter;
typedef struct
{
	VPolesFilter fXL, fXR, fYL, fYR;
	float lpB0, lpB1, lpB2, lpA1, lpA2;
	float lpX1, lpX2, lpY1, lpY2;
	float bassGain, qPeak, sideGainX, sideGainY, lowFreqX;
} VDynamicBass;
typedef struct
{
	float *bufL, *bufR;
	int widx;
	float delayL, delayR;
} DiffSurround;
typedef struct
{
	int mode;
	float sharp;
	float prev[2];
	float shelf[5];
	float shelfZ[2][4];
} ViperClarity;
typedef struct
{
	float sideGain, midGain;
} FieldSurround;
#define ECHO_BUFLEN 131072
#define ECHO_APLEN 2048
typedef struct
{
	float *bufL, *bufR;
	float apL[ECHO_APLEN], apR[ECHO_APLEN];
	int widx, diffDelay;
	float fs;
	float inputLevel;
	float delaySamples, targetDelay, smoothCoeff, offsetSamples;
	int keepPitch;
	float xfade, xfadePos;
	int model, filterType;
	float stereoSpread, feedback;
	float cutoffHz, svfK, svfA1, svfA2, svfA3, svfIc1[2], svfIc2[2];
	float srStep, srPhase, srHoldL, srHoldR, bitLevels;
	float modRate, modInc, modPhase, modTime, modCutoff;
	float diffusion, spread;
	int distMode;
	float distLevel, knee, symmetry;
	float tone, toneCoeff, toneZL, toneZR;
	float wet, dry;
} EchoDelay;
// Slots for biquad sections, not for handles on the graph: a cutoff expands
// into one section per 12dB of slope, so a single 96dB/octave low-pass is eight
// of these on its own.
#define MBD_MAX_BANDS 24
#define MBD_MAX_CUTOFF_STAGES 8
#define MBD_CHORUS_BUFLEN 8192
#define MBD_CHORUS_VOICES 4
#define MBD_OS_MAX 8
enum MbdModel
{
	MBD_MODEL_SOFT = 0,
	MBD_MODEL_HARD,
	MBD_MODEL_TUBE,
	MBD_MODEL_OVERDRIVE,
	MBD_MODEL_FOLD,
	MBD_MODEL_FUZZ,
	MBD_MODEL_RECTIFY,
	MBD_MODEL_CRUSH,
	MBD_MODEL_COUNT
};
enum MbdRouting
{
	MBD_ROUTING_SPLIT = 0,
	MBD_ROUTING_PARALLEL
};
// Mirrors ParametricEqFilterType.code on the app side; the two must not drift.
enum MbdFilterType
{
	MBD_FILTER_PEAKING = 0,
	MBD_FILTER_LOW_SHELF,
	MBD_FILTER_HIGH_SHELF,
	MBD_FILTER_LOW_PASS,
	MBD_FILTER_HIGH_PASS
};
typedef struct
{
	// Band-select filter: a biquad cascade built from the parametric band list
	// the editor produces, with per-channel state.
	int numBands;
	float b0[MBD_MAX_BANDS], b1[MBD_MAX_BANDS], b2[MBD_MAX_BANDS];
	float a1[MBD_MAX_BANDS], a2[MBD_MAX_BANDS];
	float z1[2][MBD_MAX_BANDS], z2[2][MBD_MAX_BANDS];
	// Distortion
	int routing, model, transparent;
	float drive, shaperBlend, bias, shape, bitStep, holdLen;
	float tilt, bandGain, mix;
	float toneA, toneZ[2];
	float dcR, dcX[2], dcY[2];
	float shHold[2], shPhase[2];
	int osFactor;
	samplerateTool smpUp[2], smpDown[2];
	// Chorus on the distorted band; buffers held only while enabled.
	float *chBufL, *chBufR;
	int chPos[2], chVoices;
	float chPhase, chInc, chBase, chDepth, chFeedback, chSpread, chMix;
	float fs;
} MultibandDist;
typedef struct
{
	float b0, b1, b2, a1, a2;
	float z1[2], z2[2];
} LowEndStage;
typedef struct
{
	// Two sections make the subsonic slope fourth order; the other two are the
	// body shelf and the wide dip where recordings turn thick.
	LowEndStage sub1, sub2, weight, mud;
	float subsonicHz, weightHz, weightDb, mudHz, mudDb;
	float fs, mix;
	int transparent;
} LowEnd;
#define TRANSIENT_BANDS 3
typedef struct
{
	float b0, b1, b2, a1, a2;
	float z1[2], z2[2];
} TransientStage;
typedef struct
{
	// Two follower pairs. The gap inside the first says how hard a note is
	// starting; the gap inside the second says how long it is taking to die.
	float envAtkFast, envAtkSlow, envSusFast, envSusSlow;
	float atkFastC, atkSlowC, atkRelC, susAttC, susFastC, susSlowC;
	float attack, sustain, gainDb;
} TransientBand;
typedef struct
{
	// Two running lowpasses; the bands are their differences and the
	// remainder, so the three always sum back to the input.
	TransientStage split[TRANSIENT_BANDS - 1];
	TransientBand band[TRANSIENT_BANDS];
	float freqLow, freqHigh, rangeDb, fs, mix;
	int transparent;
} Transient;
typedef struct
{
	// One channel of state: everything here runs on the side signal alone.
	float b0, b1, b2, a1, a2;
	float z1, z2;
} ImagingStage;
typedef struct
{
	// Width is the gain of the side signal, so these equalise side and leave
	// mid alone. At every width 1.0 each stage is the identity and the output
	// is the input, bitwise.
	ImagingStage mono, mono2, low, mid, high;
	float monoBelow, freqLow, freqMid, freqHigh;
	float widthLow, widthMid, widthHigh;
	float fs, mix;
	int transparent;
} Imaging;
#define DYNEQ_MAX_BANDS 8
// freq, Q, threshold dB, ratio, attack ms, release ms, range dB, mode
#define DYNEQ_VALUES_PER_BAND 8
enum DynEqMode
{
	DYNEQ_MODE_COMPRESS = 0,	// act on what rises above the threshold
	DYNEQ_MODE_EXPAND			// act on what falls below it
};
typedef struct
{
	float freq, q, thresholdDb, ratio, attackMs, releaseMs, rangeDb;
	int mode;
	// Sidechain bandpass: what this band listens to.
	float sb0, sb1, sb2, sa1, sa2;
	float sz1[2], sz2[2];
	// Peaking filter: what the listener hears. Redesigned as the gain moves.
	float b0, b1, b2, a1, a2;
	float z1[2], z2[2];
	float env, gainDb, appliedDb, attC, relC;
} DynEqBand;
typedef struct
{
	int numBands;
	DynEqBand band[DYNEQ_MAX_BANDS];
	float fs, mix;
	int redesign;
} DynamicEq;
// 1024 samples is 5.3ms at 48kHz and still 5.3ms of headroom at 192kHz, where
// the longest lookahead any mode asks for is 768 samples.
#define MAXR_BUFLEN 1024
#define MAXR_OS_MAX 8
// Which curve the character control uses to round peaks. Smooth is the shape
// this shipped with, so it must stay index 0 and stay bit-identical.
enum MaxrClip
{
	// Ordered by how early the curve starts bending, which is what actually
	// separates them. The algebraic one bends from the origin and so lifts
	// everything under the peaks - measured about 1.2dB denser than hard on
	// music. The hard one stays linear until two thirds and only rounds the
	// very top. A saturator at one end, a clipper at the other.
	MAXR_CLIP_SMOOTH = 0,	// bends immediately, adds the most density
	MAXR_CLIP_TANH,			// classic, in between
	MAXR_CLIP_HARD,			// linear until late, touches peaks only
	MAXR_CLIP_COUNT
};
enum MaxrMode
{
	MAXR_MODE_TRANSPARENT = 0,
	MAXR_MODE_PUNCHY,
	MAXR_MODE_WARM,
	MAXR_MODE_AGGRESSIVE,
	MAXR_MODE_COUNT
};
typedef struct
{
	int mode, truePeak, osRequest, osFactor, lookahead, clipShape;
	float inGain, ceiling, character, transient, stereoLink;
	float attCoef, relCoef, gainState[2];
	// Delay line, plus the monotonic deque that gives the sliding-window peak.
	float *buf[2];
	unsigned *dq[2];
	float *dqVal[2];
	unsigned dqHead[2], dqTail[2], widx;
	samplerateTool smp[2];
	float fs;
} Maximizer;
typedef struct
{
	float *buf[2];
	int w;
	float phasor;
	float rate, mix;
	int win, bypass;
} PitchShift;
#define VREV_COMBLEN 8192
#define VREV_APLEN 2048
typedef struct
{
	float thrLin, slope, attC, relC, makeup, env;
} FetComp;
typedef struct
{
	float lpCoef, feed, norm, lpzL, lpzR;
} Cure;
typedef struct
{
	int mode;
	float f[5], fz[2][4];
	float lp[5], lpz[2][4];
	float dc[2];
	float harm;
} ViperBass;
#define VREV_PREMAX 96000
#define VREV_DIFF_N 4
#define VREV_TANK_N 6
#define VREV_FDN_N 8
typedef struct
{
	/* Classic (freeverb-lineage) network - the original ViPER model */
	float *combMem;
	float *comb[2][4];
	float *ap[2][2];
	int clen[2][4], alen[2][2];
	int cidx[2][4], aidx[2][2];
	float cstore[2][4];
	float cflt[2][4];
	float astore[2][2];
	float fb, damp, wet1, wet2, dry;

	/* Shared */
	int model;
	float fs, roomSize, dampAmt, width, wetAmt;
	float decay, diffusionAmt, modDepth, bassMult, erLevel;
	float *preMem;
	int preLen, preIdx;

	/* Plate: Dattorro figure-eight tank (MVerb lineage) */
	float *datMem;
	float *diff[VREV_DIFF_N];
	int diffLen[VREV_DIFF_N], diffIdx[VREV_DIFF_N];
	float *tank[VREV_TANK_N];
	int tankLen[VREV_TANK_N], tankIdx[VREV_TANK_N];
	float tankLp[2], modPhase;

	/* Hall / Room: feedback delay network (zita-rev1 lineage) */
	float *fdnMem;
	float *fdn[VREV_FDN_N];
	int fdnLen[VREV_FDN_N], fdnIdx[VREV_FDN_N];
	float fdnLp[VREV_FDN_N], fdnBass[VREV_FDN_N];
	float fdnGainHi[VREV_FDN_N], fdnGainLo[VREV_FDN_N];
	float *erMem;
	int erLen, erIdx;
} VReverb;
typedef struct
{
	float hp[5], hpz[2][4];
	float pk[5], pkz[2][4];
	float sh[5], shz[2][4];
	float mix;
	float air[5];
	float airz[2][4];
} SpeakerOpt;
typedef struct
{
	float *bufL, *bufR;
	int widx;
	float cross, room, width, fb, norm;
	int dCross, dR1, dR2, dR3;
	float lpCoef, lpzL, lpzR;
	float dampCoef, fbzL, fbzR;
} HpSurround;
typedef struct
{
	float target, maxGain;
	float envCoef, gainCoef;
	float env, gain;
} Agc;
#define FFTSIZE_DRS (8192)
#define ANALYSIS_OVERLAP_DRS_MAX (8)
#define HALFWNDLEN_DRS ((FFTSIZE_DRS >> 1) + 1)
#define MAX_OUTPUT_BUFFERS_DRS 2
#define NUMPTS_DRS (7)
#define DYN_BANDS_GAMMATONE (16)
#define MAXORDER (12)
#define MAXSECTIONS (MAXORDER >> 1)
// number of points of pre and post padding used to set initial conditions
#define PREPAD (200) // Max pad
#define POSPAD (200) // Max pad
typedef struct
{
	float real, imag;
} cplx;
typedef struct
{
	double real, imag;
} cplxDouble;
typedef struct str_dynfreqdomain
{
	// Frequency domain
	float analysisWnd[FFTSIZE_DRS];
	unsigned int fftLen, minus_fftLen, ovpLen, ovpCount, halfLen, smpShift, procUpTo;
	void(*fft)(float*, const float*);
	unsigned int mBitRev[FFTSIZE_DRS];
	float 	mSineTab[FFTSIZE_DRS >> 1];
	float synthesisWnd[FFTSIZE_DRS];
	// Shared variable between all FFT length and modes and channel config
	int  mOutputReadSampleOffset;
	int  mOutputBufferCount; // How many buffers are actually in use
	unsigned int mInputSamplesNeeded;
	float 	*mOutputBuffer[MAX_OUTPUT_BUFFERS_DRS];
	float buffer[MAX_OUTPUT_BUFFERS_DRS][FFTSIZE_DRS >> 1];
	unsigned int mInputPos;
	float 	mInput[2][FFTSIZE_DRS];
	float mOverlapStage2dash[2][ANALYSIS_OVERLAP_DRS_MAX][FFTSIZE_DRS >> 1];
	float 	mTempLBuffer[FFTSIZE_DRS];
	float 	mTempRBuffer[FFTSIZE_DRS];
	float timeDomainOut[2][FFTSIZE_DRS];
	float mag[HALFWNDLEN_DRS];
	float aheight[HALFWNDLEN_DRS];
	char noGridDownsampling;
	unsigned int smallGridSize;
	char octaveSmooth[sizeof(unsigned int) + sizeof(float) + sizeof(unsigned int) + ((HALFWNDLEN_DRS + 1) << 1) * sizeof(unsigned int) + (HALFWNDLEN_DRS + 1) * sizeof(float) + ((HALFWNDLEN_DRS + 1) + 3) * 2 * sizeof(float)];
	float finalGain[HALFWNDLEN_DRS];
	double freq2[NUMPTS_DRS + 2];
	float freq3[DYN_BANDS_GAMMATONE + 2];
	double gains2[NUMPTS_DRS + 2];
	float DREmultUniform[HALFWNDLEN_DRS];
	float DREmult[HALFWNDLEN_DRS];
	float oldBuf[HALFWNDLEN_DRS];
	ierper pch;
	float headRoomdB;
	float fgt_fac, fgt_facT;
	float spectralRate;
	// Time domain
	float bRe[DYN_BANDS_GAMMATONE];
	float aRe[DYN_BANDS_GAMMATONE];
	float aIm[DYN_BANDS_GAMMATONE];
	float Zre[DYN_BANDS_GAMMATONE * 2];
	float Zim[DYN_BANDS_GAMMATONE * 2];
	float gmtFreq[DYN_BANDS_GAMMATONE];
	float interpolatedGain[DYN_BANDS_GAMMATONE + 2];
	float diffGain[DYN_BANDS_GAMMATONE];
	float c1[(DYN_BANDS_GAMMATONE - 1)];
	float c2[(DYN_BANDS_GAMMATONE - 1)];
	float d0[(DYN_BANDS_GAMMATONE - 1)];
	float d1[(DYN_BANDS_GAMMATONE - 1)];
	float overallGain;
	float c1step[(DYN_BANDS_GAMMATONE - 1)];
	float c2step[(DYN_BANDS_GAMMATONE - 1)];
	float d0step[(DYN_BANDS_GAMMATONE - 1)];
	float d1step[(DYN_BANDS_GAMMATONE - 1)];
	float overallGainstep;
	float z1_AL[(DYN_BANDS_GAMMATONE - 1)];
	float z2_AL[(DYN_BANDS_GAMMATONE - 1)];
	float z1_AR[(DYN_BANDS_GAMMATONE - 1)];
	float z2_AR[(DYN_BANDS_GAMMATONE - 1)];
	double trigo[(DYN_BANDS_GAMMATONE - 1) * 4];
	unsigned int updatePerNSmps;
	float dsSm, alpha;
	int updateIdx;
	// Global variable
	int granularity, tfresolution;
	// CWT
	unsigned int reqSynthesisWnd;
	float gauss_c1[PREPAD + HALFWNDLEN_DRS + POSPAD - 1], gauss_c2[PREPAD + HALFWNDLEN_DRS + POSPAD - 1], gauss_b[PREPAD + HALFWNDLEN_DRS + POSPAD - 1];
	unsigned int prepad, pospad;
	unsigned int bitrevfftshift[FFTSIZE_DRS];
	float corrF[HALFWNDLEN_DRS];
	void (*process)(struct str_dynfreqdomain *);
	float shiftCentre[HALFWNDLEN_DRS];
	float scalarGain;
	float fftBuf[2][FFTSIZE_DRS], getbackCorrectedToSpectrum1[2][FFTSIZE_DRS];
	float real[2][HALFWNDLEN_DRS], imag[2][HALFWNDLEN_DRS];
	float specHannReal[2][PREPAD + HALFWNDLEN_DRS + POSPAD - 1], specHannImag[2][PREPAD + HALFWNDLEN_DRS + POSPAD - 1];
	cplx tmp[2][PREPAD + HALFWNDLEN_DRS + POSPAD - 1];
} FFTCompander;
typedef struct
{
	int needOversample;
	samplerateTool smp[2];
	SixBandsCrossover subband[2];
	float pregain, postgain;
} VacuumTube;
typedef struct
{
	float inputs[1024];
	int inPoint;
	int outPoint;
	int allocateLen;
} integerDelayLine;
typedef struct
{
	int filterType;
	float gCoeff; // gain element 
	float RCoeff; // feedback damping element
	float KCoeff; // shelf gain element
	float precomputeCoeff1, precomputeCoeff2, precomputeCoeff3, precomputeCoeff4;
	float z1_A, z2_A; // state variables (z^-1)
} StateVariable2ndOrder;
typedef struct
{
	float maxGain;
	float originalBuf[960];
	int downsamplerPos;
	samplerateTool downsampler;
	float delayLine[16];
	float fftBuf[16];
	float smoothFFTBuffer[9];
	float freq[9];
	float maxSmoothingFactor, minusmaxSmoothingFactor;
	float smoothMaxFreq;
	float boostdB;
	double fs;
	float gainSmoothingFactor, minusgainSmoothingFactor;
	StateVariable2ndOrder svf[2];
	integerDelayLine dL[2];
} DBB;
//   sf_reverb_state_st rv;
//   sf_presetreverb(&rv, 44100, SF_REVERB_PRESET_DEFAULT);
//
//   for each abitrary length sample:
//   float outputL, outputR;
//   sf_reverb_process(&rv, inputL, inputR, &outputL, &outputR);
#define SF_REVERB_DS        3000
typedef struct
{
	int pos;                 // current write position
	int size;                // delay size
	float buf[SF_REVERB_DS]; // delay buffer
} sf_rv_delay_st;
// 1st order IIR filter
typedef struct
{
	float a2; // coefficients
	float b1;
	float b2;
	float y1; // state
} sf_rv_iir1_st;
// biquad
// note: we don't use biquad.c because we want to step through the sound one sample at a time, one
//       channel at a time
typedef struct
{
	float b0; // biquad coefficients
	float b1;
	float b2;
	float a1;
	float a2;
	float xn1; // input[n - 1]
	float xn2; // input[n - 2]
	float yn1; // output[n - 1]
	float yn2; // output[n - 2]
} sf_rv_biquad_st;
// early reflection
typedef struct
{
	int             delaytblL[18], delaytblR[18];
	sf_rv_delay_st  delayPWL, delayPWR;
	sf_rv_delay_st  delayRL, delayLR;
	sf_rv_biquad_st allpassXL, allpassXR;
	sf_rv_biquad_st allpassL, allpassR;
	sf_rv_iir1_st   lpfL, lpfR;
	sf_rv_iir1_st   hpfL, hpfR;
	float wet1, wet2;
} sf_rv_earlyref_st;
// oversampling
// maximum oversampling factor
#define SF_REVERB_OF        2
typedef struct
{
	int factor;           // oversampling factor [1 to SF_REVERB_OF]
	sf_rv_biquad_st lpfU; // lowpass filter used for upsampling
	sf_rv_biquad_st lpfD; // lowpass filter used for downsampling
} sf_rv_oversample_st;
// dc cut
typedef struct
{
	float gain;
	float y1;
	float y2;
} sf_rv_dccut_st;
// fractal noise cache
// noise buffer size; must be a power of 2 because it's generated via fractal generator
#define SF_REVERB_NS        (1<<11)
typedef struct
{
	int pos;                 // current read position in the buffer
	float buf[SF_REVERB_NS]; // buffer filled with noise
} sf_rv_noise_st;
// low-frequency oscilator (LFO)
typedef struct
{
	float re;  // real part
	float im;  // imaginary part
	float sn;  // sin of angle increment per sample
	float co;  // cos of angle increment per sample
	int count; // number of samples generated so far (used to apply small corrections over time)
} sf_rv_lfo_st;
// all-pass filter
// maximum size
#define SF_REVERB_APS       3400
typedef struct
{
	int pos;
	int size;
	float feedback;
	float decay;
	float buf[SF_REVERB_APS];
} sf_rv_allpass_st;
// 2nd order all-pass filter
// maximum sizes of the two buffers
#define SF_REVERB_AP2S1     4200
#define SF_REVERB_AP2S2     3000
typedef struct
{
	//    line 1                 line 2
	int   pos1, pos2;
	int   size1, size2;
	float feedback1, feedback2;
	float decay1, decay2;
	float buf1[SF_REVERB_AP2S1], buf2[SF_REVERB_AP2S2];
} sf_rv_allpass2_st;
// 3rd order all-pass filter with modulation
// maximum sizes of the three buffers and maximum mod size of the first line
#define SF_REVERB_AP3S1     4000
#define SF_REVERB_AP3M1     600
#define SF_REVERB_AP3S2     2000
#define SF_REVERB_AP3S3     3000
typedef struct
{
	//    line 1 (with modulation)                 line 2                 line 3
	int   rpos1, wpos1, pos2, pos3;
	int   size1, msize1, size2, size3;
	float feedback1, feedback2, feedback3;
	float decay1, decay2, decay3;
	float buf1[SF_REVERB_AP3S1 + SF_REVERB_AP3M1], buf2[SF_REVERB_AP3S2], buf3[SF_REVERB_AP3S3];
} sf_rv_allpass3_st;
// modulated all-pass filter
// maximum size and maximum mod size
#define SF_REVERB_APMS      3600
#define SF_REVERB_APMM      137
typedef struct
{
	int rpos, wpos;
	int size, msize;
	float feedback;
	float decay;
	float z1;
	float buf[SF_REVERB_APMS + SF_REVERB_APMM];
} sf_rv_allpassm_st;
// comb filter
// maximum size of the buffer
#define SF_REVERB_CS        1500
typedef struct
{
	int pos;
	int size;
	float buf[SF_REVERB_CS];
} sf_rv_comb_st;
//
// the final reverb state structure
//
// note: this struct is about 1Mb
typedef struct
{
	sf_rv_earlyref_st   earlyref;
	sf_rv_oversample_st oversampleL, oversampleR;
	sf_rv_dccut_st      dccutL, dccutR;
	sf_rv_noise_st      noise;
	sf_rv_lfo_st        lfo1;
	sf_rv_iir1_st       lfo1_lpf;
	sf_rv_allpassm_st   diffL[10], diffR[10];
	sf_rv_allpass_st    crossL[4], crossR[4];
	sf_rv_iir1_st       clpfL, clpfR; // cross LPF
	sf_rv_delay_st      cdelayL, cdelayR; // cross delay
	sf_rv_biquad_st     bassapL, bassapR; // bass all-pass
	sf_rv_biquad_st     basslpL, basslpR; // bass lowpass
	sf_rv_iir1_st       damplpL, damplpR; // dampening lowpass
	sf_rv_allpassm_st   dampap1L, dampap1R; // dampening all-pass (1)
	sf_rv_delay_st      dampdL, dampdR; // dampening delay
	sf_rv_allpassm_st   dampap2L, dampap2R; // dampening all-pass (2)
	sf_rv_delay_st      cbassd1L, cbassd1R; // cross-fade bass delay (1)
	sf_rv_allpass2_st   cbassap1L, cbassap1R; // cross-fade bass allpass (1)
	sf_rv_delay_st      cbassd2L, cbassd2R; // cross-fade bass delay (2)
	sf_rv_allpass3_st   cbassap2L, cbassap2R; // cross-fade bass allpass (2)
	sf_rv_lfo_st        lfo2;
	sf_rv_iir1_st       lfo2_lpf;
	sf_rv_comb_st       combL, combR;
	sf_rv_biquad_st     lastlpfL, lastlpfR;
	sf_rv_delay_st      lastdelayL, lastdelayR;
	sf_rv_delay_st      inpdelayL, inpdelayR;
	int outco[32];
	float loopdecay;
	float wet1, wet2;
	float wander;
	float bassb;
	float ertolate; // early reflection mix parameters
	float erefwet;
	float dry;
} sf_reverb_state_st;
typedef enum
{
	SF_REVERB_PRESET_DEFAULT,
	SF_REVERB_PRESET_SMALLHALL1,
	SF_REVERB_PRESET_SMALLHALL2,
	SF_REVERB_PRESET_MEDIUMHALL1,
	SF_REVERB_PRESET_MEDIUMHALL2,
	SF_REVERB_PRESET_LARGEHALL1,
	SF_REVERB_PRESET_LARGEHALL2,
	SF_REVERB_PRESET_SMALLROOM1,
	SF_REVERB_PRESET_SMALLROOM2,
	SF_REVERB_PRESET_MEDIUMROOM1,
	SF_REVERB_PRESET_MEDIUMROOM2,
	SF_REVERB_PRESET_LARGEROOM1,
	SF_REVERB_PRESET_LARGEROOM2,
	SF_REVERB_PRESET_MEDIUMER1,
	SF_REVERB_PRESET_MEDIUMER2,
	SF_REVERB_PRESET_PLATEHIGH,
	SF_REVERB_PRESET_PLATELOW,
	SF_REVERB_PRESET_LONGREVERB1,
	SF_REVERB_PRESET_LONGREVERB2
} sf_reverb_preset;
extern void sf_advancereverb(sf_reverb_state_st *rv, int rate, int oversamplefactor, float ertolate, float erefwet, float dry, float ereffactor, float erefwidth, float width, float wet, float wander, float bassb, float spin, float inputlpf, float basslpf, float damplpf, float outputlpf, float rt60, float delay);

typedef struct
{
	char subband[2][getMemSizeWarpedPFB(5, 2)];
	float emaAlpha[5];
	float sumStates[5];
	float diffStates[5];
	float mix, minusMix, gain;
} stereoEnhancement;
// ---------------------------------------------------------------------------
// Processing chain
//
// Every effect processes in place on tmpBuffer, so the order they run in is a
// scheduling choice rather than something baked into the maths. These ids let
// the host reorder the chain at runtime. The output stage (post gain +
// limiter) always runs last and is deliberately not part of this list.
// ---------------------------------------------------------------------------
#define JDSP_LIVEPROG_EXTRA 3
// Headroom over JDSP_EFX_COUNT. The chain is completed by appending any effect
// the caller omitted, so this has to be at least the number of effects or the
// append truncates and drops one again - the exact fault it exists to prevent.
#define JDSP_EFX_MAX 48
enum JdspEffectId
{
	JDSP_EFX_TUBE = 0,
	JDSP_EFX_COMPRESSOR,
	JDSP_EFX_PITCHSHIFT,
	JDSP_EFX_FETCOMP,
	JDSP_EFX_DIFFSURROUND,
	JDSP_EFX_BASSBOOST,
	JDSP_EFX_VDYNBASS,
	JDSP_EFX_VIPERBASS,
	JDSP_EFX_BASSEX,
	JDSP_EFX_EQUALIZER,
	JDSP_EFX_ARBITRARYMAG,
	JDSP_EFX_CONVOLVER,
	JDSP_EFX_DDC,
	JDSP_EFX_LIVEPROG,
	JDSP_EFX_LIVEPROG2,
	JDSP_EFX_LIVEPROG3,
	JDSP_EFX_LIVEPROG4,
	JDSP_EFX_CROSSFEED,
	JDSP_EFX_CURE,
	JDSP_EFX_STEREOWIDE,
	JDSP_EFX_FIELDSURROUND,
	JDSP_EFX_HPSURROUND,
	JDSP_EFX_SPECTRUMEXT,
	JDSP_EFX_CLARITY,
	JDSP_EFX_AGC,
	JDSP_EFX_SPEAKEROPT,
	JDSP_EFX_REVERB,
	JDSP_EFX_VREVERB,
	JDSP_EFX_ECHODELAY,
	JDSP_EFX_MULTIBANDDIST,
	JDSP_EFX_MAXIMIZER,
	JDSP_EFX_DYNAMICEQ,
	JDSP_EFX_IMAGING,
	JDSP_EFX_TRANSIENT,
	JDSP_EFX_LOWEND,
	JDSP_EFX_COUNT
};
typedef struct
{
	NSEEL_VMCTX vm;
	NSEEL_CODEHANDLE codehandleInit, codehandleProcess;
	float *vmFs, *input1, *input2;
	int compileSucessfully;
    int active;
} LiveProg;
typedef struct
{
	double b0, b1, b2, a1, a2;
	double v1L, v2L, v1R, v2R; // State
} DirectForm2;
typedef struct
{
	char *oldFile;
	int usedSOSCount;
	DirectForm2 *sosPointer;
} DDC;
/* Minimum/maximum cut frequency (Hz) */
/* bs2b_set_level_fcut() */
#define BS2B_MINFCUT 300
#define BS2B_MAXFCUT 2000
/* Minimum/maximum feed level (dB * 10 @ low frequencies) */
/* bs2b_set_level_feed() */
#define BS2B_MINFEED 10   /* 1 dB */
#define BS2B_MAXFEED 150  /* 15 dB */
/* Default crossfeed levels */
/* Sets a new coefficients by new crossfeed value.
 * level = ( ( uint32_t )fcut | ( ( uint32_t )feed << 16 ) )
 * where 'feed' is crossfeeding level at low frequencies (dB * 10)
 * and 'fcut' is cut frecuency (Hz)
 */
#define BS2B_DEFAULT_CLEVEL  ((unsigned int)700 | ((unsigned int)45 << 16))
#define BS2B_CMOY_CLEVEL     ((unsigned int)700 | ((unsigned int)60 << 16))
#define BS2B_JMEIER_CLEVEL   ((unsigned int)650 | ((unsigned int)95 << 16))
typedef struct str_t_bs2bd
{
	double a0_lo, b1_lo;         /* Lowpass IIR filter coefficients */
	double a0_hi, a1_hi, b1_hi;  /* Highboost IIR filter coefficients */
	double gain;                 /* Global gain against overloading */
	/* Buffer of last filtered sample: [0] 1-st channel, [1] 2-d channel */
	struct { double asis[2], lo[2], hi[2]; } lfs;
} t_bs2bdp;
/* Get flevel value that used in function BS2BInit */
int BS2BCalculateflevel(unsigned int fcut, unsigned int gain);
void BS2BInit(t_bs2bdp *bs2bdp, unsigned int samplerate, int flevel);
/* sample poits to double floats native endians */
void BS2BProcess(t_bs2bdp *bs2bdp, double *sampleL, double *sampleR);
typedef struct
{
	int mode; // 0: BS2B Lv 1, 1: BS2B Lv 2, 2: HRTF crossfeed, 2: HRTF surround 1, 2: HRTF surround 2, 2: HRTF surround 3
	t_bs2bdp bs2b[2];
	FFTConvolver2x4x2 *conv[3];
	TwoStageFFTConvolver2x4x2 *convLong_T_S;
	FFTConvolver2x4x2 *convLong_S_S;
	void(*process)(struct dspsys *, size_t);
} Crossfeed;
typedef struct dspsys dspsys;
typedef struct
{
	FFTConvolver2x2 *conv1d2x2_S_S;
	TwoStageFFTConvolver2x2 *conv1d2x2_T_S;
	FFTConvolver2x4x2 *conv1d2x4x2_S_S;
	TwoStageFFTConvolver2x4x2 *conv1d2x4x2_T_S;
	void(*process)(struct dspsys*, size_t);
} Convolver1D;
typedef struct
{
	ArbitraryEq coeffGen;
	unsigned int filterLen;
	FFTConvolver2x2 convState;
} ArbEqConv;
#define NUMPTS 15
typedef struct
{
	// FIR
	int currentInterpolationMode, operatingMode;
	ierper pch1, pch2;
	ArbEqConv instance;
	FFTConvolver2x2 conv;
	double freq[NUMPTS + 2];
	double gain[NUMPTS + 2];
	// IIR
	int order, nSec;
	float c1[(NUMPTS - 1) * MAXSECTIONS];
	float c2[(NUMPTS - 1) * MAXSECTIONS];
	float d0[(NUMPTS - 1) * MAXSECTIONS];
	float d1[(NUMPTS - 1) * MAXSECTIONS];
	float z1_AL[(NUMPTS - 1) * MAXSECTIONS];
	float z2_AL[(NUMPTS - 1) * MAXSECTIONS];
	float z1_AR[(NUMPTS - 1) * MAXSECTIONS];
	float z2_AR[(NUMPTS - 1) * MAXSECTIONS];
	float overallGain;
	char sec[NUMPTS - 1];
} MultimodalEQ;
extern unsigned int HSHOSVF(double fs, double fc, unsigned int filterOrder, double gain, double overallGainDb, float *c1, float *c2, float *d0, float *d1, float *overallGain);
typedef struct
{
	ArbEqConv instance;
	FFTConvolver2x2 conv;
	int linearPhase;
} arbitraryMagnitude;
typedef struct
{
	float *impulseResponse;
	unsigned int impChannels, impulseLengthActual;
} tmpIRData;
typedef struct dspsys
{
	// Sys var
	char enableASRC;
	IntegerASRCHandler asrc[2];
	float trueSampleRate, fs;
	// Effect
	// Compressor
	int compEnabled, compForceRefresh;
	FFTCompander comp;
	// Bass boost
	int bassBoostEnabled;
	DBB dbb;
	// Equalizer
	int equalizerEnabled, equalizerForceRefresh;
	MultimodalEQ mEQ;
	// Reverb
	int reverbEnabled;
	sf_reverb_state_st reverb;
	// Stereo enhancement
	int sterEnhEnabled;
	stereoEnhancement sterEnh;
	// Vacuum tube
	int tubeEnabled;
	VacuumTube tube;
	int bassExEnabled;
	BassExciter bassEx;
	int spectrumExtEnabled;
	SpectrumExtension spectrumExt;
	int vdynBassEnabled;
	VDynamicBass vdynBass;
	int diffSurroundEnabled;
	DiffSurround diffSurround;
	int viperClarityEnabled;
	ViperClarity viperClarity;
	int fieldSurroundEnabled;
	FieldSurround fieldSurround;
	int agcEnabled;
	Agc agc;
	int hpSurroundEnabled;
	HpSurround hpSurround;
	int fetCompEnabled;
	FetComp fetComp;
	int cureEnabled;
	Cure cure;
	int viperBassEnabled;
	ViperBass viperBass;
	int vreverbEnabled;
	VReverb vreverb;
	int speakerOptEnabled;
	SpeakerOpt speakerOpt;
	int pitchShiftEnabled;
	PitchShift pitchShift;
	int echoDelayEnabled;
	EchoDelay echoDelay;
	int multibandDistEnabled;
	MultibandDist multibandDist;
	int maximizerEnabled;
	int dynamicEqEnabled;
	DynamicEq dynamicEq;
	int imagingEnabled;
	Imaging imaging;
	int transientEnabled;
	Transient transient;
	int lowEndEnabled;
	LowEnd lowEnd;
	Maximizer maximizer;
	// Crossfeed
	int crossfeedEnabled, crossfeedForceRefresh;
	Crossfeed advXF;
	// DDC
	int ddcEnabled, ddcForceRefresh;
	DDC vdcFl;
	// Convolver
	int convolverEnabled;
	Convolver1D conv;
	// Live programmable effect
	int liveprogEnabled;
	LiveProg eel;
	// Additional chained Liveprog slots (slot 0 is 'eel' above)
	int liveprogExtraEnabled[JDSP_LIVEPROG_EXTRA];
	LiveProg eelExtra[JDSP_LIVEPROG_EXTRA];
	// User-defined processing order
	int chainOrder[JDSP_EFX_MAX];
	int chainCount;
	// Arbitrary magnitude response
	int arbitraryMagEnabled, arbMagForceRefresh;
	arbitraryMagnitude arbMag;
	// Output limiter
	float postGain;
	JLimiter limiter;
	size_t blockSize, blockSizeMax, pw2BlockMemSize;
	float *tmpBuffer[6];
	// Internal function pointer
	void(*processInternal)(struct dspsys *, size_t);
	int32_t(*i32_from_p24)(const uint8_t *);
	void (*p24_from_i32)(int32_t, uint8_t *);
	// I/O function pointer
	void(*processInt16Deinterleaved)(struct dspsys*, int16_t*, int16_t*, int16_t*, int16_t*, size_t);
	void(*processInt32Deinterleaved)(struct dspsys*, int32_t*, int32_t*, int32_t*, int32_t*, size_t);
	void(*processInt8_24Deinterleaved)(struct dspsys *, int32_t*, int32_t*, int32_t*, int32_t*, size_t);
	void(*processInt24PackedDeinterleaved)(struct dspsys *, uint8_t*, uint8_t*, uint8_t*, uint8_t*, size_t);
	void(*processFloatDeinterleaved)(struct dspsys*, float*, float*, float*, float*, size_t);
	void(*processInt16Multiplexd)(struct dspsys*, int16_t*, int16_t*, size_t);
	void(*processInt32Multiplexd)(struct dspsys*, int32_t*, int32_t*, size_t);
	void(*processInt8_24Multiplexd)(struct dspsys*, int32_t*, int32_t*, size_t);
	void(*processInt24PackedMultiplexd)(struct dspsys*, uint8_t*, uint8_t*, size_t);
	void(*processFloatMultiplexd)(struct dspsys*, float*, float*, size_t);
	// Blobs(resampled)
	int blobsResampledLen;
	float *blobsCh1[3];
	float *blobsCh2[3];
	float *blobsCh3[3];
	float *blobsCh4[3];
	int frameLenSVirResampled;
	float *hrtfblobsResampled[4];
	tmpIRData impulseResponseStorage;
	// Mutex lock(pthread)
	int isMutexSuccess;
	pthread_mutex_t m_in_processing;
	// Random number and related
	uint64_t rndstate[2];
} JamesDSPLib;
// JamesDSP controller
extern void JamesDSPGlobalMemoryAllocation();
extern void JamesDSPGlobalMemoryDeallocation();
extern void JamesDSPReallocateBlock(JamesDSPLib *jdsp, size_t blockSizeMax);
extern void JamesDSP_Load_benchmark(double *_c0, double *_c1);
extern void JamesDSP_Save_benchmark(double *_c0, double *_c1);
extern void JamesDSP_Start_benchmark();
extern void jdsp_lock(JamesDSPLib *jdsp);
extern void jdsp_unlock(JamesDSPLib *jdsp);
extern void JamesDSPFree(JamesDSPLib *jdsp);
extern void JamesDSPInit(JamesDSPLib *jdsp, int blockSizeMax, float sample_rate);
extern void JamesDSPSetPostGain(JamesDSPLib *jdsp, double pGaindB);
extern int JamesDSPGetMutexStatus(JamesDSPLib *jdsp);
extern void JamesDSPSetSampleRate(JamesDSPLib *jdsp, float new_sample_rate, int forceRefresh);
extern int selectConvPartitions(JamesDSPLib *jdsp, unsigned int impulseLengthActual, unsigned int *seg2Len);
// Limiter
extern void JLimiterSetCoefficients(JamesDSPLib *jdsp, double thresholddB, double msRelease);
extern void JLimiterInit(JamesDSPLib *jdsp);
extern void JLimiterSetMode(JamesDSPLib *jdsp, int mode);
// Psychoacoustic bass exciter
extern void BassExciterSetParam(JamesDSPLib *jdsp, float cutoff, float intensity, float mixPct);
extern void BassExciterProcess(JamesDSPLib *jdsp, size_t n);
extern void BassExciterEnable(JamesDSPLib *jdsp);
extern void BassExciterDisable(JamesDSPLib *jdsp);
// Spectrum extension (treble exciter)
extern void SpectrumExtensionSetParam(JamesDSPLib *jdsp, float barkFreq, float strengthPct);
extern void SpectrumExtensionProcess(JamesDSPLib *jdsp, size_t n);
extern void SpectrumExtensionEnable(JamesDSPLib *jdsp);
extern void SpectrumExtensionDisable(JamesDSPLib *jdsp);
extern void BassExciterSetParam2(JamesDSPLib *jdsp, int band2On, float cutoff2, float intensity2, float mixPct2);
// ViPER dynamic bass
extern void VDynBassSetParam(JamesDSPLib *jdsp, float gainPct, float x1, float x2, float y1, float y2, float sgxPct, float sgyPct);
extern void VDynBassProcess(JamesDSPLib *jdsp, size_t n);
extern void VDynBassEnable(JamesDSPLib *jdsp);
extern void VDynBassDisable(JamesDSPLib *jdsp);
// Differential surround
extern void DiffSurroundSetParam(JamesDSPLib *jdsp, float delayLms, float delayRms);
extern void DiffSurroundProcess(JamesDSPLib *jdsp, size_t n);
extern void DiffSurroundEnable(JamesDSPLib *jdsp);
extern void DiffSurroundDisable(JamesDSPLib *jdsp);
// ViPER clarity
extern void ViperClaritySetParam(JamesDSPLib *jdsp, int mode, float gainDb);
extern void ViperClarityProcess(JamesDSPLib *jdsp, size_t n);
extern void ViperClarityEnable(JamesDSPLib *jdsp);
extern void ViperClarityDisable(JamesDSPLib *jdsp);
// Field surround
extern void FieldSurroundSetParam(JamesDSPLib *jdsp, float strengthPct, float midImagePct);
extern void FieldSurroundProcess(JamesDSPLib *jdsp, size_t n);
extern void FieldSurroundEnable(JamesDSPLib *jdsp);
extern void FieldSurroundDisable(JamesDSPLib *jdsp);
// Auto gain control
extern void AgcSetParam(JamesDSPLib *jdsp, float targetPct, float maxBoostDb);
extern void AgcProcess(JamesDSPLib *jdsp, size_t n);
extern void AgcEnable(JamesDSPLib *jdsp);
extern void AgcDisable(JamesDSPLib *jdsp);
// Headphone surround+ (lite)
extern void HpSurroundSetParam(JamesDSPLib *jdsp, float strengthPct, float roomPct);
extern void HpSurroundProcess(JamesDSPLib *jdsp, size_t n);
extern void HpSurroundEnable(JamesDSPLib *jdsp);
extern void HpSurroundDisable(JamesDSPLib *jdsp);
extern void FetCompSetParam(JamesDSPLib *jdsp, float thresholdDb, float ratio, float attackMs, float releaseMs, float makeupDb);
extern void FetCompProcess(JamesDSPLib *jdsp, size_t n);
extern void FetCompEnable(JamesDSPLib *jdsp);
extern void FetCompDisable(JamesDSPLib *jdsp);
extern void CureSetParam(JamesDSPLib *jdsp, int level);
extern void CureProcess(JamesDSPLib *jdsp, size_t n);
extern void CureEnable(JamesDSPLib *jdsp);
extern void CureDisable(JamesDSPLib *jdsp);
extern void ViperBassSetParam(JamesDSPLib *jdsp, int mode, float freq, float gainDb);
extern void ViperBassProcess(JamesDSPLib *jdsp, size_t n);
extern void ViperBassEnable(JamesDSPLib *jdsp);
extern void ViperBassDisable(JamesDSPLib *jdsp);
extern void VReverbSetParam(JamesDSPLib *jdsp, int model, float roomPct, float dampPct,
	float widthPct, float predelayMs, float decayPct, float diffusionPct,
	float modPct, float bassPct, float erPct, float wetPct, float dryPct);
extern void VReverbProcess(JamesDSPLib *jdsp, size_t n);
extern void VReverbEnable(JamesDSPLib *jdsp);
extern void VReverbDisable(JamesDSPLib *jdsp);
extern void SpeakerOptSetParam(JamesDSPLib *jdsp, float strengthPct);
extern void SpeakerOptProcess(JamesDSPLib *jdsp, size_t n);
extern void SpeakerOptEnable(JamesDSPLib *jdsp);
extern void SpeakerOptDisable(JamesDSPLib *jdsp);
extern void PitchShiftSetParam(JamesDSPLib *jdsp, float semitones, float mixPct);
extern void PitchShiftProcess(JamesDSPLib *jdsp, size_t n);
extern void PitchShiftEnable(JamesDSPLib *jdsp);
extern void PitchShiftDisable(JamesDSPLib *jdsp);
extern void EchoDelaySetParam(JamesDSPLib *jdsp, float inputLevel, float timeMs,
	float smoothingPct, float offsetMs, int keepPitch,
	int model, float stereoPct,
	float feedbackPct, float cutoffHz, float resonance, int filterType,
	float smpRatePct, float bits,
	float modRateHz, float modTimePct, float modCutoffPct,
	float diffusionPct, float spreadPct,
	int distMode, float distLevel, float knee, float symmetry,
	float tonePct, float wetPct, float dryPct);
extern void EchoDelayUpdateFilter(EchoDelay *e, float cutoffHz);
extern void EchoDelayProcess(JamesDSPLib *jdsp, size_t n);
extern void EchoDelayEnable(JamesDSPLib *jdsp);
extern void LowEndSetParam(JamesDSPLib *jdsp, float subsonicHz,
	float weightHz, float weightDb, float mudHz, float mudDb, float mixPct);
extern void LowEndProcess(JamesDSPLib *jdsp, size_t n);
extern void LowEndEnable(JamesDSPLib *jdsp);
extern void LowEndDisable(JamesDSPLib *jdsp);
extern void TransientSetParam(JamesDSPLib *jdsp, float freqLow, float freqHigh,
	float attackLow, float sustainLow, float attackMid, float sustainMid,
	float attackHigh, float sustainHigh, float rangeDb, float mixPct);
extern void TransientProcess(JamesDSPLib *jdsp, size_t n);
extern void TransientEnable(JamesDSPLib *jdsp);
extern void TransientDisable(JamesDSPLib *jdsp);
extern void ImagingSetParam(JamesDSPLib *jdsp, float monoBelowHz,
	float freqLow, float freqMid, float freqHigh,
	float widthLow, float widthMid, float widthHigh, float mixPct);
extern void ImagingProcess(JamesDSPLib *jdsp, size_t n);
extern void ImagingEnable(JamesDSPLib *jdsp);
extern void ImagingDisable(JamesDSPLib *jdsp);
extern void DynamicEqSetBands(JamesDSPLib *jdsp, const float *bands, int count);
extern void DynamicEqSetParam(JamesDSPLib *jdsp, float mixPct);
extern void DynamicEqProcess(JamesDSPLib *jdsp, size_t n);
extern void DynamicEqEnable(JamesDSPLib *jdsp);
extern void DynamicEqDisable(JamesDSPLib *jdsp);
extern void MultibandDistSetBands(JamesDSPLib *jdsp, const float *bands, int count);
extern void MultibandDistSetParam(JamesDSPLib *jdsp,
	int routing, int model,
	float drivePct, float biasPct, float shapePct,
	float bits, float downsamplePct,
	float tonePct, float bandGainPct,
	float chorusRateHz, float chorusDepthMs, float chorusFeedbackPct,
	float chorusSpreadPct, int chorusVoices, float chorusMixPct,
	float mixPct);
extern void MultibandDistProcess(JamesDSPLib *jdsp, size_t n);
extern void MultibandDistEnable(JamesDSPLib *jdsp);
extern void MultibandDistDisable(JamesDSPLib *jdsp);
extern void MaximizerSetParam(JamesDSPLib *jdsp,
	int mode, float gainDb, float ceilingDb, float releaseMs,
	float characterPct, float transientPct, int truePeak,
	float stereoLinkPct, int oversample, int clipShape);
extern void MaximizerProcess(JamesDSPLib *jdsp, size_t n);
extern void MaximizerEnable(JamesDSPLib *jdsp);
extern void MaximizerDisable(JamesDSPLib *jdsp);
extern void EchoDelayDisable(JamesDSPLib *jdsp);
extern void JamesDSPReleaseEffectBuffers(JamesDSPLib *jdsp);
// Compressor
extern void CompressorConstructor(JamesDSPLib *jdsp);
extern void CompressorDestructor(JamesDSPLib *jdsp);
extern void CompressorSetParam(JamesDSPLib *jdsp, float fgt_facT, int granularity, int tfresolution, char forceRefresh);
extern void CompressorSetGain(JamesDSPLib *jdsp, double *freq, double *gains, char cpy);
extern void CompressorEnable(JamesDSPLib *jdsp, char enable);
extern void CompressorDisable(JamesDSPLib *jdsp);
extern void CompressorProcess(JamesDSPLib *jdsp, size_t n);
// Bass boost
extern void BassBoostEnable(JamesDSPLib *jdsp);
extern void BassBoostDisable(JamesDSPLib *jdsp);
extern void BassBoostConstructor(JamesDSPLib *jdsp);
extern void BassBoostSetParam(JamesDSPLib *jdsp, float maxG);
extern void BassBoostProcess(JamesDSPLib *jdsp, size_t n);
// Reverb
extern void Reverb_SetParam(JamesDSPLib *jdsp, int presets);
extern void ReverbEnable(JamesDSPLib *jdsp);
extern void ReverbDisable(JamesDSPLib *jdsp);
extern void ReverbProcess(JamesDSPLib *jdsp, size_t n);
// Stereo enhancement
extern void StereoEnhancementDestructor(JamesDSPLib *jdsp);
extern void StereoEnhancementConstructor(JamesDSPLib *jdsp);
extern void StereoEnhancementRefresh(JamesDSPLib *jdsp);
extern void StereoEnhancementSetParam(JamesDSPLib *jdsp, float mix);
extern void StereoEnhancementEnable(JamesDSPLib *jdsp);
extern void StereoEnhancementDisable(JamesDSPLib *jdsp);
extern void StereoEnhancementProcess(JamesDSPLib *jdsp, size_t n);
// Vacuum tube
extern void VacuumTubeEnable(JamesDSPLib *jdsp);
extern void VacuumTubeDisable(JamesDSPLib *jdsp);
extern void VacuumTubeSetGain(JamesDSPLib *jdsp, double dbGain);
extern void VacuumTubeProcess(JamesDSPLib *jdsp, size_t n);
// Live programmable effect
extern const char* checkErrorCode(int errCode);
extern void LiveProgConstructor(JamesDSPLib *jdsp);
extern void LiveProgDestructor(JamesDSPLib *jdsp);
extern int LiveProgStringParser(JamesDSPLib *jdsp, char *eelCode);
extern void LiveProgEnable(JamesDSPLib *jdsp);
extern void LiveProgDisable(JamesDSPLib *jdsp);
extern void LiveProgProcess(JamesDSPLib *jdsp, size_t n);
// Chained Liveprog slots. Slot 0 is the original engine; 1..3 are extra.
extern int LiveProgStringParserSlot(JamesDSPLib *jdsp, int slot, char *eelCode);
extern void LiveProgEnableSlot(JamesDSPLib *jdsp, int slot);
extern void LiveProgDisableSlot(JamesDSPLib *jdsp, int slot);
extern void LiveProgProcessSlot(JamesDSPLib *jdsp, int slot, size_t n);
extern void LiveProgConstructorSlot(JamesDSPLib *jdsp, int slot);
extern void LiveProgDestructorSlot(JamesDSPLib *jdsp, int slot);
// Processing order
extern void JamesDSPSetChainOrder(JamesDSPLib *jdsp, const int *order, int count);
extern void JamesDSPResetChainOrder(JamesDSPLib *jdsp);
// DDC
extern void DDCConstructor(JamesDSPLib *jdsp);
extern void DDCDestructor(JamesDSPLib *jdsp);
extern int DDCEnable(JamesDSPLib *jdsp, char enable);
extern void DDCDisable(JamesDSPLib *jdsp);
extern int DDCStringParser(JamesDSPLib *jdsp, char *newStr);
extern void DDCProcess(JamesDSPLib *jdsp, size_t n);
// Crossfeed
extern void CrossfeedConstructor(JamesDSPLib *jdsp);
extern void CrossfeedDestructor(JamesDSPLib *jdsp);
extern void CrossfeedEnable(JamesDSPLib *jdsp, char enable);
extern void CrossfeedDisable(JamesDSPLib *jdsp);
extern void CrossfeedChangeMode(JamesDSPLib *jdsp, int nMode);
extern void CrossfeedProcess(JamesDSPLib *jdsp, size_t n);
// Convolver
extern void Convolver1DEnable(JamesDSPLib *jdsp);
extern void Convolver1DDisable(JamesDSPLib *jdsp);
extern void Convolver1DConstructor(JamesDSPLib *jdsp);
extern void Convolver1DDestructor(JamesDSPLib *jdsp);
extern int Convolver1DLoadImpulseResponse(JamesDSPLib *jdsp, float *tempImpulseFloat, unsigned int impChannels, size_t impulseLengthActual, char updateOld);
// Arbitrary magnitude response
extern void ArbitraryResponseEqualizerConstructor(JamesDSPLib *jdsp);
extern void ArbitraryResponseEqualizerDestructor(JamesDSPLib *jdsp);
extern void ArbitraryResponseEqualizerSetPhaseMode(JamesDSPLib *jdsp, int linearPhase);
extern void ArbitraryResponseEqualizerStringParser(JamesDSPLib *jdsp, char *stringEq);
extern void ArbitraryResponseEqualizerEnable(JamesDSPLib *jdsp, char enable);
extern void ArbitraryResponseEqualizerDisable(JamesDSPLib *jdsp);
extern void ArbitraryResponseEqualizerProcess(JamesDSPLib *jdsp, size_t n);
// FIR Equalizer
extern void MultimodalEqualizerConstructor(JamesDSPLib *jdsp);
extern void MultimodalEqualizerDestructor(JamesDSPLib *jdsp);
extern void MultimodalEqualizerAxisInterpolation(JamesDSPLib *jdsp, int interpolationMode, int operatingMode, double *freqAx, double *gaindB);
extern void MultimodalEqualizerEnable(JamesDSPLib *jdsp, char enable);
extern void MultimodalEqualizerDisable(JamesDSPLib *jdsp);
extern void MultimodalEqualizerProcess(JamesDSPLib *jdsp, size_t n);
#endif
