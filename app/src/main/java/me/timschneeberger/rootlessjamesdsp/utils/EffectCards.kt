package me.timschneeberger.rootlessjamesdsp.utils

/**
 * Which effect card belongs to which preference namespace.
 *
 * The card list itself lives in DspFragment, keyed by view id, but the keys the
 * layout persists are the resource entry names of those ids. Code outside the
 * fragment - loading a preset, for one - has neither the fragment nor the
 * resources to hand, so the correspondence is written out once here.
 *
 * Keep in step with DspFragment.searchableCards. A namespace missing from this
 * map is not an error: it only means a preset cannot reveal that card.
 */
object EffectCards {

    /** Preference namespace (the shared_prefs file name) to layout card key. */
    val cardKeyByNamespace: Map<String, String> = mapOf(
        Constants.PREF_OUTPUT to "card_output_control",
        Constants.PREF_COMPANDER to "card_compressor",
        Constants.PREF_BASS to "card_bass",
        Constants.PREF_BASSEX to "card_bassex",
        Constants.PREF_VDYNBASS to "card_vdynbass",
        Constants.PREF_DIFFSURROUND to "card_diffsurround",
        Constants.PREF_CLARITY to "card_clarity",
        Constants.PREF_FIELDSURROUND to "card_fieldsurround",
        Constants.PREF_HPSURROUND to "card_hpsurround",
        Constants.PREF_FETCOMP to "card_fetcomp",
        Constants.PREF_CURE to "card_cure",
        Constants.PREF_VIPERBASS to "card_viperbass",
        Constants.PREF_VREVERB to "card_vreverb",
        Constants.PREF_SPEAKEROPT to "card_speakeropt",
        Constants.PREF_PITCHSHIFT to "card_pitchshift",
        Constants.PREF_ECHODELAY to "card_echo",
        Constants.PREF_MULTIBANDDIST to "card_mbd",
        Constants.PREF_MAXIMIZER to "card_maxr",
        Constants.PREF_DYNAMICEQ to "card_dyneq",
        Constants.PREF_IMAGING to "card_imaging",
        Constants.PREF_TRANSIENT to "card_transient",
        Constants.PREF_LOWEND to "card_lowend",
        Constants.PREF_EXCITER to "card_exciter",
        Constants.PREF_AGC to "card_agc",
        Constants.PREF_EQ to "card_eq",
        Constants.PREF_GEQ to "card_geq",
        Constants.PREF_PEQ to "card_peq",
        Constants.PREF_DDC to "card_ddc",
        Constants.PREF_CONVOLVER to "card_convolver",
        Constants.PREF_LIVEPROG to "card_liveprog",
        Constants.PREF_LIVEPROG2 to "card_liveprog2",
        Constants.PREF_LIVEPROG3 to "card_liveprog3",
        Constants.PREF_LIVEPROG4 to "card_liveprog4",
        Constants.PREF_TUBE to "card_tube",
        Constants.PREF_SPECTRUMEXT to "card_spectrumext",
        Constants.PREF_STEREOWIDE to "card_stereowide",
        Constants.PREF_CROSSFEED to "card_crossfeed",
        Constants.PREF_REVERB to "card_reverb",
    )

    /** "dsp_bass.xml" -> "card_bass", or null if the file is not an effect. */
    fun cardKeyForPrefFile(fileName: String): String? =
        cardKeyByNamespace[fileName.removeSuffix(".xml")]

    /**
     * True if the given shared-preferences XML turns its effect on.
     *
     * Every effect namespace stores its master switch as a boolean whose name
     * ends in "_enable", so this reads the switch without needing to know the
     * exact key per effect. Matched on the stored file rather than on resource
     * keys because the caller is working with an extracted archive, not with a
     * live SharedPreferences.
     */
    fun declaresEffectEnabled(xml: String): Boolean =
        ENABLE_TRUE.containsMatchIn(xml)

    private val ENABLE_TRUE =
        Regex("""<boolean\s+name="[^"]*_enable"\s+value="true"\s*/>""")
}
