package me.timschneeberger.rootlessjamesdsp.utils

import android.content.Context
import me.timschneeberger.rootlessjamesdsp.R

/**
 * "ViPER4Android only" mode: restricts the app to the effect set the original
 * V4A shipped, disables everything else (so their engines free their buffers
 * and cost nothing), and lets screens re-skin themselves with V4A naming.
 */
object V4aMode {
    const val KEY = "v4a_only_mode"

    /**
     * Cached deliberately. This is read from preference row binding, which runs
     * for every row of every card while scrolling, and MODE_MULTI_PROCESS
     * re-parses the whole file from disk on each call - the app is single
     * process, so that cost bought nothing at all.
     */
    @Volatile private var cached: Boolean? = null

    fun isOn(ctx: Context): Boolean = cached ?: ctx
        .getSharedPreferences(Constants.PREF_APP, Context.MODE_PRIVATE)
        .getBoolean(KEY, false)
        .also { cached = it }

    /** Called when the setting changes, so the next read picks it up. */
    fun invalidate() { cached = null }

    /** Effects the original ViPER4Android did NOT have: namespace + enable key. */
    private fun nonV4a(ctx: Context) = listOf(
        Constants.PREF_COMPANDER to R.string.key_compander_enable,
        Constants.PREF_BASS to R.string.key_bass_enable,
        Constants.PREF_BASSEX to R.string.key_bassex_enable,
        Constants.PREF_PITCHSHIFT to R.string.key_pitchshift_enable,
        Constants.PREF_ECHODELAY to R.string.key_echo_enable,
        Constants.PREF_MULTIBANDDIST to R.string.key_mbd_enable,
        Constants.PREF_MAXIMIZER to R.string.key_maxr_enable,
        Constants.PREF_DYNAMICEQ to R.string.key_dyneq_enable,
        Constants.PREF_IMAGING to R.string.key_imaging_enable,
        Constants.PREF_TRANSIENT to R.string.key_transient_enable,
        Constants.PREF_LOWEND to R.string.key_lowend_enable,
        Constants.PREF_EXCITER to R.string.key_exciter_enable,
        Constants.PREF_TAPE to R.string.key_tape_enable,
        Constants.PREF_VINYL to R.string.key_vinyl_enable,
        Constants.PREF_BALANCE to R.string.key_balance_enable,
        Constants.PREF_GEQ to R.string.key_geq_enable,
        Constants.PREF_PEQ to R.string.key_peq_enable,
        Constants.PREF_LIVEPROG to R.string.key_liveprog_enable,
        Constants.PREF_LIVEPROG2 to R.string.key_liveprog2_enable,
        Constants.PREF_LIVEPROG3 to R.string.key_liveprog3_enable,
        Constants.PREF_LIVEPROG4 to R.string.key_liveprog4_enable,
        Constants.PREF_STEREOWIDE to R.string.key_stereowide_enable,
        Constants.PREF_CROSSFEED to R.string.key_crossfeed_enable,
        Constants.PREF_REVERB to R.string.key_reverb_enable,
    )

    /**
     * Card containers that vanish in V4A mode (matches the list above).
     *
     * Both lists are of what to exclude rather than what to keep, which means
     * anything added to the app later is inside V4A mode until someone
     * remembers to add it here. Worth knowing when adding an effect: a new card
     * that is not an original V4A one belongs in both lists, or the mode shows
     * it and its engine keeps running while the user believes neither.
     */
    val hiddenCardIds = intArrayOf(
        R.id.card_compressor, R.id.card_bass, R.id.card_bassex,
        R.id.card_pitchshift, R.id.card_echo, R.id.card_mbd, R.id.card_maxr, R.id.card_dyneq, R.id.card_imaging, R.id.card_transient, R.id.card_lowend, R.id.card_exciter, R.id.card_tape, R.id.card_geq,
        R.id.card_peq, R.id.card_liveprog, R.id.card_liveprog2,
        R.id.card_liveprog3, R.id.card_liveprog4, R.id.card_stereowide,
        R.id.card_crossfeed, R.id.card_reverb
    )


    /**
     * The original ViPER4Android processing order, taken from ViPER.cpp in the
     * ViPERFX_RE decompilation:
     *
     *   convolver -> headphone surround (VHE) -> DDC -> spectrum extension ->
     *   FIR equalizer -> colourful music (field surround) -> differential
     *   surround -> reverberation -> speaker correction -> playback gain (AGC)
     *   -> FET compressor -> dynamic system -> ViPER bass -> ViPER clarity ->
     *   cure -> tube simulator -> analogX -> software limiter
     *
     * The limiter is not listed here: the engine always runs the output stage
     * last, exactly as V4A did. Effects this fork adds are absent because
     * V4A-only mode disables them anyway.
     */
    val v4aChainOrder = intArrayOf(
        11, // JDSP_EFX_CONVOLVER
        21, // JDSP_EFX_HPSURROUND       (VHE)
        12, // JDSP_EFX_DDC
        22, // JDSP_EFX_SPECTRUMEXT
        9,  // JDSP_EFX_EQUALIZER        (FIR equalizer)
        20, // JDSP_EFX_FIELDSURROUND    (colourful music)
        4,  // JDSP_EFX_DIFFSURROUND
        27, // JDSP_EFX_VREVERB          (reverberation)
        25, // JDSP_EFX_SPEAKEROPT       (speaker correction)
        24, // JDSP_EFX_AGC              (playback gain)
        3,  // JDSP_EFX_FETCOMP
        6,  // JDSP_EFX_VDYNBASS         (dynamic system)
        7,  // JDSP_EFX_VIPERBASS
        23, // JDSP_EFX_CLARITY
        18, // JDSP_EFX_CURE
        0,  // JDSP_EFX_TUBE
    )

    /**
     * Switches every non-V4A effect off. Each engine frees its buffers on
     * disable, so this is also what guarantees they take no resources.
     */
    fun disableNonV4aEffects(ctx: Context) {
        nonV4a(ctx).forEach { (namespace, keyRes) ->
            ctx.getSharedPreferences(namespace, Context.MODE_MULTI_PROCESS)
                .edit().putBoolean(ctx.getString(keyRes), false).apply()
        }
    }
}
