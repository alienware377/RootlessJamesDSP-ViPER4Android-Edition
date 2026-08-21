package me.timschneeberger.rootlessjamesdsp.interop

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.media.audiofx.AudioEffect
import android.media.audiofx.AudioEffectHidden
import me.timschneeberger.rootlessjamesdsp.MainApplication
import me.timschneeberger.rootlessjamesdsp.interop.structure.EelVmVariable
import me.timschneeberger.rootlessjamesdsp.utils.Constants
import me.timschneeberger.rootlessjamesdsp.utils.extensions.AudioEffectExtensions.getParameterInt
import me.timschneeberger.rootlessjamesdsp.utils.extensions.AudioEffectExtensions.setParameter
import me.timschneeberger.rootlessjamesdsp.utils.extensions.AudioEffectExtensions.setParameterCharBuffer
import me.timschneeberger.rootlessjamesdsp.utils.extensions.AudioEffectExtensions.setParameterFloatArray
import me.timschneeberger.rootlessjamesdsp.utils.extensions.AudioEffectExtensions.setParameterIntArray
import me.timschneeberger.rootlessjamesdsp.utils.extensions.AudioEffectExtensions.setParameterImpulseResponseBuffer
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.registerLocalReceiver
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.showAlert
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.toast
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.unregisterLocalReceiver
import me.timschneeberger.rootlessjamesdsp.utils.extensions.crc
import me.timschneeberger.rootlessjamesdsp.utils.extensions.toShort
import timber.log.Timber
import java.util.UUID
import kotlin.math.roundToInt

class JamesDspRemoteEngine(
    context: Context,
    val sessionId: Int,
    val priority: Int,
    callbacks: JamesDspWrapper.JamesDspCallbacks? = null,
) : JamesDspBaseEngine(context, callbacks) {

    private var convolverSampleRate = 0

    private val broadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                Constants.ACTION_SAMPLE_RATE_UPDATED -> syncWithPreferences(arrayOf(Constants.PREF_CONVOLVER))
                Constants.ACTION_PREFERENCES_UPDATED -> syncWithPreferences()
                Constants.ACTION_SERVICE_RELOAD_LIVEPROG -> syncWithPreferences(arrayOf(Constants.PREF_LIVEPROG, Constants.PREF_LIVEPROG2, Constants.PREF_LIVEPROG3, Constants.PREF_LIVEPROG4))
                Constants.ACTION_SERVICE_HARD_REBOOT_CORE -> rebootEngine()
                Constants.ACTION_SERVICE_SOFT_REBOOT_CORE -> { clearCache(); syncWithPreferences() }
            }
        }
    }

    var effect: AudioEffectHidden? = createEffect()

    override var enabled: Boolean
        set(value) { effect?.enabled = value }
        get() = effect?.enabled ?: false

    override var sampleRate: Float
        get() {
            super.sampleRate = effect.getParameterInt(20001)?.toFloat() ?: -0f
            return super.sampleRate
        }
        set(_){}

    init {
        syncWithPreferences()

        val filter = IntentFilter()
        filter.addAction(Constants.ACTION_PREFERENCES_UPDATED)
        filter.addAction(Constants.ACTION_SAMPLE_RATE_UPDATED)
        filter.addAction(Constants.ACTION_SERVICE_RELOAD_LIVEPROG)
        filter.addAction(Constants.ACTION_SERVICE_HARD_REBOOT_CORE)
        filter.addAction(Constants.ACTION_SERVICE_SOFT_REBOOT_CORE)
        context.registerLocalReceiver(broadcastReceiver, filter)
    }

    private fun createEffect(): AudioEffectHidden {
        return try {
            AudioEffectHidden(EFFECT_TYPE_CUSTOM, EFFECT_JAMESDSP, priority, sessionId)
        } catch (e: Exception) {
            Timber.e("Failed to create JamesDSP effect")
            Timber.e(e)
            throw IllegalStateException(e)
        }
    }

    private fun checkEngine() {
        if (!isPidValid) {
            Timber.e("PID ($pid) for session $sessionId invalid. Engine probably crashed or detached.")
            context.toast("Engine crashed. Rebooting JamesDSP.", false)
            rebootEngine()
        }

        if (isSampleRateAbnormal) {
            Timber.e("PID ($pid) for session $sessionId invalid. Engine crashed.")
            context.toast("Abnormal sampling rate. Rebooting JamesDSP.", false)
            rebootEngine()
        }
    }

    private fun rebootEngine() {
        try {
            effect?.release()
            effect = createEffect()
        }
        catch (ex: IllegalStateException) {
            Timber.e("Failed to re-instantiate JamesDSP effect")
            Timber.e(ex.cause)
            effect = null
            return
        }
    }

    override fun syncWithPreferences(forceUpdateNamespaces: Array<String>?) {
        if(effect == null) {
            Timber.d("Rejecting update due to disposed engine")
            return
        }

        checkEngine()
        super.syncWithPreferences(forceUpdateNamespaces)
    }

    override fun close() {
        context.unregisterLocalReceiver(broadcastReceiver)
        effect?.release()
        effect = null
        super.close()
    }

    override fun setBassExciter(enable: Boolean, cutoff: Float, intensity: Float, mix: Float, band2: Boolean, cutoff2: Float, intensity2: Float, mix2: Float): Boolean {
        // Bass exciter is unsupported in root/plugin mode (remote engine)
        return true
    }

    override fun setVDynBass(enable: Boolean, gain: Float, x1: Float, x2: Float, y1: Float, y2: Float, sgx: Float, sgy: Float): Boolean {
        // ViPER dynamic bass is unsupported in root/plugin mode (remote engine)
        return true
    }

    override fun setDiffSurround(enable: Boolean, delayLms: Float, delayRms: Float): Boolean {
        // Differential surround is unsupported in root/plugin mode (remote engine)
        return true
    }

    override fun setViperClarity(enable: Boolean, mode: Int, gain: Float): Boolean =
        sendForkEffect(PARAM_CLARITY, enable, floatArrayOf(mode.toFloat(), gain))

    override fun setFieldSurround(enable: Boolean, strength: Float, midImage: Float): Boolean =
        sendForkEffect(PARAM_FIELD_SURROUND, enable, floatArrayOf(strength, midImage))

    override fun setAgc(enable: Boolean, target: Float, maxBoost: Float): Boolean =
        sendForkEffect(PARAM_AGC, enable, floatArrayOf(target, maxBoost))

    override fun setHpSurround(enable: Boolean, strength: Float, room: Float): Boolean =
        sendForkEffect(PARAM_HP_SURROUND, enable, floatArrayOf(strength, room))

    override fun setFetComp(enable: Boolean, threshold: Float, ratio: Float, attack: Float, release: Float, makeup: Float): Boolean =
        sendForkEffect(PARAM_FET_COMP, enable, floatArrayOf(threshold, ratio, attack, release, makeup))

    override fun setCure(enable: Boolean, level: Int): Boolean =
        sendForkEffect(PARAM_CURE, enable, floatArrayOf(level.toFloat()))

    override fun setViperBass(enable: Boolean, mode: Int, freq: Float, gain: Float): Boolean =
        sendForkEffect(PARAM_VIPER_BASS, enable, floatArrayOf(mode.toFloat(), freq, gain))

    override fun setVReverb(enable: Boolean, model: Int, room: Float, damp: Float,
                            width: Float, predelay: Float, decay: Float, diffusion: Float,
                            mod: Float, bass: Float, er: Float, wet: Float, dry: Float): Boolean =
        sendForkEffect(PARAM_VREVERB, enable, floatArrayOf(
            model.toFloat(), room, damp, width, predelay, decay,
            diffusion, mod, bass, er, wet, dry))

    override fun setSpeakerOpt(enable: Boolean, strength: Float): Boolean =
        sendForkEffect(PARAM_SPEAKER_OPT, enable, floatArrayOf(strength))

    override fun setMaximizer(enable: Boolean, mode: Int, gain: Float, ceiling: Float, release: Float, character: Float, transient: Float, truePeak: Boolean, stereoLink: Float, oversample: Int, clipShape: Int): Boolean =
        sendForkEffect(PARAM_MAXIMIZER, enable, floatArrayOf(
            mode.toFloat(), gain, ceiling, release, character, transient,
            if (truePeak) 1f else 0f, stereoLink, oversample.toFloat(),
            clipShape.toFloat()))

    // The legacy AudioEffect parameter path has no slot for an arbitrary
    // array, and the plugin build has no multiband distortion behind it, so
    // these succeed without doing anything rather than reporting a failure
    // the user cannot act on.
    /**
     * Held until the switch arrives. The system effect takes one array per
     * effect, so the bands and the mix travel together rather than as two
     * calls that could be interleaved with anything else.
     */
    private var pendingDyneqBands: FloatArray? = null

    override fun setTape(enable: Boolean, wow: Float, flutter: Float, saturation: Float, bias: Float, headBump: Float, mix: Float): Boolean =
        sendForkEffect(PARAM_TAPE, enable, floatArrayOf(wow, flutter, saturation, bias, headBump, mix))

    override fun setExciter(enable: Boolean, f1: Float, f2: Float, f3: Float, a1: Float, a2: Float, a3: Float, a4: Float, character: Int, drive: Float, mix: Float): Boolean =
        sendForkEffect(PARAM_EXCITER, enable, floatArrayOf(
            f1, f2, f3, a1, a2, a3, a4, character.toFloat(), drive, mix))

    override fun setLowEnd(enable: Boolean, subsonic: Float, weightHz: Float, weightDb: Float, mudHz: Float, mudDb: Float, mix: Float): Boolean =
        sendForkEffect(PARAM_LOWEND, enable, floatArrayOf(
            subsonic, weightHz, weightDb, mudHz, mudDb, mix))

    override fun setTransient(enable: Boolean, freqLow: Float, freqHigh: Float, attackLow: Float, sustainLow: Float, attackMid: Float, sustainMid: Float, attackHigh: Float, sustainHigh: Float, range: Float, mix: Float): Boolean =
        sendForkEffect(PARAM_TRANSIENT, enable, floatArrayOf(
            freqLow, freqHigh, attackLow, sustainLow, attackMid, sustainMid,
            attackHigh, sustainHigh, range, mix))

    override fun setImaging(enable: Boolean, monoBelow: Float, freqLow: Float, freqMid: Float, freqHigh: Float, widthLow: Float, widthMid: Float, widthHigh: Float, mix: Float): Boolean =
        sendForkEffect(PARAM_IMAGING, enable, floatArrayOf(
            monoBelow, freqLow, freqMid, freqHigh, widthLow, widthMid, widthHigh, mix))

    override fun setDynamicEqBandsInternal(bands: FloatArray?): Boolean {
        pendingDyneqBands = bands
        return true
    }

    override fun setDynamicEq(enable: Boolean, mix: Float, msMode: Int): Boolean {
        val bands = pendingDyneqBands ?: FloatArray(0)
        val payload = FloatArray(3 + bands.size)
        payload[0] = mix
        payload[1] = (bands.size / JamesDspBaseEngine.DYNEQ_VALUES_PER_BAND).toFloat()
        payload[2] = msMode.toFloat()
        bands.copyInto(payload, 3)
        return sendForkEffect(PARAM_DYNAMIC_EQ, enable, payload)
    }

    override fun setMultibandDistBandsInternal(bands: FloatArray?): Boolean = true

    override fun setMultibandDist(enable: Boolean, routing: Int, model: Int, drive: Float, bias: Float, shape: Float, bits: Float, downsample: Float, tone: Float, bandGain: Float, chorusRate: Float, chorusDepth: Float, chorusFeedback: Float, chorusSpread: Float, chorusVoices: Int, chorusMix: Float, mix: Float): Boolean = true

    override fun setEchoDelay(enable: Boolean, input: Float, time: Float, smoothing: Float, offset: Float, keepPitch: Boolean, model: Int, stereo: Float, feedback: Float, cutoff: Float, res: Float, filter: Int, smpRate: Float, bits: Float, modRate: Float, modTime: Float, modCutoff: Float, diffusion: Float, spread: Float, distMode: Int, distLevel: Float, knee: Float, symmetry: Float, tone: Float, wet: Float, dry: Float): Boolean =
        sendForkEffect(PARAM_ECHO_DELAY, enable, floatArrayOf(
            input, time, smoothing, offset, if (keepPitch) 1f else 0f, model.toFloat(),
            stereo, feedback, cutoff, res, filter.toFloat(), smpRate, bits,
            modRate, modTime, modCutoff, diffusion, spread, distMode.toFloat(),
            distLevel, knee, symmetry, tone, wet, dry))

    override fun setPitchShift(enable: Boolean, semitones: Float, mix: Float): Boolean =
        sendForkEffect(PARAM_PITCH_SHIFT, enable, floatArrayOf(semitones, mix))

    /**
     * Sends one fork effect: its values as a float array, then its enable flag.
     * Values go first so the effect is never switched on with stale settings.
     */
    private fun sendForkEffect(param: Int, enable: Boolean, values: FloatArray): Boolean {
        val ok = effect.setParameterFloatArray(param, values) == AudioEffect.SUCCESS
        return ok and (effect.setParameter(
            param + PARAM_FORK_ENABLE_OFFSET, (if (enable) 1 else 0).toShort()
        ) == AudioEffect.SUCCESS)
    }

    override fun setChainOrder(order: IntArray?): Boolean {
        order ?: return true
        return effect.setParameterIntArray(PARAM_CHAIN_ORDER, order) == AudioEffect.SUCCESS
    }

    override fun setSpectrumExtension(enable: Boolean, barkFreq: Float, strength: Float): Boolean {
        // Spectrum extension is unsupported in root/plugin mode (remote engine)
        return true
    }

    override fun setOutputControl(threshold: Float, release: Float, postGain: Float, limiterMode: Int): Boolean {
        // Limiter mode is unsupported in root/plugin mode (remote engine)
        return effect.setParameterFloatArray(
            1500,
            floatArrayOf(threshold, release, postGain)
        ) == AudioEffect.SUCCESS
    }

    override fun setCompanderInternal(
        enable: Boolean,
        timeConstant: Float,
        granularity: Int,
        tfTransforms: Int,
        bands: DoubleArray
    ): Boolean {
        return (effect.setParameterFloatArray(
            115,
            floatArrayOf(timeConstant, granularity.toFloat(), tfTransforms.toFloat()) + bands.map { it.toFloat() }
        ) == AudioEffect.SUCCESS) and (effect.setParameter(1200, enable.toShort()) == AudioEffect.SUCCESS)
    }

    override fun setReverb(enable: Boolean, preset: Int): Boolean {
        var ret = true
        if (enable)
            ret = effect.setParameter(128, preset.toShort()) == AudioEffect.SUCCESS
        return ret and (effect.setParameter(1203, enable.toShort()) == AudioEffect.SUCCESS)
    }

    override fun setCrossfeed(enable: Boolean, mode: Int): Boolean {
        var ret = true
        if (enable)
            ret = effect.setParameter(188, mode.toShort()) == AudioEffect.SUCCESS
        return ret and (effect.setParameter(1208, enable.toShort()) == AudioEffect.SUCCESS)
    }

    override fun setCrossfeedCustom(enable: Boolean, fcut: Int, feed: Int): Boolean {
        throw UnsupportedOperationException()
    }

    override fun setBassBoost(enable: Boolean, maxGain: Float): Boolean {
        var ret = true
        if (enable)
            ret = effect.setParameter(112, maxGain.roundToInt().toShort()) == AudioEffect.SUCCESS
        return ret and (effect.setParameter(1201, enable.toShort()) == AudioEffect.SUCCESS)
    }

    override fun setStereoEnhancement(enable: Boolean, level: Float): Boolean {
        var ret = true
        if (enable)
            ret = effect.setParameter(137, level.roundToInt().toShort()) == AudioEffect.SUCCESS
        return ret and (effect.setParameter(1204, enable.toShort()) == AudioEffect.SUCCESS)
    }

    override fun setVacuumTube(enable: Boolean, level: Float): Boolean {
        var ret = true
        if (enable)
            ret = effect.setParameter(150, (level * 1000).roundToInt().toShort()) == AudioEffect.SUCCESS
        return ret and (effect.setParameter(1206, enable.toShort()) == AudioEffect.SUCCESS)
    }

    override fun setMultiEqualizerInternal(
        enable: Boolean,
        filterType: Int,
        interpolationMode: Int,
        bands: DoubleArray,
    ): Boolean {
        var ret = true

        if (enable) {
            val properties = floatArrayOf(
                filterType.toFloat(),
                if(interpolationMode == 1) 1.0f else -1.0f
            ) + bands.map { it.toFloat() }
            ret = effect.setParameterFloatArray(116, properties) == AudioEffect.SUCCESS
        }

        return ret and (effect.setParameter(1202, enable.toShort()) == AudioEffect.SUCCESS)
    }

    override fun setVdcInternal(enable: Boolean, vdc: String): Boolean {
        val prevCrc = this.ddcHash
        val currentCrc = vdc.crc()

        Timber.i("VDC hash before: $prevCrc, current: $currentCrc")
        if (prevCrc != currentCrc && enable) {
            effect.setParameterCharBuffer(12001, 10009, vdc)
            effect.setParameter(25001, currentCrc) // Commit hash
        }

        return effect.setParameter(1212, enable.toShort()) == AudioEffect.SUCCESS
    }

    override fun setConvolverInternal(
        enable: Boolean,
        impulseResponse: FloatArray,
        irChannels: Int,
        irFrames: Int,
        irCrc: Int,
        irSampleRate: Int,
    ): Boolean {

        convolverSampleRate = irSampleRate

        val prevCrc = this.convolverHash

        Timber.i("Convolver hash before: $prevCrc, current: $irCrc")
        if (prevCrc != irCrc && enable) {
            effect.setParameterImpulseResponseBuffer(12000, 10004, impulseResponse, irChannels)
            effect.setParameter(25003, irCrc) // Commit hash
        }

        return effect.setParameter(1205, enable.toShort()) == AudioEffect.SUCCESS
    }

    fun reloadConvolverIfSampleRateChanged() {
        val currentSampleRate = sampleRate.toInt()
        if (currentSampleRate > 0 && currentSampleRate != convolverSampleRate) {
            Timber.i(
                "Convolver sample rate changed from ${convolverSampleRate}Hz to ${currentSampleRate}Hz"
            )
            syncWithPreferences(arrayOf(Constants.PREF_CONVOLVER))
        }
    }

    override fun setEqPhaseMode(linearPhase: Boolean): Boolean =
        effect.setParameter(PARAM_EQ_PHASE, (if (linearPhase) 1 else 0).toShort()) == AudioEffect.SUCCESS

    override fun setGraphicEqInternal(enable: Boolean, bands: String): Boolean {
        val prevCrc = this.graphicEqHash
        val currentCrc = bands.crc()

        Timber.i("GraphicEQ hash before: $prevCrc, current: $currentCrc")
        if (prevCrc != currentCrc && enable) {
            effect.setParameterCharBuffer(12001, 10006, bands)
            effect.setParameter(25000, currentCrc) // Commit hash
        }

        return effect.setParameter(1210, enable.toShort()) == AudioEffect.SUCCESS
    }

    override fun setLiveprogSlotInternal(slot: Int, enable: Boolean, name: String, script: String): Boolean = true

    override fun setLiveprogInternal(enable: Boolean, name: String, script: String): Boolean {
        val prevCrc = this.liveprogHash
        val currentCrc = script.crc()

        Timber.i("Liveprog hash before: $prevCrc, current: $currentCrc")
        if (prevCrc != currentCrc && enable) {
            effect.setParameterCharBuffer(12001, 10010, script)
            effect.setParameter(25002, currentCrc) // Commit hash
        }

        return effect.setParameter(1213, enable.toShort()) == AudioEffect.SUCCESS
    }

    // Feature support
    override fun supportsEelVmAccess(): Boolean { return false }
    override fun supportsCustomCrossfeed(): Boolean { return false }

    // EEL VM utilities (unavailable)
    override fun enumerateEelVariables(): ArrayList<EelVmVariable> { return arrayListOf() }
    override fun manipulateEelVariable(name: String, value: Float): Boolean { return false }
    override fun freezeLiveprogExecution(freeze: Boolean) {}

    // Status
    val pid: Int
        get() = effect.getParameterInt(20002) ?: -1
    val isPidValid: Boolean
        get() = pid > 0
    val isSampleRateAbnormal: Boolean
        get() = sampleRate <= 0
    val paramCommitCount: Int
        get() = effect.getParameterInt(19998) ?: -1
    val isPresetInitialized: Boolean
        get() = paramCommitCount > 0
    val bufferLength: Int
        get() = effect.getParameterInt(19999) ?: -1
    val allocatedBlockLength: Int
        get() = effect.getParameterInt(20000) ?: -1
    val graphicEqHash: Int
        get() = effect.getParameterInt(30000) ?: -1
    val ddcHash: Int
        get() = effect.getParameterInt(30001) ?: -1
    val liveprogHash: Int
        get() = effect.getParameterInt(30002) ?: -1
    val convolverHash: Int
        get() = effect.getParameterInt(30003) ?: -1

    enum class PluginState {
        Unavailable,
        Available,
        Unsupported
    }

    companion object {
        /*
         * Ids for the effects this fork adds. They sit well clear of the
         * upstream range (which tops out in the 25000s) so a stock module and
         * this one can't misread each other's parameters. Each effect sends
         * its values as one float array rather than an id per value, which
         * keeps the two sides in step: adding a parameter changes the array,
         * not the id space.
         */
        private const val PARAM_FORK_BASE = 26000
        private const val PARAM_CLARITY = PARAM_FORK_BASE + 0
        private const val PARAM_FIELD_SURROUND = PARAM_FORK_BASE + 1
        private const val PARAM_AGC = PARAM_FORK_BASE + 2
        private const val PARAM_HP_SURROUND = PARAM_FORK_BASE + 3
        private const val PARAM_FET_COMP = PARAM_FORK_BASE + 4
        private const val PARAM_CURE = PARAM_FORK_BASE + 5
        private const val PARAM_VIPER_BASS = PARAM_FORK_BASE + 6
        private const val PARAM_VREVERB = PARAM_FORK_BASE + 7
        private const val PARAM_SPEAKER_OPT = PARAM_FORK_BASE + 8
        private const val PARAM_PITCH_SHIFT = PARAM_FORK_BASE + 9
        private const val PARAM_ECHO_DELAY = PARAM_FORK_BASE + 10
        private const val PARAM_EQ_PHASE = PARAM_FORK_BASE + 11
        private const val PARAM_CHAIN_ORDER = PARAM_FORK_BASE + 12
        private const val PARAM_DYNAMIC_EQ = PARAM_FORK_BASE + 13
        private const val PARAM_IMAGING = PARAM_FORK_BASE + 14
        private const val PARAM_TRANSIENT = PARAM_FORK_BASE + 15
        private const val PARAM_LOWEND = PARAM_FORK_BASE + 16
        private const val PARAM_EXCITER = PARAM_FORK_BASE + 17
        private const val PARAM_TAPE = PARAM_FORK_BASE + 18
        private const val PARAM_MAXIMIZER = PARAM_FORK_BASE + 19
        /** Enable flags live one hundred above their value id. */
        private const val PARAM_FORK_ENABLE_OFFSET = 100

        private val EFFECT_TYPE_CUSTOM = UUID.fromString("f98765f4-c321-5de6-9a45-123459495ab2")
        private val EFFECT_JAMESDSP = UUID.fromString("f27317f4-c984-4de6-9a90-545759495bf2")

        fun isPluginInstalled(): PluginState {
            return try {
                AudioEffect
                    .queryEffects()
                    .orEmpty()
                    .firstOrNull { it.uuid == EFFECT_JAMESDSP }
                    ?.run {
                        if(name.contains("v3")) PluginState.Unsupported else PluginState.Available
                    } ?: PluginState.Unavailable
            } catch (e: Exception) {
                Timber.e("isPluginInstalled: exception raised")
                Timber.e(e)
                MainApplication.instance.showAlert(
                    "Error while checking audio effect status",
                    "Unexpected error while checking whether JamesDSP's audio effect library is installed. \n\n" +
                            "Error: $e",
                )
                PluginState.Unavailable
            }
        }
    }
}
