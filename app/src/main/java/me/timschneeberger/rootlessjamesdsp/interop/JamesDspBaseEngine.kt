package me.timschneeberger.rootlessjamesdsp.interop

import android.content.Context
import android.content.Intent
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import me.timschneeberger.rootlessjamesdsp.R
import me.timschneeberger.rootlessjamesdsp.interop.structure.EelVmVariable
import me.timschneeberger.rootlessjamesdsp.model.ParametricEqBandList
import me.timschneeberger.rootlessjamesdsp.model.ProcessorMessage
import me.timschneeberger.rootlessjamesdsp.preference.FileLibraryPreference
import me.timschneeberger.rootlessjamesdsp.utils.BiquadUtils
import me.timschneeberger.rootlessjamesdsp.utils.ConvolverSampleRateFiles
import me.timschneeberger.rootlessjamesdsp.utils.Constants
import me.timschneeberger.rootlessjamesdsp.utils.V4aMode
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.sendLocalBroadcast
import timber.log.Timber
import java.io.File
import java.io.FileNotFoundException
import java.io.FileReader

abstract class JamesDspBaseEngine(val context: Context, val callbacks: JamesDspWrapper.JamesDspCallbacks? = null) : AutoCloseable {
    abstract var enabled: Boolean
    open var sampleRate: Float = 0.0f
        set(value) {
            field = value
            reportSampleRate(value)
        }

    private val syncScope = CoroutineScope(Dispatchers.IO)
    private val syncMutex = Mutex()
    protected val cache = PreferenceCache(context)

    override fun close() {
        Timber.d("Closing engine")
        reportSampleRate(0f)
        syncScope.cancel()
    }

    open fun syncWithPreferences(forceUpdateNamespaces: Array<String>? = null) {
        syncScope.launch {
            syncWithPreferencesAsync(forceUpdateNamespaces)
        }
    }

    fun clearCache() {
        cache.clear()
    }

    private fun reportSampleRate(value: Float) {
        context.sendLocalBroadcast(Intent(Constants.ACTION_REPORT_SAMPLE_RATE).apply {
            putExtra(Constants.EXTRA_SAMPLE_RATE, value)
        })
    }

    private suspend fun syncWithPreferencesAsync(forceUpdateNamespaces: Array<String>? = null) {
        Timber.d("Synchronizing with preferences... (forced: %s)", forceUpdateNamespaces?.joinToString(";") { it })

        syncMutex.withLock {
            cache.select(Constants.PREF_OUTPUT)
            val outputPostGain = cache.get(R.string.key_output_postgain, 0f)
            val limiterThreshold = cache.get(R.string.key_limiter_threshold, -0.1f)
            val limiterRelease = cache.get(R.string.key_limiter_release, 60f)
            // V4A mode uses the faithful port of the original SoftwareLimiter
            val limiterMode = if (V4aMode.isOn(context)) 3
                else cache.get(R.string.key_limiter_mode, "0").toInt()

            cache.select(Constants.PREF_COMPANDER)
            val compEnabled = cache.get(R.string.key_compander_enable, false)
            val compTimeConst = cache.get(R.string.key_compander_timeconstant, 0.22f)
            val compGranularity = cache.get(R.string.key_compander_granularity, 2f).toInt()
            val compTfTransforms = cache.get(R.string.key_compander_tftransforms, "0").toInt()
            val compResponse = cache.get(R.string.key_compander_response, "95.0;200.0;400.0;800.0;1600.0;3400.0;7500.0;0;0;0;0;0;0;0")

            cache.select(Constants.PREF_BASS)
            val bassEnabled = cache.get(R.string.key_bass_enable, false)
            val bassMaxGain = cache.get(R.string.key_bass_max_gain, 5f)

            cache.select(Constants.PREF_BASSEX)
            val bassExEnabled = cache.get(R.string.key_bassex_enable, false)
            val bassExCutoff = cache.get(R.string.key_bassex_cutoff, 100f)
            val bassExIntensity = cache.get(R.string.key_bassex_intensity, 40f)
            val bassExMix = cache.get(R.string.key_bassex_mix, 50f)

            val bassExBand2 = cache.get(R.string.key_bassex_band2_enable, false)
            val bassExCutoff2 = cache.get(R.string.key_bassex_cutoff2, 60f)
            val bassExIntensity2 = cache.get(R.string.key_bassex_intensity2, 40f)
            val bassExMix2 = cache.get(R.string.key_bassex_mix2, 40f)

            cache.select(Constants.PREF_VDYNBASS)
            val vdbEnabled = cache.get(R.string.key_vdynbass_enable, false)
            val vdbMode = cache.get(R.string.key_vdynbass_mode, "10").toInt()
            val vdbGain = cache.get(R.string.key_vdynbass_gain, 33f)
            val vdbX1 = cache.get(R.string.key_vdynbass_x1, 1000f)
            val vdbX2 = cache.get(R.string.key_vdynbass_x2, 6200f)
            val vdbY1 = cache.get(R.string.key_vdynbass_y1, 50f)
            val vdbY2 = cache.get(R.string.key_vdynbass_y2, 90f)
            val vdbSgx = cache.get(R.string.key_vdynbass_sgx, 30f)
            val vdbSgy = cache.get(R.string.key_vdynbass_sgy, 10f)

            cache.select(Constants.PREF_DIFFSURROUND)
            val dsEnabled = cache.get(R.string.key_diffsurround_enable, false)
            val dsDelayL = cache.get(R.string.key_diffsurround_delay_l, 0f)
            val dsDelayR = cache.get(R.string.key_diffsurround_delay_r, 10f)

            cache.select(Constants.PREF_CLARITY)
            val clEnabled = cache.get(R.string.key_clarity_enable, false)
            val clMode = cache.get(R.string.key_clarity_mode, "0").toInt()
            val clGain = cache.get(R.string.key_clarity_gain, 3.5f)

            cache.select(Constants.PREF_FIELDSURROUND)
            val fsEnabled = cache.get(R.string.key_fieldsurround_enable, false)
            val fsStrength = cache.get(R.string.key_fieldsurround_strength, 30f)
            val fsMid = cache.get(R.string.key_fieldsurround_mid, 50f)

            cache.select(Constants.PREF_AGC)
            val agcEnabled = cache.get(R.string.key_agc_enable, false)
            val agcTarget = cache.get(R.string.key_agc_target, 30f)
            val agcMaxBoost = cache.get(R.string.key_agc_maxboost, 12f)

            cache.select(Constants.PREF_HPSURROUND)
            val hpsEnabled = cache.get(R.string.key_hpsurround_enable, false)
            val hpsStrength = cache.get(R.string.key_hpsurround_strength, 60f)
            val hpsRoom = cache.get(R.string.key_hpsurround_room, 30f)

            cache.select(Constants.PREF_FETCOMP)
            val fetEnabled = cache.get(R.string.key_fetcomp_enable, false)
            val fetThr = cache.get(R.string.key_fetcomp_threshold, -18f)
            val fetRatio = cache.get(R.string.key_fetcomp_ratio, 4f)
            val fetAtt = cache.get(R.string.key_fetcomp_attack, 5f)
            val fetRel = cache.get(R.string.key_fetcomp_release, 120f)
            val fetMakeup = cache.get(R.string.key_fetcomp_makeup, 0f)

            cache.select(Constants.PREF_CURE)
            val cureEnabled = cache.get(R.string.key_cure_enable, false)
            val cureLevel = cache.get(R.string.key_cure_level, "0").toInt()

            cache.select(Constants.PREF_VIPERBASS)
            val vbEnabled = cache.get(R.string.key_viperbass_enable, false)
            val vbMode = cache.get(R.string.key_viperbass_mode, "0").toInt()
            val vbFreq = cache.get(R.string.key_viperbass_freq, 76f)
            val vbGain = cache.get(R.string.key_viperbass_gain, 6f)

            cache.select(Constants.PREF_VREVERB)
            val vrEnabled = cache.get(R.string.key_vreverb_enable, false)
            // V4A mode pins the classic model at read time; the stored choice
            // is preserved and comes back when the mode is switched off.
            val vrModel = if (V4aMode.isOn(context)) 0
                else cache.get(R.string.key_vreverb_model, "0").toInt()
            val vrRoom = cache.get(R.string.key_vreverb_room, 50f)
            val vrDamp = cache.get(R.string.key_vreverb_damp, 50f)
            val vrWidth = cache.get(R.string.key_vreverb_width, 100f)
            val vrPredelay = cache.get(R.string.key_vreverb_predelay, 20f)
            val vrDecay = cache.get(R.string.key_vreverb_decay, 45f)
            val vrDiffusion = cache.get(R.string.key_vreverb_diffusion, 70f)
            val vrMod = cache.get(R.string.key_vreverb_mod, 30f)
            val vrBass = cache.get(R.string.key_vreverb_bass, 50f)
            val vrEr = cache.get(R.string.key_vreverb_er, 50f)
            val vrWet = cache.get(R.string.key_vreverb_wet, 30f)
            val vrDry = cache.get(R.string.key_vreverb_dry, 100f)

            cache.select(Constants.PREF_SPEAKEROPT)
            val soEnabled = cache.get(R.string.key_speakeropt_enable, false)
            val soStrength = cache.get(R.string.key_speakeropt_strength, 60f)

            applyChainOrder()

            cache.select(Constants.PREF_MAXIMIZER)
            val maxrEnabled = cache.get(R.string.key_maxr_enable, false)
            val maxrMode = cache.get(R.string.key_maxr_mode, "0").toInt()
            val maxrGain = cache.get(R.string.key_maxr_gain, 0f)
            val maxrCeiling = cache.get(R.string.key_maxr_ceiling, -0.3f)
            val maxrRelease = cache.get(R.string.key_maxr_release, 200f)
            val maxrCharacter = cache.get(R.string.key_maxr_character, 0f)
            val maxrTransient = cache.get(R.string.key_maxr_transient, 0f)
            val maxrTruePeak = cache.get(R.string.key_maxr_true_peak, true)
            val maxrStereoLink = cache.get(R.string.key_maxr_stereo_link, 100f)
            val maxrOversample = cache.get(R.string.key_maxr_oversample, "1").toInt()

            cache.select(Constants.PREF_MULTIBANDDIST)
            val mbdEnabled = cache.get(R.string.key_mbd_enable, false)
            val mbdBands = cache.get(R.string.key_mbd_bands, Constants.DEFAULT_MBD_BANDS)
            val mbdRouting = cache.get(R.string.key_mbd_routing, "0").toInt()
            val mbdModel = cache.get(R.string.key_mbd_model, "0").toInt()
            val mbdDrive = cache.get(R.string.key_mbd_drive, 0f)
            val mbdBias = cache.get(R.string.key_mbd_bias, 0f)
            val mbdShape = cache.get(R.string.key_mbd_shape, 50f)
            val mbdBits = cache.get(R.string.key_mbd_bits, 16f)
            val mbdDownsample = cache.get(R.string.key_mbd_downsample, 0f)
            val mbdTone = cache.get(R.string.key_mbd_tone, 50f)
            val mbdBandGain = cache.get(R.string.key_mbd_band_gain, 100f)
            val mbdChorusRate = cache.get(R.string.key_mbd_chorus_rate, 0.6f)
            val mbdChorusDepth = cache.get(R.string.key_mbd_chorus_depth, 6f)
            val mbdChorusFeedback = cache.get(R.string.key_mbd_chorus_feedback, 0f)
            val mbdChorusSpread = cache.get(R.string.key_mbd_chorus_spread, 50f)
            val mbdChorusVoices = cache.get(R.string.key_mbd_chorus_voices, "1").toInt() + 1
            val mbdChorusMix = cache.get(R.string.key_mbd_chorus_mix, 0f)
            val mbdMix = cache.get(R.string.key_mbd_mix, 100f)

            cache.select(Constants.PREF_ECHODELAY)
            val echoEnabled = cache.get(R.string.key_echo_enable, false)
            val echoInput = cache.get(R.string.key_echo_input, 100f)
            val echoTime = cache.get(R.string.key_echo_time, 350f)
            val echoSmoothing = cache.get(R.string.key_echo_smoothing, 20f)
            val echoOffset = cache.get(R.string.key_echo_offset, 0f)
            val echoKeepPitch = cache.get(R.string.key_echo_keep_pitch, false)
            val echoModel = cache.get(R.string.key_echo_model, "1").toInt()
            val echoStereo = cache.get(R.string.key_echo_stereo, 50f)
            val echoFeedback = cache.get(R.string.key_echo_feedback, 40f)
            val echoCutoff = cache.get(R.string.key_echo_cutoff, 12000f)
            val echoRes = cache.get(R.string.key_echo_res, 10f)
            val echoFilter = cache.get(R.string.key_echo_filter, "0").toInt()
            val echoSmpRate = cache.get(R.string.key_echo_smp_rate, 100f)
            val echoBits = cache.get(R.string.key_echo_bits, 24f)
            val echoModRate = cache.get(R.string.key_echo_mod_rate, 0f)
            val echoModTime = cache.get(R.string.key_echo_mod_time, 0f)
            val echoModCutoff = cache.get(R.string.key_echo_mod_cutoff, 0f)
            val echoDiffusion = cache.get(R.string.key_echo_diffusion, 0f)
            val echoSpread = cache.get(R.string.key_echo_spread, 0f)
            val echoDistMode = cache.get(R.string.key_echo_dist_mode, "1").toInt()
            val echoDistLevel = cache.get(R.string.key_echo_dist_level, 0f)
            val echoKnee = cache.get(R.string.key_echo_knee, 50f)
            val echoSymmetry = cache.get(R.string.key_echo_symmetry, 0f)
            val echoTone = cache.get(R.string.key_echo_tone, 0f)
            val echoWet = cache.get(R.string.key_echo_wet, 35f)
            val echoDry = cache.get(R.string.key_echo_dry, 100f)

            cache.select(Constants.PREF_PITCHSHIFT)
            val psEnabled = cache.get(R.string.key_pitchshift_enable, false)
            val psSemitones = cache.get(R.string.key_pitchshift_semitones, 0f)
            val psMix = cache.get(R.string.key_pitchshift_mix, 100f)

            cache.select(Constants.PREF_SPECTRUMEXT)
            val spxEnabled = cache.get(R.string.key_spectrumext_enable, false)
            val spxBark = cache.get(R.string.key_spectrumext_bark, 7600f)
            val spxStrength = cache.get(R.string.key_spectrumext_strength, 15f)

            cache.select(Constants.PREF_EQ)
            val eqEnabled = cache.get(R.string.key_eq_enable, false)
            val eqFilterType = cache.get(R.string.key_eq_filter_type, "0").toInt()
            val eqInterpolationMode = cache.get(R.string.key_eq_interpolation, "0").toInt()
            val eqBands = cache.get(R.string.key_eq_bands, Constants.DEFAULT_EQ)

            cache.select(Constants.PREF_GEQ)
            val geqEnabled = cache.get(R.string.key_geq_enable, false)
            val geqLinearPhase = cache.get(R.string.key_geq_linear_phase, false)
            val geqBands = cache.get(R.string.key_geq_nodes, Constants.DEFAULT_GEQ_INTERNAL)

            cache.select(Constants.PREF_PEQ)
            val peqEnabled = cache.get(R.string.key_peq_enable, false)
            val peqBandsStr = cache.get(R.string.key_peq_bands, Constants.DEFAULT_PEQ)
            val peqPreamp = cache.get(R.string.key_peq_preamp, 0f)

            cache.select(Constants.PREF_REVERB)
            val reverbEnabled = cache.get(R.string.key_reverb_enable, false)
            val reverbPreset = cache.get(R.string.key_reverb_preset, "0").toInt()

            cache.select(Constants.PREF_STEREOWIDE)
            val swEnabled = cache.get(R.string.key_stereowide_enable, false)
            val swMode = cache.get(R.string.key_stereowide_mode, 60f)

            cache.select(Constants.PREF_CROSSFEED)
            val crossfeedEnabled = cache.get(R.string.key_crossfeed_enable, false)
            val crossfeedMode = cache.get(R.string.key_crossfeed_mode, "0").toInt()

            cache.select(Constants.PREF_TUBE)
            val tubeEnabled = cache.get(R.string.key_tube_enable, false)
            val tubeDrive = cache.get(R.string.key_tube_drive, 2f)

            cache.select(Constants.PREF_DDC)
            val ddcEnabled = cache.get(R.string.key_ddc_enable, false)
            val ddcFile = cache.get(R.string.key_ddc_file, "")

            cache.select(Constants.PREF_LIVEPROG)
            val liveProgEnabled = cache.get(R.string.key_liveprog_enable, false)
            val liveprogFile = cache.get(R.string.key_liveprog_file, "")
            cache.select(Constants.PREF_LIVEPROG2)
            val liveprog2Enabled = cache.get(R.string.key_liveprog2_enable, false)
            val liveprog2File = cache.get(R.string.key_liveprog2_file, "")

            cache.select(Constants.PREF_LIVEPROG3)
            val liveprog3Enabled = cache.get(R.string.key_liveprog3_enable, false)
            val liveprog3File = cache.get(R.string.key_liveprog3_file, "")

            cache.select(Constants.PREF_LIVEPROG4)
            val liveprog4Enabled = cache.get(R.string.key_liveprog4_enable, false)
            val liveprog4File = cache.get(R.string.key_liveprog4_file, "")

            cache.select(Constants.PREF_CONVOLVER)
            val convolverEnabled = cache.get(R.string.key_convolver_enable, false)
            val convolverFile = cache.get(R.string.key_convolver_file, "")
            val convolverSampleRateFiles = cache.get(R.string.key_convolver_sample_rate_files, "")
            val convolverAdvImp = cache.get(R.string.key_convolver_adv_imp, Constants.DEFAULT_CONVOLVER_ADVIMP)
            val convolverMode = cache.get(R.string.key_convolver_mode, "0").toInt()

            val targets = cache.changedNamespaces.toTypedArray() + (forceUpdateNamespaces ?: arrayOf())
            targets.forEach {
                Timber.i("Committing new changes in namespace '$it'")
                CrashBreadcrumb.mark(context, "apply start: " + it)

                val result = try { when (it) {
                    Constants.PREF_OUTPUT -> setOutputControl(limiterThreshold, limiterRelease, outputPostGain, limiterMode)
                    Constants.PREF_COMPANDER -> setCompander(compEnabled, compTimeConst, compGranularity, compTfTransforms, compResponse)
                    Constants.PREF_BASS -> setBassBoost(bassEnabled, bassMaxGain)
                    Constants.PREF_BASSEX -> setBassExciter(bassExEnabled, bassExCutoff, bassExIntensity, bassExMix, bassExBand2, bassExCutoff2, bassExIntensity2, bassExMix2)
                    Constants.PREF_VDYNBASS -> {
                        val p = if (vdbMode in vdynBassPresets.indices) vdynBassPresets[vdbMode]
                                else floatArrayOf(vdbX1, vdbX2, vdbY1, vdbY2, vdbSgx, vdbSgy)
                        setVDynBass(vdbEnabled, vdbGain, p[0], p[1], p[2], p[3], p[4], p[5])
                    }
                    Constants.PREF_DIFFSURROUND -> setDiffSurround(dsEnabled, dsDelayL, dsDelayR)
                    Constants.PREF_CLARITY -> setViperClarity(clEnabled, clMode, clGain)
                    Constants.PREF_FIELDSURROUND -> setFieldSurround(fsEnabled, fsStrength, fsMid)
                    Constants.PREF_AGC -> setAgc(agcEnabled, agcTarget, agcMaxBoost)
                    Constants.PREF_HPSURROUND -> setHpSurround(hpsEnabled, hpsStrength, hpsRoom)
                    Constants.PREF_FETCOMP -> setFetComp(fetEnabled, fetThr, fetRatio, fetAtt, fetRel, fetMakeup)
                    Constants.PREF_CURE -> setCure(cureEnabled, cureLevel)
                    Constants.PREF_VIPERBASS -> setViperBass(vbEnabled, vbMode, vbFreq, vbGain)
                    Constants.PREF_VREVERB -> setVReverb(
                        vrEnabled, vrModel, vrRoom, vrDamp, vrWidth, vrPredelay, vrDecay,
                        vrDiffusion, vrMod, vrBass, vrEr, vrWet, vrDry)
                    Constants.PREF_SPEAKEROPT -> setSpeakerOpt(soEnabled, soStrength)
                    Constants.PREF_PITCHSHIFT -> setPitchShift(psEnabled, psSemitones, psMix)
                    Constants.PREF_MAXIMIZER -> setMaximizer(
                        maxrEnabled, maxrMode, maxrGain, maxrCeiling, maxrRelease,
                        maxrCharacter, maxrTransient, maxrTruePeak, maxrStereoLink,
                        maxrOversample
                    )
                    Constants.PREF_MULTIBANDDIST -> {
                        // Bands first: the cascade has to be in place before
                        // the stage that feeds off it is switched on.
                        setMultibandDistBands(mbdBands)
                        setMultibandDist(
                            mbdEnabled, mbdRouting, mbdModel, mbdDrive, mbdBias, mbdShape,
                            mbdBits, mbdDownsample, mbdTone, mbdBandGain,
                            mbdChorusRate, mbdChorusDepth, mbdChorusFeedback,
                            mbdChorusSpread, mbdChorusVoices, mbdChorusMix, mbdMix
                        )
                    }
                    Constants.PREF_ECHODELAY -> setEchoDelay(
                        echoEnabled, echoInput, echoTime, echoSmoothing, echoOffset, echoKeepPitch, echoModel, echoStereo, echoFeedback, echoCutoff, echoRes, echoFilter, echoSmpRate, echoBits, echoModRate, echoModTime, echoModCutoff, echoDiffusion, echoSpread, echoDistMode, echoDistLevel, echoKnee, echoSymmetry, echoTone, echoWet, echoDry
                    )
                    Constants.PREF_SPECTRUMEXT -> setSpectrumExtension(spxEnabled, spxBark, spxStrength)
                    Constants.PREF_EQ -> setMultiEqualizer(eqEnabled, eqFilterType, eqInterpolationMode, eqBands)
                    Constants.PREF_GEQ -> {
                        // Phase mode first: it rebuilds the coefficient
                        // generator, so the nodes must be re-sent after it.
                        setEqPhaseMode(geqLinearPhase)
                        setGraphicEqCombined(geqEnabled, geqBands, peqEnabled, peqBandsStr, peqPreamp)
                    }
                    Constants.PREF_PEQ -> {
                        setEqPhaseMode(geqLinearPhase)
                        setGraphicEqCombined(geqEnabled, geqBands, peqEnabled, peqBandsStr, peqPreamp)
                    }
                    Constants.PREF_REVERB -> setReverb(reverbEnabled, reverbPreset)
                    Constants.PREF_STEREOWIDE -> setStereoEnhancement(swEnabled, swMode)
                    Constants.PREF_CROSSFEED -> setCrossfeed(crossfeedEnabled, crossfeedMode)
                    Constants.PREF_TUBE -> setVacuumTube(tubeEnabled, tubeDrive)
                    Constants.PREF_DDC -> setVdc(ddcEnabled, ddcFile)
                    Constants.PREF_LIVEPROG -> setLiveprog(liveProgEnabled, liveprogFile)
                    Constants.PREF_LIVEPROG2 -> setLiveprogSlot(1, liveprog2Enabled, liveprog2File)
                    Constants.PREF_LIVEPROG3 -> setLiveprogSlot(2, liveprog3Enabled, liveprog3File)
                    Constants.PREF_LIVEPROG4 -> setLiveprogSlot(3, liveprog4Enabled, liveprog4File)
                    Constants.PREF_CONVOLVER -> {
                        val mappedFile = ConvolverSampleRateFiles.resolve(
                            convolverSampleRateFiles,
                            sampleRate.toInt(),
                            convolverFile,
                        )
                        val selectedFile = mappedFile.takeIf {
                            File(FileLibraryPreference.createFullPathCompat(context, it)).isFile
                        } ?: convolverFile
                        setConvolver(convolverEnabled, selectedFile, convolverMode, convolverAdvImp)
                    }
                    else -> true
                } }
                catch (e: Throwable) {
                    Timber.e(e, "Exception while applying namespace ")
                    try {
                        android.os.Handler(android.os.Looper.getMainLooper()).post {
                            android.widget.Toast.makeText(context, "DSP section '" + it + "' failed: " + e, android.widget.Toast.LENGTH_LONG).show()
                        }
                    } catch (_: Exception) {}
                    false
                }

                CrashBreadcrumb.mark(context, "apply done: " + it)

                if(!result) {
                    Timber.e("Failed to apply $it")
                }
            }

            cache.markChangesAsCommitted()
            Timber.i("Preferences synchronized")
        }
    }

    fun setMultiEqualizer(enable: Boolean, filterType: Int, interpolationMode: Int, bands: String): Boolean
    {
        val doubleArray = DoubleArray(30)
        val array = bands.split(";")
        for((i, str) in array.withIndex())
        {
            val number = str.toDoubleOrNull()
            if(number == null) {
                Timber.e("setFirEqualizer: malformed EQ string")
                return false
            }
            doubleArray[i] = number
        }

        return setMultiEqualizerInternal(enable, filterType, interpolationMode, doubleArray)
    }

    fun setCompander(enable: Boolean, timeConstant: Float, granularity: Int, tfTransforms: Int, bands: String): Boolean
    {
        val doubleArray = DoubleArray(14)
        val array = bands.split(";")
        for((i, str) in array.withIndex())
        {
            val number = str.toDoubleOrNull()
            if(number == null) {
                Timber.e("setCompander: malformed string")
                return false
            }
            doubleArray[i] = number
        }

        return setCompanderInternal(enable, timeConstant, granularity, tfTransforms, doubleArray)
    }

    fun setVdc(enable: Boolean, vdcPath: String): Boolean
    {
        val fullPath = FileLibraryPreference.createFullPathCompat(context, vdcPath)

        if(!File(fullPath).exists() || File(fullPath).isDirectory) {
            Timber.w("setVdc: file does not exist")
            setVdcInternal(false, "")
            return true /* non-critical */
        }

        return safeFileReader(fullPath)?.use {
            setVdcInternal(enable, it.readText())
        } ?: false
    }

    fun setConvolver(enable: Boolean, impulseResponsePath: String, optimizationMode: Int, waveEditStr: String): Boolean
    {
        val path = FileLibraryPreference.createFullPathCompat(context, impulseResponsePath)
        val targetSampleRate = sampleRate.toInt()

        // Handle disabled state before everything else
        if(!enable || !File(path).exists() || File(path).isDirectory) {
            setConvolverInternal(false, FloatArray(0), 0, 0, 0, targetSampleRate)
            return true
        }

        val advConv = waveEditStr.split(";")
        val advSetting = IntArray(6)
        advSetting.fill(0)
        advSetting[0] = -80
        advSetting[1] = -100
        try
        {
            if (advConv.size == 6)
            {
                for (i in advConv.indices) advSetting[i] = Integer.valueOf(advConv[i])
            }
            else {
                Timber.w("setConvolver: AdvImp setting has the wrong size (${advConv.size})")
                callbacks?.onConvolverParseError(ProcessorMessage.ConvolverErrorCode.AdvParamsInvalid)
            }
        }
        catch(ex: NumberFormatException) {
            Timber.e("setConvolver: NumberFormatException while parsing AdvImp setting. Using defaults.")
            callbacks?.onConvolverParseError(ProcessorMessage.ConvolverErrorCode.AdvParamsInvalid)
        }

        val info = IntArray(4)
        val imp = JdspImpResToolbox.ReadImpulseResponseToFloat(
            path,
            targetSampleRate,
            info,
            optimizationMode,
            advSetting
        )

        if(imp == null) {
            Timber.e("setConvolver: Failed to read IR")
            setConvolverInternal(false, FloatArray(0), 0, 0, 0, targetSampleRate)
            callbacks?.onConvolverParseError(ProcessorMessage.ConvolverErrorCode.Corrupted)
            return false
        }

        // check frame count
        if(info[1] == 0) {
            Timber.e("setConvolver: IR has no frames")
            setConvolverInternal(false, FloatArray(0), 0, 0, 0, targetSampleRate)
            callbacks?.onConvolverParseError(ProcessorMessage.ConvolverErrorCode.NoFrames)
            return false
        }

        // check if advSetting was invalid
        if(info[3] == 0) {
            Timber.w("setConvolver: advSetting was invalid")
            callbacks?.onConvolverParseError(ProcessorMessage.ConvolverErrorCode.AdvParamsInvalid)
        }

        return setConvolverInternal(true, imp, info[0], info[1], info[2], targetSampleRate)
    }

    fun setGraphicEq(enable: Boolean, bands: String): Boolean
    {
        // Sanity check
        if(!bands.contains("GraphicEQ:", true)) {
            Timber.e("setGraphicEq: malformed string")
            setGraphicEqInternal(false, "")
            return false
        }

        return setGraphicEqInternal(enable, bands)
    }

    fun setGraphicEqCombined(
        geqEnabled: Boolean, geqBands: String,
        peqEnabled: Boolean, peqBandsStr: String,
        peqPreamp: Float = 0f
    ): Boolean {
        // Parse PEQ bands and compute biquad magnitude response at 512 points
        val peqBands = ParametricEqBandList()
        peqBands.deserialize(peqBandsStr)
        val hasPeqBands = peqEnabled && peqBands.isNotEmpty()
        val peqResponse = if (hasPeqBands) {
            BiquadUtils.computeCombinedResponse(peqBands, numPoints = 512)
        } else null

        val anyEnabled = geqEnabled || hasPeqBands
        if (!anyEnabled) {
            return setGraphicEqInternal(false, "")
        }

        // Apply preamp offset to PEQ response
        val preampOffset = if (hasPeqBands) peqPreamp.toDouble() else 0.0

        if (peqResponse != null && geqEnabled && geqBands.contains("GraphicEQ:", true)) {
            // Both PEQ and GEQ enabled: merge magnitudes
            val combined = mergeGeqWithPeq(geqBands, peqResponse, preampOffset)
            return setGraphicEqInternal(true, combined)
        } else if (peqResponse != null) {
            // Only PEQ enabled
            val peqString = BiquadUtils.toGraphicEqString(peqResponse, preampOffset)
            return setGraphicEqInternal(true, peqString)
        } else if (geqEnabled) {
            // Only GEQ enabled
            return setGraphicEq(geqEnabled, geqBands)
        }

        return setGraphicEqInternal(false, "")
    }

    private fun mergeGeqWithPeq(
        geqBands: String,
        peqResponse: List<Pair<Double, Double>>,
        preampOffset: Double = 0.0
    ): String {
        // Parse GEQ nodes from "GraphicEQ: f1 g1; f2 g2; ..." string
        val geqNodes = mutableListOf<Pair<Double, Double>>()
        val content = geqBands.replace("GraphicEQ:", "").trim()
        content.split(";").map { it.trim() }.filter { it.isNotBlank() }.forEach { s ->
            val parts = s.split(" ").filter { it.isNotBlank() }
            val freq = parts.getOrNull(0)?.toDoubleOrNull()
            val gain = parts.getOrNull(1)?.toDoubleOrNull()
            if (freq != null && gain != null) {
                geqNodes.add(Pair(freq, gain))
            }
        }
        geqNodes.sortBy { it.first }

        // For each PEQ sample point, interpolate GEQ gain (log-linear) and sum
        val sb = StringBuilder("GraphicEQ: ")
        for ((peqFreq, peqGain) in peqResponse) {
            val geqGain = interpolateGeq(geqNodes, peqFreq)
            sb.append("${dfMergeFreq.format(peqFreq)} ${dfMergeGain.format(peqGain + geqGain + preampOffset)}; ")
        }

        return sb.toString()
    }

    private fun interpolateGeq(nodes: List<Pair<Double, Double>>, freq: Double): Double {
        if (nodes.isEmpty()) return 0.0
        if (freq <= nodes.first().first) return nodes.first().second
        if (freq >= nodes.last().first) return nodes.last().second

        // Find surrounding nodes and do log-linear interpolation
        for (i in 0 until nodes.size - 1) {
            val (f0, g0) = nodes[i]
            val (f1, g1) = nodes[i + 1]
            if (freq in f0..f1) {
                if (f1 <= f0) return g0
                val logF = kotlin.math.ln(freq)
                val logF0 = kotlin.math.ln(f0)
                val logF1 = kotlin.math.ln(f1)
                val t = (logF - logF0) / (logF1 - logF0)
                return g0 + t * (g1 - g0)
            }
        }
        return 0.0
    }

    /**
     * Loads a script into one of the chained Liveprog slots (1-3). Slot 0 is
     * the original Liveprog card and goes through [setLiveprog].
     */
    fun setLiveprogSlot(slot: Int, enable: Boolean, path: String): Boolean {
        if (slot < 1 || slot > 3) return false
        if (path.isBlank()) return setLiveprogSlotInternal(slot, false, "", "")

        val fullPath = FileLibraryPreference.createFullPathCompat(context, path)
        val file = File(fullPath)
        if (!file.exists() || file.isDirectory) {
            Timber.w("setLiveprogSlot: file does not exist ($fullPath)")
            return setLiveprogSlotInternal(slot, false, "", "")
        }
        return safeFileReader(fullPath)?.use {
            setLiveprogSlotInternal(slot, enable, file.name, it.readText())
        } ?: setLiveprogSlotInternal(slot, false, "", "")
    }

    fun setLiveprog(enable: Boolean, path: String): Boolean
    {
        val fullPath = FileLibraryPreference.createFullPathCompat(context, path)

        if(!File(fullPath).exists() || File(fullPath).isDirectory) {
            Timber.w("setLiveprog: file does not exist")
            return setLiveprogInternal(false, "", "")
        }

        return safeFileReader(fullPath)?.use {
            val name = File(fullPath).name
            setLiveprogInternal(enable, name, it.readText())
        } ?: false
    }

    private fun safeFileReader(path: String) =
        try { FileReader(path) }
        catch (ex: FileNotFoundException) {
            /* Exception may occur when old presets created with version <1.4.3 are swapped
               between root, rootless, debug, or release builds due to path name differences. */
            Timber.w(ex)
            null
        }

    // Effect config
    abstract fun setOutputControl(threshold: Float, release: Float, postGain: Float, limiterMode: Int = 0): Boolean
    abstract fun setBassExciter(enable: Boolean, cutoff: Float, intensity: Float, mix: Float, band2: Boolean, cutoff2: Float, intensity2: Float, mix2: Float): Boolean
    abstract fun setVDynBass(enable: Boolean, gain: Float, x1: Float, x2: Float, y1: Float, y2: Float, sgx: Float, sgy: Float): Boolean
    abstract fun setDiffSurround(enable: Boolean, delayLms: Float, delayRms: Float): Boolean
    abstract fun setViperClarity(enable: Boolean, mode: Int, gain: Float): Boolean
    abstract fun setFieldSurround(enable: Boolean, strength: Float, midImage: Float): Boolean
    abstract fun setAgc(enable: Boolean, target: Float, maxBoost: Float): Boolean
    abstract fun setHpSurround(enable: Boolean, strength: Float, room: Float): Boolean
    abstract fun setFetComp(enable: Boolean, threshold: Float, ratio: Float, attack: Float, release: Float, makeup: Float): Boolean
    abstract fun setCure(enable: Boolean, level: Int): Boolean
    abstract fun setViperBass(enable: Boolean, mode: Int, freq: Float, gain: Float): Boolean
    abstract fun setVReverb(enable: Boolean, model: Int, room: Float, damp: Float,
                            width: Float, predelay: Float, decay: Float, diffusion: Float,
                            mod: Float, bass: Float, er: Float, wet: Float, dry: Float): Boolean
    abstract fun setSpeakerOpt(enable: Boolean, strength: Float): Boolean
    abstract fun setPitchShift(enable: Boolean, semitones: Float, mix: Float): Boolean
    /**
     * Hands the band-select cascade to the engine as a flat array of
     * (frequency, gain, q, type) groups, in the same order the editor shows
     * them. The type codes are [me.timschneeberger.rootlessjamesdsp.model.ParametricEqFilterType.code],
     * which the engine mirrors as MbdFilterType - the two must not drift.
     */
    fun setMultibandDistBands(serialized: String): Boolean {
        val bands = ParametricEqBandList()
        bands.deserialize(serialized)
        if (bands.isEmpty())
            return setMultibandDistBandsInternal(null)
        val flat = FloatArray(bands.size * 4)
        for ((i, band) in bands.withIndex()) {
            flat[i * 4] = band.frequency.toFloat()
            flat[i * 4 + 1] = band.gain.toFloat()
            flat[i * 4 + 2] = band.q.toFloat()
            flat[i * 4 + 3] = band.filterType.code.toFloat()
        }
        return setMultibandDistBandsInternal(flat)
    }

    abstract fun setMaximizer(enable: Boolean, mode: Int, gain: Float, ceiling: Float, release: Float, character: Float, transient: Float, truePeak: Boolean, stereoLink: Float, oversample: Int): Boolean
    protected abstract fun setMultibandDistBandsInternal(bands: FloatArray?): Boolean
    abstract fun setMultibandDist(enable: Boolean, routing: Int, model: Int, drive: Float, bias: Float, shape: Float, bits: Float, downsample: Float, tone: Float, bandGain: Float, chorusRate: Float, chorusDepth: Float, chorusFeedback: Float, chorusSpread: Float, chorusVoices: Int, chorusMix: Float, mix: Float): Boolean
    abstract fun setEchoDelay(enable: Boolean, input: Float, time: Float, smoothing: Float, offset: Float, keepPitch: Boolean, model: Int, stereo: Float, feedback: Float, cutoff: Float, res: Float, filter: Int, smpRate: Float, bits: Float, modRate: Float, modTime: Float, modCutoff: Float, diffusion: Float, spread: Float, distMode: Int, distLevel: Float, knee: Float, symmetry: Float, tone: Float, wet: Float, dry: Float): Boolean
    abstract fun setChainOrder(order: IntArray?): Boolean

    abstract fun setSpectrumExtension(enable: Boolean, barkFreq: Float, strength: Float): Boolean
    abstract fun setReverb(enable: Boolean, preset: Int): Boolean
    abstract fun setCrossfeed(enable: Boolean, mode: Int): Boolean
    abstract fun setCrossfeedCustom(enable: Boolean, fcut: Int, feed: Int): Boolean
    abstract fun setBassBoost(enable: Boolean, maxGain: Float): Boolean
    abstract fun setStereoEnhancement(enable: Boolean, level: Float): Boolean
    abstract fun setVacuumTube(enable: Boolean, level: Float): Boolean

    protected abstract fun setMultiEqualizerInternal(enable: Boolean, filterType: Int, interpolationMode: Int, bands: DoubleArray): Boolean
    protected abstract fun setCompanderInternal(enable: Boolean, timeConstant: Float, granularity: Int, tfTransforms: Int, bands: DoubleArray): Boolean
    protected abstract fun setVdcInternal(enable: Boolean, vdc: String): Boolean
    protected abstract fun setConvolverInternal(
        enable: Boolean,
        impulseResponse: FloatArray,
        irChannels: Int,
        irFrames: Int,
        irCrc: Int,
        irSampleRate: Int,
    ): Boolean
    abstract fun setEqPhaseMode(linearPhase: Boolean): Boolean

    protected abstract fun setGraphicEqInternal(enable: Boolean, bands: String): Boolean
    protected abstract fun setLiveprogInternal(enable: Boolean, name: String, script: String): Boolean

    protected abstract fun setLiveprogSlotInternal(slot: Int, enable: Boolean, name: String, script: String): Boolean

    // Feature support
    abstract fun supportsEelVmAccess(): Boolean
    abstract fun supportsCustomCrossfeed(): Boolean

    // EEL VM utilities
    abstract fun enumerateEelVariables(): ArrayList<EelVmVariable>
    abstract fun manipulateEelVariable(name: String, value: Float): Boolean
    abstract fun freezeLiveprogExecution(freeze: Boolean)

    protected inner class DummyCallbacks : JamesDspWrapper.JamesDspCallbacks
    {
        override fun onLiveprogOutput(message: String) {}
        override fun onLiveprogExec(id: String) {}
        override fun onLiveprogResult(resultCode: Int, id: String, errorMessage: String?) {}
        override fun onVdcParseError() {}
        override fun onConvolverParseError(errorCode: ProcessorMessage.ConvolverErrorCode) {}
    }

    companion object {
        private val dfMergeFreq = java.text.DecimalFormat("0.00", java.text.DecimalFormatSymbols.getInstance(java.util.Locale.ENGLISH))
        private val dfMergeGain = java.text.DecimalFormat("0.000000", java.text.DecimalFormatSymbols.getInstance(java.util.Locale.ENGLISH))
    }

    /** Pushes the user's processing order (if any) down to the engine. */
    fun applyChainOrder() {
        // V4A-only mode runs the original ViPER4Android chain. Pinned at read
        // time so the user's own order is preserved and returns untouched when
        // the mode is switched off.
        if (V4aMode.isOn(context)) {
            setChainOrder(V4aMode.v4aChainOrder)
            return
        }
        val saved = context
            .getSharedPreferences(Constants.PREF_CHAIN_ORDER, Context.MODE_PRIVATE)
            .getString(Constants.KEY_CHAIN_ORDER, null)
        val order = saved
            ?.split(",")
            ?.mapNotNull { it.trim().toIntOrNull() }
            ?.toIntArray()
        setChainOrder(if (order == null || order.isEmpty()) null else order)
    }

}

// x1, x2, y1, y2, sideGainX, sideGainY — from the ViperFX DynamicBass presets
internal val vdynBassPresets = arrayOf(
    floatArrayOf(140f,6200f,40f,60f,10f,80f),
    floatArrayOf(180f,5800f,55f,80f,10f,70f),
    floatArrayOf(300f,5600f,60f,105f,10f,50f),
    floatArrayOf(600f,5400f,60f,105f,10f,20f),
    floatArrayOf(100f,5600f,40f,80f,50f,50f),
    floatArrayOf(1200f,6200f,40f,80f,0f,20f),
    floatArrayOf(1000f,6200f,40f,80f,0f,10f),
    floatArrayOf(800f,6200f,40f,80f,10f,0f),
    floatArrayOf(400f,6200f,40f,80f,10f,0f),
    floatArrayOf(1200f,6200f,50f,90f,15f,10f),
    floatArrayOf(1000f,6200f,50f,90f,30f,10f),
    floatArrayOf(1100f,6200f,60f,100f,20f,0f),
    floatArrayOf(1200f,6200f,50f,100f,10f,50f),
    floatArrayOf(1200f,6200f,60f,100f,0f,30f),
    floatArrayOf(1200f,6200f,40f,80f,0f,30f),
    floatArrayOf(1000f,6200f,60f,100f,0f,0f),
    floatArrayOf(1000f,6200f,60f,120f,0f,0f),
    floatArrayOf(1000f,6200f,80f,140f,0f,0f),
    floatArrayOf(800f,6200f,80f,140f,0f,0f)
)
