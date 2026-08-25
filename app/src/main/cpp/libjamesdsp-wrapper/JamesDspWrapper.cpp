#include <android/log.h>

#define TAG "JamesDspWrapper_JNI"
#include <Log.h>

#include <string>
#include <jni.h>

#include "JamesDspWrapper.h"
#include "JArrayList.h"
#include "EelVmVariable.h"

extern "C" {
#include "../EELStdOutExtension.h"
#include <jdsp_header.h>
}

// C interop
inline JamesDSPLib* cast(void* raw){
    if(raw == nullptr)
    {
        LOGE("JamesDspWrapper::cast: JamesDSPLib pointer is NULL")
    }
    return static_cast<JamesDSPLib*>(raw);
}

inline JamesDspWrapper* castWrapper(jlong raw){
    if(raw == 0)
    {
        LOGE("JamesDspWrapper::castWrapper: JamesDspWrapper pointer is NULL")
    }
    return reinterpret_cast<JamesDspWrapper*>(raw);
}

#define RETURN_IF_NULL(name, retval) \
    if(name == nullptr)      \
        return retval;

#define DECLARE_WRAPPER(retval) \
     if(self == 0L) \
        return retval; \
     auto* wrapper = castWrapper(self); \
     RETURN_IF_NULL(wrapper, retval)

#define DECLARE_DSP(retval) \
    DECLARE_WRAPPER(retval) \
    auto* dsp = cast(wrapper->dsp); \
    RETURN_IF_NULL(dsp, retval)

#define DECLARE_WRAPPER_V DECLARE_WRAPPER()
#define DECLARE_DSP_V DECLARE_DSP()
#define DECLARE_WRAPPER_B DECLARE_WRAPPER(false)
#define DECLARE_DSP_B DECLARE_DSP(false)

inline int32_t arySearch(int32_t *array, int32_t N, int32_t x)
{
    for (int32_t i = 0; i < N; i++)
    {
        if (array[i] == x)
            return i;
    }
    return -1;
}

#define FLOIDX 20000
/*inline void* GetStringForIndex(eel_string_context_state *st, float val, int32_t write)
{
    auto castedValue = (int32_t)(val + 0.5f);
    if (castedValue < FLOIDX)
        return nullptr;
    int32_t idx = arySearch(st->map, st->slot, castedValue);
    if (idx < 0)
        return nullptr;
    if (!write)
    {
        s_str *tmp = &st->m_literal_strings[idx];
        const char *s = s_str_c_str(tmp);
        return (void*)s;
    }
    else
        return (void*)&st->m_literal_strings[idx];
}*/

extern "C" JNIEXPORT jlong JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_alloc(JNIEnv *env, jobject obj, jobject callback)
{
    auto* self = new JamesDspWrapper();
    self->callbackInterface = env->NewGlobalRef(callback);
    self->env = env;

    jclass callbackClass = env->GetObjectClass(callback);
    if (callbackClass == nullptr)
    {
        LOGE("JamesDspWrapper::ctor: Cannot find callback class");
        delete self;
        return 0;
    }
    else
    {
        self->callbackOnLiveprogOutput = env->GetMethodID(callbackClass, "onLiveprogOutput",
                                                      "(Ljava/lang/String;)V");
        self->callbackOnLiveprogExec = env->GetMethodID(callbackClass, "onLiveprogExec",
                                                    "(Ljava/lang/String;)V");
        self->callbackOnLiveprogResult = env->GetMethodID(callbackClass, "onLiveprogResult",
                                                          "(ILjava/lang/String;Ljava/lang/String;)V");
        self->callbackOnVdcParseError = env->GetMethodID(callbackClass, "onVdcParseError",
                                                          "()V");
        if (self->callbackOnLiveprogOutput == nullptr || self->callbackOnLiveprogExec == nullptr ||
            self->callbackOnLiveprogResult == nullptr || self->callbackOnVdcParseError == nullptr)
        {
            LOGE("JamesDspWrapper::ctor: Cannot find callback method");
            delete self;
            return 0;
        }
    }


    auto* _dsp = (JamesDSPLib*)malloc(sizeof(JamesDSPLib));
    memset(_dsp, 0, sizeof(JamesDSPLib));

    if(!_dsp)
    {
        LOGE("JamesDspWrapper::ctor: Failed to allocate memory for libjamesdsp class object");
        delete self;
        return 1;
    }

    JamesDSPGlobalMemoryAllocation();
    JamesDSPInit(_dsp, 128, 48000);

    if(!JamesDSPGetMutexStatus(_dsp))
    {
        LOGE("JamesDspWrapper::ctor: JamesDSPGetMutexStatus returned false. "
                    "Cannot run safely in multi-threaded environment.");
        JamesDSPFree(_dsp);
        JamesDSPGlobalMemoryDeallocation();
        delete self;
        return 2;
    }

    self->dsp = _dsp;

    LOGD("JamesDspWrapper::ctor: memory allocated at %lx", (long)self);
    return (long)self;
}

extern "C" JNIEXPORT void JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_free(JNIEnv *env, jobject obj, jlong self)
{
    DECLARE_DSP_V

    LOGD("JamesDspWrapper::dtor: freeing memory allocated at %lx", (long)self);

    setStdOutHandler(nullptr, nullptr);

    JamesDSPFree(dsp);
    free(dsp);
    wrapper->dsp = nullptr;

    JamesDSPGlobalMemoryDeallocation();

    env->DeleteGlobalRef(wrapper->callbackInterface);
    delete wrapper;

    LOGD("JamesDspWrapper::dtor: memory freed");
}

extern "C" JNIEXPORT jint JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_getBenchmarkSize(JNIEnv *env, jobject obj) {
    return MAX_BENCHMARK;
}

extern "C" JNIEXPORT void JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_runBenchmark(JNIEnv *env, jobject obj, jdoubleArray jc0, jdoubleArray jc1)
{
    LOGD("JamesDspWrapper::runBenchmark: started");

    auto c0 = env->GetDoubleArrayElements(jc0, nullptr);
    auto c1 = env->GetDoubleArrayElements(jc1, nullptr);

    JamesDSP_Start_benchmark();
    JamesDSP_Save_benchmark(c0, c1);

    env->ReleaseDoubleArrayElements(jc0, c0, 0);
    env->ReleaseDoubleArrayElements(jc1, c1, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_loadBenchmark(JNIEnv *env, jobject obj, jdoubleArray jc0, jdoubleArray jc1)
{
    LOGD("JamesDspWrapper::loadBenchmark: loading data");

    auto c0 = env->GetDoubleArrayElements(jc0, nullptr);
    auto c1 = env->GetDoubleArrayElements(jc1, nullptr);

    JamesDSP_Load_benchmark(c0, c1);

    env->ReleaseDoubleArrayElements(jc0, c0, JNI_ABORT);
    env->ReleaseDoubleArrayElements(jc1, c1, JNI_ABORT);
}

extern "C"
JNIEXPORT void JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setSamplingRate(JNIEnv *env,
                                                                                 jobject obj,
                                                                                 jlong self,
                                                                                 jfloat sample_rate,
                                                                                 jboolean force_refresh)
{
    DECLARE_DSP_V
    JamesDSPSetSampleRate(dsp, sample_rate, force_refresh);
}


extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_isHandleValid(JNIEnv *env, jobject obj, jlong self)
{
    DECLARE_DSP_B // This macro returns false if the DSP object can't be accessed
    return true;
}

extern "C"
JNIEXPORT void JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_processInt16(JNIEnv *env, jobject obj, jlong self, jshortArray inputObj, jshortArray outputObj, jint offset, jint size)
{
    DECLARE_DSP_V

    jsize inputLength;
    if(size < 0)
        inputLength = env->GetArrayLength(inputObj);
    else
        inputLength = size;
    if(offset < 0)
        offset = 0;

    auto input = env->GetShortArrayElements(inputObj, nullptr);
    auto output = env->GetShortArrayElements(outputObj, nullptr);
    dsp->processInt16Multiplexd(dsp, input + offset, output, inputLength / 2);
    env->ReleaseShortArrayElements(inputObj, input, JNI_ABORT);
    env->ReleaseShortArrayElements(outputObj, output, 0);
}

extern "C"
JNIEXPORT void JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_processInt32(JNIEnv *env, jobject obj, jlong self, jintArray inputObj, jintArray outputObj, jint offset, jint size)
{
    DECLARE_DSP_V

    jsize inputLength;
    if(size < 0)
        inputLength = env->GetArrayLength(inputObj);
    else
        inputLength = size;
    if(offset < 0)
        offset = 0;

    auto input = env->GetIntArrayElements(inputObj, nullptr);
    auto output = env->GetIntArrayElements(outputObj, nullptr);
    dsp->processInt32Multiplexd(dsp, input + offset, output, inputLength / 2);
    env->ReleaseIntArrayElements(inputObj, input, JNI_ABORT);
    env->ReleaseIntArrayElements(outputObj, output, 0);
}

extern "C"
JNIEXPORT jbooleanArray JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_processInt24Packed(JNIEnv *env, jobject obj, jlong self, jbooleanArray inputObj)
{
    /* We need to use jbooleanArray (= unsigned 8-bit) instead of jbyteArray (= signed 8-bit) here! */

    // Return inputObj if DECLARE failed
    DECLARE_DSP(inputObj)

    auto inputLength = env->GetArrayLength(inputObj);
    auto outputObj = env->NewBooleanArray(inputLength);

    auto input = env->GetBooleanArrayElements(inputObj, nullptr);
    auto output = env->GetBooleanArrayElements(outputObj, nullptr);
    dsp->processInt24PackedMultiplexd(dsp, input, output, inputLength / 2);
    env->ReleaseBooleanArrayElements(inputObj, input, JNI_ABORT);
    env->ReleaseBooleanArrayElements(outputObj, output, 0);
    return outputObj;
}

extern "C"
JNIEXPORT jintArray JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_processInt8U24(JNIEnv *env, jobject obj, jlong self, jintArray inputObj)
{
    // Return inputObj if DECLARE failed
    DECLARE_DSP(inputObj)

    auto inputLength = env->GetArrayLength(inputObj);
    auto outputObj = env->NewIntArray(inputLength);

    auto input = env->GetIntArrayElements(inputObj, nullptr);
    auto output = env->GetIntArrayElements(outputObj, nullptr);
    dsp->processInt8_24Multiplexd(dsp, input, output, inputLength / 2);
    env->ReleaseIntArrayElements(inputObj, input, JNI_ABORT);
    env->ReleaseIntArrayElements(outputObj, output, 0);
    return outputObj;
}

extern "C"
JNIEXPORT void JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_processFloat(JNIEnv *env, jobject obj, jlong self, jfloatArray inputObj, jfloatArray outputObj, jint offset, jint size)
{
    DECLARE_DSP_V

    jsize inputLength;
    if(size < 0)
        inputLength = env->GetArrayLength(inputObj);
    else
        inputLength = size;
    if(offset < 0)
        offset = 0;

    auto input = env->GetFloatArrayElements(inputObj, nullptr);
    auto output = env->GetFloatArrayElements(outputObj, nullptr);

    dsp->processFloatMultiplexd(dsp, input + offset, output, inputLength / 2);

    env->ReleaseFloatArrayElements(inputObj, input, JNI_ABORT);
    env->ReleaseFloatArrayElements(outputObj, output, 0);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setLimiter(JNIEnv *env, jobject obj, jlong self, jfloat threshold, jfloat release)
{
    DECLARE_DSP_B
    JLimiterSetCoefficients(dsp, threshold, release);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setLimiterMode(JNIEnv *env, jobject obj, jlong self, jint mode)
{
    DECLARE_DSP_B
    JLimiterSetMode(dsp, mode);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setBassExciter(JNIEnv *env, jobject obj, jlong self, jboolean enable, jfloat cutoff, jfloat intensity, jfloat mix, jboolean band2, jfloat cutoff2, jfloat intensity2, jfloat mix2)
{
    DECLARE_DSP_B
    BassExciterSetParam(dsp, cutoff, intensity, mix);
    BassExciterSetParam2(dsp, band2 ? 1 : 0, cutoff2, intensity2, mix2);
    if (enable)
        BassExciterEnable(dsp);
    else
        BassExciterDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setVDynBass(JNIEnv *env, jobject obj, jlong self, jboolean enable, jfloat gain, jfloat x1, jfloat x2, jfloat y1, jfloat y2, jfloat sgx, jfloat sgy)
{
    DECLARE_DSP_B
    VDynBassSetParam(dsp, gain, x1, x2, y1, y2, sgx, sgy);
    if (enable)
        VDynBassEnable(dsp);
    else
        VDynBassDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setDiffSurround(JNIEnv *env, jobject obj, jlong self, jboolean enable, jfloat delayLms, jfloat delayRms)
{
    DECLARE_DSP_B
    DiffSurroundSetParam(dsp, delayLms, delayRms);
    if (enable)
        DiffSurroundEnable(dsp);
    else
        DiffSurroundDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setViperClarity(JNIEnv *env, jobject obj, jlong self, jboolean enable, jint mode, jfloat gain)
{
    DECLARE_DSP_B
    ViperClaritySetParam(dsp, mode, gain);
    if (enable) ViperClarityEnable(dsp); else ViperClarityDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setFieldSurround(JNIEnv *env, jobject obj, jlong self, jboolean enable, jfloat strength, jfloat midImage)
{
    DECLARE_DSP_B
    FieldSurroundSetParam(dsp, strength, midImage);
    if (enable) FieldSurroundEnable(dsp); else FieldSurroundDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setAgc(JNIEnv *env, jobject obj, jlong self, jboolean enable, jfloat target, jfloat maxBoost)
{
    DECLARE_DSP_B
    AgcSetParam(dsp, target, maxBoost);
    if (enable) AgcEnable(dsp); else AgcDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setHpSurround(JNIEnv *env, jobject obj, jlong self, jboolean enable, jfloat strength, jfloat room)
{
    DECLARE_DSP_B
    HpSurroundSetParam(dsp, strength, room);
    if (enable) HpSurroundEnable(dsp); else HpSurroundDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setFetComp(JNIEnv *env, jobject obj, jlong self, jboolean enable, jfloat threshold, jfloat ratio, jfloat attack, jfloat release, jfloat makeup)
{
    DECLARE_DSP_B
    FetCompSetParam(dsp, threshold, ratio, attack, release, makeup);
    if (enable) FetCompEnable(dsp); else FetCompDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setCure(JNIEnv *env, jobject obj, jlong self, jboolean enable, jint level)
{
    DECLARE_DSP_B
    CureSetParam(dsp, level);
    if (enable) CureEnable(dsp); else CureDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setViperBass(JNIEnv *env, jobject obj, jlong self, jboolean enable, jint mode, jfloat freq, jfloat gain)
{
    DECLARE_DSP_B
    ViperBassSetParam(dsp, mode, freq, gain);
    if (enable) ViperBassEnable(dsp); else ViperBassDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setVReverb(JNIEnv *env, jobject obj, jlong self,
        jboolean enable, jint model, jfloat room, jfloat damp, jfloat width, jfloat predelay,
        jfloat decay, jfloat diffusion, jfloat mod, jfloat bass, jfloat er, jfloat wet, jfloat dry)
{
    DECLARE_DSP_B
    VReverbSetParam(dsp, model, room, damp, width, predelay, decay, diffusion, mod, bass, er, wet, dry);
    if (enable) VReverbEnable(dsp); else VReverbDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setSpeakerOpt(JNIEnv *env, jobject obj, jlong self, jboolean enable, jfloat strength)
{
    DECLARE_DSP_B
    SpeakerOptSetParam(dsp, strength);
    if (enable) SpeakerOptEnable(dsp); else SpeakerOptDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setChainOrder(JNIEnv *env, jobject obj, jlong self, jintArray order)
{
    DECLARE_DSP_B
    if (order == nullptr)
    {
        JamesDSPResetChainOrder(dsp);
        return true;
    }
    jsize count = env->GetArrayLength(order);
    if (count <= 0)
    {
        JamesDSPResetChainOrder(dsp);
        return true;
    }
    jint *elements = env->GetIntArrayElements(order, nullptr);
    if (elements == nullptr)
        return false;
    JamesDSPSetChainOrder(dsp, reinterpret_cast<const int *>(elements), (int)count);
    env->ReleaseIntArrayElements(order, elements, JNI_ABORT);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setEchoDelay(JNIEnv *env, jobject obj, jlong self,
        jboolean enable,
        jfloat input,
        jfloat time,
        jfloat smoothing,
        jfloat offset,
        jboolean keepPitch,
        jint model,
        jfloat stereo,
        jfloat feedback,
        jfloat cutoff,
        jfloat res,
        jint filter,
        jfloat smpRate,
        jfloat bits,
        jfloat modRate,
        jfloat modTime,
        jfloat modCutoff,
        jfloat diffusion,
        jfloat spread,
        jint distMode,
        jfloat distLevel,
        jfloat knee,
        jfloat symmetry,
        jfloat tone,
        jfloat wet,
        jfloat dry)
{
    DECLARE_DSP_B
    EchoDelaySetParam(dsp, input, time, smoothing, offset, keepPitch, model, stereo, feedback, cutoff, res, filter, smpRate, bits, modRate, modTime, modCutoff, diffusion, spread, distMode, distLevel, knee, symmetry, tone, wet, dry);
    if (enable) EchoDelayEnable(dsp); else EchoDelayDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setMultibandDist(JNIEnv *env, jobject obj, jlong self,
        jboolean enable,
        jint routing,
        jint model,
        jfloat drive,
        jfloat bias,
        jfloat shape,
        jfloat bits,
        jfloat downsample,
        jfloat tone,
        jfloat bandGain,
        jfloat chorusRate,
        jfloat chorusDepth,
        jfloat chorusFeedback,
        jfloat chorusSpread,
        jint chorusVoices,
        jfloat chorusMix,
        jfloat mix)
{
    DECLARE_DSP_B
    MultibandDistSetParam(dsp, routing, model, drive, bias, shape, bits, downsample,
                          tone, bandGain, chorusRate, chorusDepth, chorusFeedback,
                          chorusSpread, chorusVoices, chorusMix, mix);
    if (enable) MultibandDistEnable(dsp); else MultibandDistDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setMaximizer(JNIEnv *env, jobject obj, jlong self,
        jboolean enable,
        jint mode,
        jfloat gain,
        jfloat ceiling,
        jfloat release,
        jfloat character,
        jfloat transient,
        jboolean truePeak,
        jfloat stereoLink,
        jint oversample,
        jint clipShape)
{
    DECLARE_DSP_B
    MaximizerSetParam(dsp, mode, gain, ceiling, release, character, transient,
                      truePeak, stereoLink, oversample, clipShape);
    if (enable) MaximizerEnable(dsp); else MaximizerDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setMultibandDistBands(JNIEnv *env, jobject obj, jlong self,
        jfloatArray bandsObj)
{
    DECLARE_DSP_B

    // No bands is a legitimate state, not an error: it means the whole signal
    // goes into the distortion rather than a selected range.
    if (bandsObj == nullptr)
    {
        MultibandDistSetBands(dsp, nullptr, 0);
        return true;
    }

    jsize len = env->GetArrayLength(bandsObj);
    if (len % 4 != 0)
    {
        LOGE("JamesDspWrapper::setMultibandDistBands: expected groups of four "
             "(frequency, gain, q, type), got %d values", (int)len);
        return false;
    }

    auto *elems = env->GetFloatArrayElements(bandsObj, nullptr);
    MultibandDistSetBands(dsp, elems, (int)(len / 4));
    env->ReleaseFloatArrayElements(bandsObj, elems, JNI_ABORT);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setTape(JNIEnv *env, jobject obj, jlong self,
        jboolean enable, jfloat wow, jfloat flutter, jfloat saturation,
        jfloat bias, jfloat headBump, jfloat mix)
{
    DECLARE_DSP_B
    TapeSetParam(dsp, wow, flutter, saturation, bias, headBump, mix);
    if (enable) TapeEnable(dsp); else TapeDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setBalance(JNIEnv *env, jobject obj, jlong self,
        jboolean enable, jfloat balance, jboolean swap, jfloat mono)
{
    DECLARE_DSP_B
    BalanceSetParam(dsp, balance, swap ? 1 : 0, mono);
    if (enable) BalanceEnable(dsp); else BalanceDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setVinyl(JNIEnv *env, jobject obj, jlong self,
        jboolean enable, jfloat surface, jfloat crackle, jfloat crackleSize,
        jfloat pops, jfloat clicks, jfloat sizzle, jfloat hiss, jfloat prickle,
        jfloat rumble, jfloat wear, jfloat follow, jfloat mix)
{
    DECLARE_DSP_B
    VinylSetParam(dsp, surface, crackle, crackleSize, pops, clicks, sizzle,
                  hiss, prickle, rumble, wear, follow, mix);
    if (enable) VinylEnable(dsp); else VinylDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setExciter(JNIEnv *env, jobject obj, jlong self,
        jboolean enable, jfloat f1, jfloat f2, jfloat f3,
        jfloat a1, jfloat a2, jfloat a3, jfloat a4,
        jint character, jfloat drive, jfloat mix)
{
    DECLARE_DSP_B
    ExciterSetParam(dsp, f1, f2, f3, a1, a2, a3, a4, character, drive, mix);
    if (enable) ExciterEnable(dsp); else ExciterDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setLowEnd(JNIEnv *env, jobject obj, jlong self,
        jboolean enable, jfloat subsonic, jfloat weightHz, jfloat weightDb,
        jfloat mudHz, jfloat mudDb, jfloat mix)
{
    DECLARE_DSP_B
    LowEndSetParam(dsp, subsonic, weightHz, weightDb, mudHz, mudDb, mix);
    if (enable) LowEndEnable(dsp); else LowEndDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setTransient(JNIEnv *env, jobject obj, jlong self,
        jboolean enable, jfloat freqLow, jfloat freqHigh,
        jfloat attackLow, jfloat sustainLow, jfloat attackMid, jfloat sustainMid,
        jfloat attackHigh, jfloat sustainHigh, jfloat range, jfloat mix)
{
    DECLARE_DSP_B
    TransientSetParam(dsp, freqLow, freqHigh, attackLow, sustainLow,
                      attackMid, sustainMid, attackHigh, sustainHigh, range, mix);
    if (enable) TransientEnable(dsp); else TransientDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setImaging(JNIEnv *env, jobject obj, jlong self,
        jboolean enable, jfloat monoBelow, jfloat freqLow, jfloat freqMid, jfloat freqHigh,
        jfloat widthLow, jfloat widthMid, jfloat widthHigh, jfloat mix)
{
    DECLARE_DSP_B
    ImagingSetParam(dsp, monoBelow, freqLow, freqMid, freqHigh,
                    widthLow, widthMid, widthHigh, mix);
    if (enable) ImagingEnable(dsp); else ImagingDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setDynamicEq(JNIEnv *env, jobject obj, jlong self,
        jboolean enable, jfloat mix, jint msMode)
{
    DECLARE_DSP_B
    DynamicEqSetParam(dsp, mix, msMode);
    if (enable) DynamicEqEnable(dsp); else DynamicEqDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setDynamicEqBands(JNIEnv *env, jobject obj, jlong self,
        jfloatArray bandsObj)
{
    DECLARE_DSP_B

    // An empty band list is a legitimate state: it means the effect is present
    // but has nothing to watch, and must pass audio through untouched.
    if (bandsObj == nullptr)
    {
        DynamicEqSetBands(dsp, nullptr, 0);
        return true;
    }

    jsize len = env->GetArrayLength(bandsObj);
    if (len % DYNEQ_VALUES_PER_BAND != 0)
    {
        LOGE("JamesDspWrapper::setDynamicEqBands: expected groups of %d "
             "(frequency, q, threshold, ratio, attack, release, range, mode), "
             "got %d values", DYNEQ_VALUES_PER_BAND, (int)len);
        return false;
    }

    auto *elems = env->GetFloatArrayElements(bandsObj, nullptr);
    DynamicEqSetBands(dsp, elems, (int)(len / DYNEQ_VALUES_PER_BAND));
    env->ReleaseFloatArrayElements(bandsObj, elems, JNI_ABORT);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setPitchShift(JNIEnv *env, jobject obj, jlong self, jboolean enable, jfloat semitones, jfloat mix)
{
    DECLARE_DSP_B
    PitchShiftSetParam(dsp, semitones, mix);
    if (enable) PitchShiftEnable(dsp); else PitchShiftDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setSpectrumExtension(JNIEnv *env, jobject obj, jlong self, jboolean enable, jfloat barkFreq, jfloat strength)
{
    DECLARE_DSP_B
    SpectrumExtensionSetParam(dsp, barkFreq, strength);
    if (enable)
        SpectrumExtensionEnable(dsp);
    else
        SpectrumExtensionDisable(dsp);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setPostGain(JNIEnv *env, jobject obj, jlong self, jfloat gain)
{
    DECLARE_DSP_B
    JamesDSPSetPostGain(dsp, gain);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setMultiEqualizer(JNIEnv *env, jobject obj, jlong self,
                                                                                   jboolean enable, jint filterType, jint interpolationMode,
                                                                                   jdoubleArray bands)
{
    DECLARE_DSP_B

    if(env->GetArrayLength(bands) != 30)
    {
        LOGE("JamesDspWrapper::setMultiEqualizer: Invalid EQ data. 30 semicolon-separated fields expected, "
                      "found %d fields instead.", env->GetArrayLength(bands));
        return false;
    }

    if(bands == nullptr)
    {
        LOGW("JamesDspWrapper::setMultiEqualizer: EQ band pointer is NULL. Disabling EQ");
        MultimodalEqualizerDisable(dsp);
        return true;
    }

    if(enable)
    {
        auto* nativeBands = (env->GetDoubleArrayElements(bands, nullptr));
        MultimodalEqualizerAxisInterpolation(dsp, interpolationMode, filterType, nativeBands, nativeBands + 15);
        env->ReleaseDoubleArrayElements(bands, nativeBands, JNI_ABORT);
        MultimodalEqualizerEnable(dsp, 1);
    }
    else
    {
        MultimodalEqualizerDisable(dsp);
    }
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setVdc(JNIEnv *env, jobject obj, jlong self,
                                                                       jboolean enable, jstring vdcContents)
{
    DECLARE_DSP_B
    if(enable)
    {
        const char *nativeString = env->GetStringUTFChars(vdcContents, nullptr);
        DDCStringParser(dsp, (char*)nativeString);
        env->ReleaseStringUTFChars(vdcContents, nativeString);

        int ret = DDCEnable(dsp, 1);
        if (ret <= 0)
        {
            LOGE("JamesDspWrapper::setVdc: Call to DDCEnable(wrapper->dsp) failed. Invalid DDC parameter?");
            LOGE("JamesDspWrapper::setVdc: Disabling DDC engine");
            env->CallVoidMethod(wrapper->callbackInterface, wrapper->callbackOnVdcParseError);

            DDCDisable(dsp);
            return false;
        }
    }
    else
    {
        DDCDisable(dsp);
    }
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setCompander(JNIEnv *env, jobject obj, jlong self,
                                                                              jboolean enable, jfloat timeConstant, jint granularity, jint tfresolution, jdoubleArray bands)
{
    DECLARE_DSP_B

    if(env->GetArrayLength(bands) != 14)
    {
        LOGE("JamesDspWrapper::setCompander: Invalid compander data. 14 semicolon-separated fields expected, "
             "found %d fields instead.", env->GetArrayLength(bands));
        return false;
    }

    if(bands == nullptr)
    {
        LOGW("JamesDspWrapper::setCompander: Compander band pointer is NULL. Disabling compander");
        MultimodalEqualizerDisable(dsp);
        return true;
    }

    if(enable)
    {
        CompressorSetParam(dsp, timeConstant, granularity, tfresolution, 0);
        auto* nativeBands = (env->GetDoubleArrayElements(bands, nullptr));
        CompressorSetGain(dsp, nativeBands, nativeBands + 7, 1);
        env->ReleaseDoubleArrayElements(bands, nativeBands, JNI_ABORT);
        CompressorEnable(dsp, 1);
    }
    else
    {
        CompressorDisable(dsp);
    }
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setReverb(JNIEnv *env, jobject obj, jlong self,
                                                                          jboolean enable, jint preset)
{
    DECLARE_DSP_B
    if(enable)
    {
        Reverb_SetParam(dsp, preset);
        ReverbEnable(dsp);
    }
    else
    {
        ReverbDisable(dsp);
    }
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setConvolver(JNIEnv *env, jobject obj, jlong self,
                                                                             jboolean enable, jfloatArray impulseResponse,
                                                                             jint irChannels, jint irFrames)
{
    DECLARE_DSP_B

    int success = 1;
    if(env->GetArrayLength(impulseResponse) <= 0)
    {
        LOGW("JamesDspWrapper::setConvolver: Impulse response array is empty. Disabling convolver");
        enable = false;
    }

    if(enable)
    {
        if(irFrames <= 0)
        {
            LOGW("JamesDspWrapper::setConvolver: Impulse response has zero frames");
        }

        LOGD("JamesDspWrapper::setConvolver: Impulse response loaded: channels=%d, frames=%d", irChannels, irFrames);

        Convolver1DDisable(dsp);

        auto* nativeImpulse = (env->GetFloatArrayElements(impulseResponse, nullptr));
        success = Convolver1DLoadImpulseResponse(dsp, nativeImpulse, irChannels, irFrames, 1);
        env->ReleaseFloatArrayElements(impulseResponse, nativeImpulse, JNI_ABORT);
    }

    if(enable)
        Convolver1DEnable(dsp);
    else
        Convolver1DDisable(dsp);

    if(success <= 0)
    {
        LOGD("JamesDspWrapper::setConvolver: Failed to update convolver. Convolver1DLoadImpulseResponse returned an error.");
        return false;
    }

    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setEqPhaseMode(JNIEnv *env, jobject obj, jlong self, jboolean linearPhase)
{
    DECLARE_DSP_B
    ArbitraryResponseEqualizerSetPhaseMode(dsp, linearPhase ? 1 : 0);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setGraphicEq(JNIEnv *env, jobject obj, jlong self,
                                                                             jboolean enable, jstring graphicEq)
{
    DECLARE_DSP_B
    if(graphicEq == nullptr || env->GetStringUTFLength(graphicEq) <= 0)
    {
        LOGE("JamesDspWrapper::setGraphicEq: graphicEq is empty or NULL. Disabling graphic eq.");
        enable = false;
    }

    if(enable)
    {
        const char *nativeString = env->GetStringUTFChars(graphicEq, nullptr);
        ArbitraryResponseEqualizerStringParser(dsp, (char*)nativeString);
        env->ReleaseStringUTFChars(graphicEq, nativeString);

        ArbitraryResponseEqualizerEnable(dsp, 1);
    }
    else
        ArbitraryResponseEqualizerDisable(dsp);

    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setCrossfeed(JNIEnv *env, jobject obj, jlong self,
                                                                             jboolean enable, jint mode, jint customFcut, jint customFeed)
{
    DECLARE_DSP_B
    if(mode == 99)
    {
        memset(&dsp->advXF.bs2b, 0, sizeof(dsp->advXF.bs2b));
        BS2BInit(&dsp->advXF.bs2b[1], (unsigned int)dsp->fs, ((unsigned int)customFcut | ((unsigned int)customFeed << 16)));
        dsp->advXF.mode = 1;
    }
    else
    {
       CrossfeedChangeMode(dsp, mode);
    }

    if(enable)
        CrossfeedEnable(dsp, 1);
    else
        CrossfeedDisable(dsp);

    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setBassBoost(JNIEnv *env, jobject obj, jlong self,
                                                                             jboolean enable, jfloat maxGain)
{
    DECLARE_DSP_B
    if(enable)
    {
        BassBoostSetParam(dsp, maxGain);
        BassBoostEnable(dsp);
    }
    else
    {
        BassBoostDisable(dsp);
    }
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setStereoEnhancement(JNIEnv *env, jobject obj, jlong self,
                                                                                     jboolean enable, jfloat level)
{
    DECLARE_DSP_B
    StereoEnhancementDisable(dsp);
    StereoEnhancementSetParam(dsp, level / 100.0f);
    if(enable)
    {
        StereoEnhancementEnable(dsp);
    }
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setVacuumTube(JNIEnv *env, jobject obj, jlong self,
                                                                              jboolean enable, jfloat level)
{
    DECLARE_DSP_B
    if(enable)
    {
        VacuumTubeSetGain(dsp, level);
        VacuumTubeEnable(dsp);
    }
    else
    {
        VacuumTubeDisable(dsp);
    }
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setLiveprogSlot(JNIEnv *env, jobject obj, jlong self,
                                                                                jint slot, jboolean enable, jstring id, jstring liveprogContent)
{
    DECLARE_DSP_B

    if (slot < 1 || slot > 3)
        return false;

    setStdOutHandler(receiveLiveprogStdOut, wrapper);
    LiveProgDisableSlot(dsp, slot);

    if (!enable)
        return true;

    const char *nativeString = env->GetStringUTFChars(liveprogContent, nullptr);
    if (strlen(nativeString) < 1) {
        LOGD("JamesDspWrapper::setLiveprogSlot: empty file")
        env->ReleaseStringUTFChars(liveprogContent, nativeString);
        return true;
    }

    env->CallVoidMethod(wrapper->callbackInterface, wrapper->callbackOnLiveprogExec, id);

    int ret = LiveProgStringParserSlot(dsp, slot, (char*)nativeString);
    env->ReleaseStringUTFChars(liveprogContent, nativeString);

    // Workaround due to library bug (mirrors setLiveprog)
    jdsp_unlock(dsp);

    if (ret <= 0)
    {
        LOGW("JamesDspWrapper::setLiveprogSlot: failed to compile script for slot %d (code %d)", slot, ret)
        return false;
    }

    // Without this the slot holds a compiled script but never runs
    LiveProgEnableSlot(dsp, slot);
    return true;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_setLiveprog(JNIEnv *env, jobject obj, jlong self,
                                                                            jboolean enable, jstring id, jstring liveprogContent)
{
    DECLARE_DSP_B

    // Attach log listener
    setStdOutHandler(receiveLiveprogStdOut, wrapper);

    LiveProgDisable(dsp);

    const char *nativeString = env->GetStringUTFChars(liveprogContent, nullptr);
    if(strlen(nativeString) < 1) {
        LOGD("JamesDspWrapper::setLiveprog: empty file")
        env->ReleaseStringUTFChars(liveprogContent, nativeString);
        return true;
    }

    env->CallVoidMethod(wrapper->callbackInterface, wrapper->callbackOnLiveprogExec, id);

    int ret = LiveProgStringParser(dsp, (char*)nativeString); // Ignore constness, libjamesdsp does not modify it
    env->ReleaseStringUTFChars(liveprogContent, nativeString);

    // Workaround due to library bug
    jdsp_unlock(dsp);

    const char* errorString = NSEEL_code_getcodeerror(dsp->eel.vm);
    if(errorString != nullptr)
    {
        LOGW("JamesDspWrapper::setLiveprog: NSEEL_code_getcodeerror: Syntax error in script file, cannot load. Reason: %s", errorString);
    }
    if(ret <= 0)
    {
        LOGW("JamesDspWrapper::setLiveprog: %s", checkErrorCode(ret));
    }

    jstring errorStringJni = env->NewStringUTF(errorString);
    env->CallVoidMethod(wrapper->callbackInterface, wrapper->callbackOnLiveprogResult, ret, id, errorStringJni);
    env->DeleteLocalRef(errorStringJni);

    if(enable)
        LiveProgEnable(dsp);
    else
        LiveProgDisable(dsp);
    return true;
}


extern "C" JNIEXPORT jobject JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_enumerateEelVariables(JNIEnv *env, jobject obj, jlong self)
{
    auto array = JArrayList(env);

    // Return empty array if DECLARE failed
    DECLARE_DSP(array.getJavaReference())

    auto *ctx = (compileContext*)dsp->eel.vm;
    for (int i = 0; i < ctx->varTable_numBlocks; i++)
    {
        for (int j = 0; j < NSEEL_VARS_PER_BLOCK; j++)
        {
            // TODO fix string handling (broke after last libjamesdsp update)
            const char *valid = nullptr;//(char*)GetStringForIndex(ctx->region_context, ctx->varTable_Values[i][j], 1);
            bool isString = valid;

            if (ctx->varTable_Names[i][j])
            {
                const char* name = ctx->varTable_Names[i][j];
                const char* value;

                if(isString)
                    value = valid;
                else
                    value = std::to_string(ctx->varTable_Values[i][j]).c_str();

                auto var = EelVmVariable(env, name, value, isString);
                array.add(var.getJavaReference());
            }
        }
    }

    return array.getJavaReference();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_manipulateEelVariable(JNIEnv *env, jobject obj, jlong self,
                                                                                      jstring name, jfloat value)
{
    DECLARE_DSP_B
    auto* ctx = (compileContext*)dsp->eel.vm;
    for (int i = 0; i < ctx->varTable_numBlocks; i++)
    {
        for (int j = 0; j < NSEEL_VARS_PER_BLOCK; j++)
        {
            const char *nativeName = env->GetStringUTFChars(name, nullptr);
            if(!ctx->varTable_Names[i][j] || std::strcmp(ctx->varTable_Names[i][j], nativeName) != 0)
            {
                env->ReleaseStringUTFChars(name, nativeName);
                continue;
            }

            const char *valid = nullptr;//(char*)GetStringForIndex(ctx->region_context, ctx->varTable_Values[i][j], 1);
            if(valid)
            {
                LOGE("JamesDspWrapper::manipulateEelVariable: variable '%s' is a string; currently only numerical variables can be manipulated", nativeName);
                env->ReleaseStringUTFChars(name, nativeName);
                return false;
            }

            ctx->varTable_Values[i][j] = value;

            env->ReleaseStringUTFChars(name, nativeName);
            return true;
        }
    }

    const char *nativeName = env->GetStringUTFChars(name, nullptr);
    LOGE("JamesDspWrapper::manipulateEelVariable: variable '%s' not found", nativeName);
    env->ReleaseStringUTFChars(name, nativeName);
    return false;
}

extern "C" JNIEXPORT void JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_freezeLiveprogExecution(JNIEnv *env, jobject obj, jlong self,
                                                                                        jboolean freeze)
{
    DECLARE_DSP_V
    dsp->eel.active = !freeze;
    LOGD("JamesDspWrapper::freezeLiveprogExecution: Liveprog execution has been %s", (freeze ? "frozen" : "resumed"));
}

extern "C" JNIEXPORT jstring JNICALL
Java_me_timschneeberger_rootlessjamesdsp_interop_JamesDspWrapper_eelErrorCodeToString(JNIEnv *env,
                                                                                     jobject obj,
                                                                                     jint error_code)
{
    return env->NewStringUTF(checkErrorCode(error_code));
}

void receiveLiveprogStdOut(const char *buffer, void* userData)
{
    auto* self = static_cast<JamesDspWrapper*>(userData);
    if(self == nullptr)
    {
        LOGE("JamesDspWrapper::receiveLiveprogStdOut: Self reference is NULL");
        LOGE("JamesDspWrapper::receiveLiveprogStdOut: Unhandled output: %s", buffer);
        return;
    }

    self->env->CallVoidMethod(self->callbackInterface, self->callbackOnLiveprogOutput, self->env->NewStringUTF(buffer));
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *, void *)
{
#ifndef NO_CRASHLYTICS
    firebase::crashlytics::Initialize();
#endif
    LOGD("JNI_OnLoad called")
    return JNI_VERSION_1_6;
}
