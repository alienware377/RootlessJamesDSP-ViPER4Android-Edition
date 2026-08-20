package me.timschneeberger.rootlessjamesdsp.interop

import me.timschneeberger.rootlessjamesdsp.interop.structure.EelVmVariable
import me.timschneeberger.rootlessjamesdsp.model.ProcessorMessage

typealias JamesDspHandle = Long

object JamesDspWrapper {
    // Memory management
    external fun alloc(callbacks: JamesDspCallbacks): JamesDspHandle
    external fun free(self: JamesDspHandle)
    external fun isHandleValid(self: JamesDspHandle): Boolean

    // Benchmarking
    external fun getBenchmarkSize(): Int
    external fun runBenchmark(c0: DoubleArray, c1: DoubleArray)
    external fun loadBenchmark(c0: DoubleArray, c1: DoubleArray)

    // Processing (interleaved)
    external fun processInt16(self: JamesDspHandle, input: ShortArray, output: ShortArray, offset: Int = -1, length: Int = -1)
    external fun processInt8U24(self: JamesDspHandle, input: IntArray): IntArray
    external fun processInt24Packed(self: JamesDspHandle, input: BooleanArray): BooleanArray
    external fun processInt32(self: JamesDspHandle, input: IntArray, output: IntArray, offset: Int = -1, length: Int = -1)
    external fun processFloat(self: JamesDspHandle, input: FloatArray, output: FloatArray, offset: Int = -1, length: Int = -1)

    // Engine config
    external fun setSamplingRate(self: JamesDspHandle, sampleRate: Float, forceRefresh: Boolean)

    // Effect config
    external fun setLimiter(self: JamesDspHandle, threshold: Float, release: Float): Boolean
    external fun setLimiterMode(self: JamesDspHandle, mode: Int): Boolean
    external fun setBassExciter(self: JamesDspHandle, enable: Boolean, cutoff: Float, intensity: Float, mix: Float, band2: Boolean, cutoff2: Float, intensity2: Float, mix2: Float): Boolean
    external fun setVDynBass(self: JamesDspHandle, enable: Boolean, gain: Float, x1: Float, x2: Float, y1: Float, y2: Float, sgx: Float, sgy: Float): Boolean
    external fun setDiffSurround(self: JamesDspHandle, enable: Boolean, delayLms: Float, delayRms: Float): Boolean
    external fun setViperClarity(self: JamesDspHandle, enable: Boolean, mode: Int, gain: Float): Boolean
    external fun setFieldSurround(self: JamesDspHandle, enable: Boolean, strength: Float, midImage: Float): Boolean
    external fun setAgc(self: JamesDspHandle, enable: Boolean, target: Float, maxBoost: Float): Boolean
    external fun setHpSurround(self: JamesDspHandle, enable: Boolean, strength: Float, room: Float): Boolean
    external fun setFetComp(self: JamesDspHandle, enable: Boolean, threshold: Float, ratio: Float, attack: Float, release: Float, makeup: Float): Boolean
    external fun setCure(self: JamesDspHandle, enable: Boolean, level: Int): Boolean
    external fun setViperBass(self: JamesDspHandle, enable: Boolean, mode: Int, freq: Float, gain: Float): Boolean
    external fun setVReverb(self: JamesDspHandle, enable: Boolean, model: Int, room: Float,
                            damp: Float, width: Float, predelay: Float, decay: Float,
                            diffusion: Float, mod: Float, bass: Float, er: Float,
                            wet: Float, dry: Float): Boolean
    external fun setSpeakerOpt(self: JamesDspHandle, enable: Boolean, strength: Float): Boolean
    external fun setMaximizer(self: JamesDspHandle, enable: Boolean, mode: Int, gain: Float, ceiling: Float, release: Float, character: Float, transient: Float, truePeak: Boolean, stereoLink: Float, oversample: Int, clipShape: Int): Boolean
    external fun setMultibandDist(self: JamesDspHandle, enable: Boolean, routing: Int, model: Int, drive: Float, bias: Float, shape: Float, bits: Float, downsample: Float, tone: Float, bandGain: Float, chorusRate: Float, chorusDepth: Float, chorusFeedback: Float, chorusSpread: Float, chorusVoices: Int, chorusMix: Float, mix: Float): Boolean
    external fun setMultibandDistBands(self: JamesDspHandle, bands: FloatArray?): Boolean
    external fun setEchoDelay(self: JamesDspHandle, enable: Boolean, input: Float, time: Float, smoothing: Float, offset: Float, keepPitch: Boolean, model: Int, stereo: Float, feedback: Float, cutoff: Float, res: Float, filter: Int, smpRate: Float, bits: Float, modRate: Float, modTime: Float, modCutoff: Float, diffusion: Float, spread: Float, distMode: Int, distLevel: Float, knee: Float, symmetry: Float, tone: Float, wet: Float, dry: Float): Boolean
    external fun setLowEnd(self: JamesDspHandle, enable: Boolean, subsonic: Float, weightHz: Float, weightDb: Float, mudHz: Float, mudDb: Float, mix: Float): Boolean
    external fun setTransient(self: JamesDspHandle, enable: Boolean, freqLow: Float, freqHigh: Float, attackLow: Float, sustainLow: Float, attackMid: Float, sustainMid: Float, attackHigh: Float, sustainHigh: Float, range: Float, mix: Float): Boolean
    external fun setImaging(self: JamesDspHandle, enable: Boolean, monoBelow: Float, freqLow: Float, freqMid: Float, freqHigh: Float, widthLow: Float, widthMid: Float, widthHigh: Float, mix: Float): Boolean
    external fun setDynamicEq(self: JamesDspHandle, enable: Boolean, mix: Float): Boolean
    external fun setDynamicEqBands(self: JamesDspHandle, bands: FloatArray?): Boolean
    external fun setPitchShift(self: JamesDspHandle, enable: Boolean, semitones: Float, mix: Float): Boolean
    external fun setChainOrder(self: JamesDspHandle, order: IntArray?): Boolean
    external fun setSpectrumExtension(self: JamesDspHandle, enable: Boolean, barkFreq: Float, strength: Float): Boolean
    external fun initCrashGuard(path: String)
    external fun setPostGain(self: JamesDspHandle, postGain: Float): Boolean
    external fun setMultiEqualizer(self: JamesDspHandle, enable: Boolean, filterType: Int, interpolationMode: Int, bands: DoubleArray): Boolean
    external fun setVdc(self: JamesDspHandle, enable: Boolean, vdcContents: String): Boolean
    external fun setCompander(self: JamesDspHandle, enable: Boolean, timeConstant: Float, granularity: Int, tfResolution: Int, bands: DoubleArray): Boolean
    external fun setReverb(self: JamesDspHandle, enable: Boolean, preset: Int): Boolean
    external fun setConvolver(self: JamesDspHandle, enable: Boolean, impulseResponse: FloatArray, irChannels: Int, irFrames: Int): Boolean
    external fun setEqPhaseMode(self: JamesDspHandle, linearPhase: Boolean): Boolean

    external fun setGraphicEq(self: JamesDspHandle, enable: Boolean, graphicEq: String): Boolean
    external fun setCrossfeed(self: JamesDspHandle, enable: Boolean, mode: Int, customFcut: Int, customFeed: Int): Boolean
    external fun setBassBoost(self: JamesDspHandle, enable: Boolean, maxGain: Float): Boolean
    external fun setStereoEnhancement(self: JamesDspHandle, enable: Boolean, level: Float): Boolean
    external fun setVacuumTube(self: JamesDspHandle, enable: Boolean, level: Float): Boolean
    external fun setLiveprog(self: JamesDspHandle, enable: Boolean, id: String, liveprogContent: String): Boolean
    external fun setLiveprogSlot(self: JamesDspHandle, slot: Int, enable: Boolean, id: String, script: String): Boolean

    // EEL VM utilities
    external fun enumerateEelVariables(self: JamesDspHandle): ArrayList<EelVmVariable>
    external fun manipulateEelVariable(self: JamesDspHandle, name: String, value: Float): Boolean
    external fun freezeLiveprogExecution(self: JamesDspHandle, freeze: Boolean)
    external fun eelErrorCodeToString(errorCode: Int): String

    // Callbacks
    interface JamesDspCallbacks
    {
        fun onLiveprogOutput(message: String)
        fun onLiveprogExec(id: String)
        fun onLiveprogResult(resultCode: Int, id: String, errorMessage: String?)
        fun onVdcParseError()
        fun onConvolverParseError(errorCode: ProcessorMessage.ConvolverErrorCode)
    }

    init
    {
        System.loadLibrary("jamesdsp-wrapper")
    }
}