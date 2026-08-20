package me.timschneeberger.rootlessjamesdsp.interop

import android.content.Context
import android.content.Intent
import me.timschneeberger.rootlessjamesdsp.interop.structure.EelVmVariable
import me.timschneeberger.rootlessjamesdsp.utils.Constants
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.sendLocalBroadcast
import timber.log.Timber
import java.util.Timer
import kotlin.concurrent.schedule

class JamesDspLocalEngine(context: Context, callbacks: JamesDspWrapper.JamesDspCallbacks? = null) : JamesDspBaseEngine(context, callbacks) {
    var handle: JamesDspHandle = JamesDspWrapper.alloc(callbacks ?: DummyCallbacks())

    override var sampleRate: Float
        set(value) {
            super.sampleRate = value
            JamesDspWrapper.setSamplingRate(handle, value, false)
            context.sendLocalBroadcast(Intent(Constants.ACTION_SAMPLE_RATE_UPDATED))
        }
        get() = super.sampleRate
    override var enabled: Boolean = true

    init {
        if(BenchmarkManager.hasBenchmarksCached())
            BenchmarkManager.loadBenchmarksFromCache()
    }

    override fun close() {
        val oldHandle = handle
        handle = 0

        // Make sure ongoing async calls to native have enough time to finish
        Timer().schedule(100) {
            JamesDspWrapper.free(oldHandle)
            Timber.d("Handle $oldHandle has been freed")
        }
    }

    // Processing
    fun processInt16(input: ShortArray, output: ShortArray, offset: Int = -1, length: Int = -1)
    {
        if(!enabled || handle == 0L)
        {
            if(offset < 0 && length < 0) {
                input.copyInto(output)
            }
            else {
                input.copyInto(output, 0, offset, offset + length)
            }
        }
        else {
            JamesDspWrapper.processInt16(handle, input, output, offset, length)
        }
    }

    fun processInt32(input: IntArray, output: IntArray, offset: Int = -1, length: Int = -1)
    {
        if(!enabled || handle == 0L)
        {
            if(offset < 0 && length < 0) {
                input.copyInto(output)
            }
            else {
                input.copyInto(output, 0, offset, offset + length)
            }
        }
        else {
            JamesDspWrapper.processInt32(handle, input, output, offset, length)
        }
    }

    fun processFloat(input: FloatArray, output: FloatArray, offset: Int = -1, length: Int = -1)
    {
        if(!enabled || handle == 0L)
        {
            if(offset < 0 && length < 0) {
                input.copyInto(output)
            }
            else {
                input.copyInto(output, 0, offset, offset + length)
            }
        }
        else {
            JamesDspWrapper.processFloat(handle, input, output, offset, length)
            if (++processCounter % 2000L == 0L)
                CrashBreadcrumb.mark(context, "audio alive")
        }
    }

    private var processCounter = 0L

    // Effect config
    override fun setOutputControl(threshold: Float, release: Float, postGain: Float, limiterMode: Int): Boolean {
        return JamesDspWrapper.setLimiter(handle, threshold, release) and
                JamesDspWrapper.setLimiterMode(handle, limiterMode) and
                JamesDspWrapper.setPostGain(handle, postGain)
    }

    override fun setBassExciter(enable: Boolean, cutoff: Float, intensity: Float, mix: Float, band2: Boolean, cutoff2: Float, intensity2: Float, mix2: Float): Boolean {
        return JamesDspWrapper.setBassExciter(handle, enable, cutoff, intensity, mix, band2, cutoff2, intensity2, mix2)
    }

    override fun setVDynBass(enable: Boolean, gain: Float, x1: Float, x2: Float, y1: Float, y2: Float, sgx: Float, sgy: Float): Boolean {
        return JamesDspWrapper.setVDynBass(handle, enable, gain, x1, x2, y1, y2, sgx, sgy)
    }

    override fun setDiffSurround(enable: Boolean, delayLms: Float, delayRms: Float): Boolean {
        return JamesDspWrapper.setDiffSurround(handle, enable, delayLms, delayRms)
    }

    override fun setViperClarity(enable: Boolean, mode: Int, gain: Float): Boolean {
        return JamesDspWrapper.setViperClarity(handle, enable, mode, gain)
    }

    override fun setFieldSurround(enable: Boolean, strength: Float, midImage: Float): Boolean {
        return JamesDspWrapper.setFieldSurround(handle, enable, strength, midImage)
    }

    override fun setAgc(enable: Boolean, target: Float, maxBoost: Float): Boolean {
        return JamesDspWrapper.setAgc(handle, enable, target, maxBoost)
    }

    override fun setHpSurround(enable: Boolean, strength: Float, room: Float): Boolean {
        return JamesDspWrapper.setHpSurround(handle, enable, strength, room)
    }

    override fun setFetComp(enable: Boolean, threshold: Float, ratio: Float, attack: Float, release: Float, makeup: Float): Boolean {
        return JamesDspWrapper.setFetComp(handle, enable, threshold, ratio, attack, release, makeup)
    }

    override fun setCure(enable: Boolean, level: Int): Boolean {
        return JamesDspWrapper.setCure(handle, enable, level)
    }

    override fun setViperBass(enable: Boolean, mode: Int, freq: Float, gain: Float): Boolean {
        return JamesDspWrapper.setViperBass(handle, enable, mode, freq, gain)
    }

    override fun setVReverb(enable: Boolean, model: Int, room: Float, damp: Float,
                            width: Float, predelay: Float, decay: Float, diffusion: Float,
                            mod: Float, bass: Float, er: Float, wet: Float, dry: Float): Boolean {
        return JamesDspWrapper.setVReverb(handle, enable, model, room, damp, width,
            predelay, decay, diffusion, mod, bass, er, wet, dry)
    }

    override fun setSpeakerOpt(enable: Boolean, strength: Float): Boolean {
        return JamesDspWrapper.setSpeakerOpt(handle, enable, strength)
    }

    override fun setMaximizer(enable: Boolean, mode: Int, gain: Float, ceiling: Float, release: Float, character: Float, transient: Float, truePeak: Boolean, stereoLink: Float, oversample: Int, clipShape: Int): Boolean =
        JamesDspWrapper.setMaximizer(handle, enable, mode, gain, ceiling, release, character, transient, truePeak, stereoLink, oversample, clipShape)

    override fun setTape(enable: Boolean, wow: Float, flutter: Float, saturation: Float, bias: Float, headBump: Float, mix: Float): Boolean =
        JamesDspWrapper.setTape(handle, enable, wow, flutter, saturation, bias, headBump, mix)

    override fun setExciter(enable: Boolean, f1: Float, f2: Float, f3: Float, a1: Float, a2: Float, a3: Float, a4: Float, character: Int, drive: Float, mix: Float): Boolean =
        JamesDspWrapper.setExciter(handle, enable, f1, f2, f3, a1, a2, a3, a4, character, drive, mix)

    override fun setLowEnd(enable: Boolean, subsonic: Float, weightHz: Float, weightDb: Float, mudHz: Float, mudDb: Float, mix: Float): Boolean =
        JamesDspWrapper.setLowEnd(handle, enable, subsonic, weightHz, weightDb, mudHz, mudDb, mix)

    override fun setTransient(enable: Boolean, freqLow: Float, freqHigh: Float, attackLow: Float, sustainLow: Float, attackMid: Float, sustainMid: Float, attackHigh: Float, sustainHigh: Float, range: Float, mix: Float): Boolean =
        JamesDspWrapper.setTransient(handle, enable, freqLow, freqHigh, attackLow, sustainLow, attackMid, sustainMid, attackHigh, sustainHigh, range, mix)

    override fun setImaging(enable: Boolean, monoBelow: Float, freqLow: Float, freqMid: Float, freqHigh: Float, widthLow: Float, widthMid: Float, widthHigh: Float, mix: Float): Boolean =
        JamesDspWrapper.setImaging(handle, enable, monoBelow, freqLow, freqMid, freqHigh, widthLow, widthMid, widthHigh, mix)

    override fun setDynamicEqBandsInternal(bands: FloatArray?): Boolean =
        JamesDspWrapper.setDynamicEqBands(handle, bands)

    override fun setDynamicEq(enable: Boolean, mix: Float, msMode: Int): Boolean =
        JamesDspWrapper.setDynamicEq(handle, enable, mix, msMode)

    override fun setMultibandDistBandsInternal(bands: FloatArray?): Boolean =
        JamesDspWrapper.setMultibandDistBands(handle, bands)

    override fun setMultibandDist(enable: Boolean, routing: Int, model: Int, drive: Float, bias: Float, shape: Float, bits: Float, downsample: Float, tone: Float, bandGain: Float, chorusRate: Float, chorusDepth: Float, chorusFeedback: Float, chorusSpread: Float, chorusVoices: Int, chorusMix: Float, mix: Float): Boolean =
        JamesDspWrapper.setMultibandDist(handle, enable, routing, model, drive, bias, shape, bits, downsample, tone, bandGain, chorusRate, chorusDepth, chorusFeedback, chorusSpread, chorusVoices, chorusMix, mix)

    override fun setEchoDelay(enable: Boolean, input: Float, time: Float, smoothing: Float, offset: Float, keepPitch: Boolean, model: Int, stereo: Float, feedback: Float, cutoff: Float, res: Float, filter: Int, smpRate: Float, bits: Float, modRate: Float, modTime: Float, modCutoff: Float, diffusion: Float, spread: Float, distMode: Int, distLevel: Float, knee: Float, symmetry: Float, tone: Float, wet: Float, dry: Float): Boolean {
        return JamesDspWrapper.setEchoDelay(handle, enable, input, time, smoothing, offset, keepPitch, model, stereo, feedback, cutoff, res, filter, smpRate, bits, modRate, modTime, modCutoff, diffusion, spread, distMode, distLevel, knee, symmetry, tone, wet, dry)
    }

    override fun setPitchShift(enable: Boolean, semitones: Float, mix: Float): Boolean {
        return JamesDspWrapper.setPitchShift(handle, enable, semitones, mix)
    }

    override fun setChainOrder(order: IntArray?): Boolean {
        return JamesDspWrapper.setChainOrder(handle, order)
    }

    override fun setSpectrumExtension(enable: Boolean, barkFreq: Float, strength: Float): Boolean {
        return JamesDspWrapper.setSpectrumExtension(handle, enable, barkFreq, strength)
    }

    override fun setReverb(enable: Boolean, preset: Int): Boolean
    {
        return JamesDspWrapper.setReverb(handle, enable, preset)
    }

    override fun setCrossfeed(enable: Boolean, mode: Int): Boolean
    {
        return JamesDspWrapper.setCrossfeed(handle, enable, mode, 0, 0)
    }

    override fun setCrossfeedCustom(enable: Boolean, fcut: Int, feed: Int): Boolean
    {
        return JamesDspWrapper.setCrossfeed(handle, enable, 99, fcut, feed)
    }

    override fun setBassBoost(enable: Boolean, maxGain: Float): Boolean
    {
        return JamesDspWrapper.setBassBoost(handle, enable, maxGain)
    }

    override fun setStereoEnhancement(enable: Boolean, level: Float): Boolean
    {
        return JamesDspWrapper.setStereoEnhancement(handle, enable, level)
    }

    override fun setVacuumTube(enable: Boolean, level: Float): Boolean
    {
        return JamesDspWrapper.setVacuumTube(handle, enable, level)
    }

    override fun setMultiEqualizerInternal(
        enable: Boolean,
        filterType: Int,
        interpolationMode: Int,
        bands: DoubleArray
    ): Boolean {
        return JamesDspWrapper.setMultiEqualizer(handle, enable, filterType, interpolationMode, bands)
    }

    override fun setCompanderInternal(
        enable: Boolean,
        timeConstant: Float,
        granularity: Int,
        tfTransforms: Int,
        bands: DoubleArray
    ): Boolean {
        return JamesDspWrapper.setCompander(handle, enable, timeConstant, granularity, tfTransforms, bands)
    }

    override fun setVdcInternal(enable: Boolean, vdc: String): Boolean {
        return JamesDspWrapper.setVdc(handle, enable, vdc)
    }

    override fun setConvolverInternal(
        enable: Boolean,
        impulseResponse: FloatArray,
        irChannels: Int,
        irFrames: Int,
        irCrc: Int,
        irSampleRate: Int,
    ): Boolean {
        return JamesDspWrapper.setConvolver(handle, enable, impulseResponse, irChannels, irFrames)
    }

    override fun setEqPhaseMode(linearPhase: Boolean): Boolean =
        JamesDspWrapper.setEqPhaseMode(handle, linearPhase)

    override fun setGraphicEqInternal(enable: Boolean, bands: String): Boolean {
        return JamesDspWrapper.setGraphicEq(handle, enable, bands)
    }

    override fun setLiveprogInternal(enable: Boolean, name: String, script: String): Boolean {
        return JamesDspWrapper.setLiveprog(handle, enable, name, script)
    }

    override fun setLiveprogSlotInternal(slot: Int, enable: Boolean, name: String, script: String): Boolean {
        return JamesDspWrapper.setLiveprogSlot(handle, slot, enable, name, script)
    }

    // Feature support
    override fun supportsEelVmAccess(): Boolean { return true }
    override fun supportsCustomCrossfeed(): Boolean { return true }

    // EEL VM utilities
    override fun enumerateEelVariables(): ArrayList<EelVmVariable>
    {
        return JamesDspWrapper.enumerateEelVariables(handle)
    }

    override fun manipulateEelVariable(name: String, value: Float): Boolean
    {
        return JamesDspWrapper.manipulateEelVariable(handle, name, value)
    }

    override fun freezeLiveprogExecution(freeze: Boolean)
    {
        JamesDspWrapper.freezeLiveprogExecution(handle, freeze)
    }
}
