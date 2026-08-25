package me.timschneeberger.rootlessjamesdsp.utils

import me.timschneeberger.rootlessjamesdsp.BuildConfig

object Constants {
    // App-relevant preference namespaces
    const val PREF_APP = "application"
    const val PREF_VAR = "variable"

    // DSP-relevant preference namespaces
    const val PREF_BASS = "dsp_bass"
    const val PREF_COMPANDER = "dsp_compander"
    const val PREF_CONVOLVER = "dsp_convolver"
    const val PREF_CROSSFEED = "dsp_crossfeed"
    const val PREF_DDC = "dsp_ddc"
    const val PREF_EQ = "dsp_equalizer"
    const val PREF_GEQ = "dsp_graphiceq"
    const val PREF_PEQ = "dsp_parametriceq"
    const val PREF_LIVEPROG = "dsp_liveprog"
    const val ACTION_LIVEPROG_SLOTS_CHANGED = "me.timschneeberger.rootlessjamesdsp.LIVEPROG_SLOTS_CHANGED"
    const val PREF_LIVEPROG2 = "dsp_liveprog2"
    const val PREF_LIVEPROG3 = "dsp_liveprog3"
    const val PREF_LIVEPROG4 = "dsp_liveprog4"
    const val PREF_OUTPUT = "dsp_output_control"
    const val PREF_REVERB = "dsp_reverb"
    const val PREF_STEREOWIDE = "dsp_stereowide"
    const val PREF_TUBE = "dsp_tube"
    const val PREF_BASSEX = "dsp_bassex"
    const val PREF_SPECTRUMEXT = "dsp_spectrumext"
    const val PREF_VDYNBASS = "dsp_vdynbass"
    const val PREF_DIFFSURROUND = "dsp_diffsurround"
    const val PREF_CLARITY = "dsp_clarity"
    const val PREF_FIELDSURROUND = "dsp_fieldsurround"
    const val PREF_AGC = "dsp_agc"
    const val PREF_HPSURROUND = "dsp_hpsurround"
    const val PREF_FETCOMP = "dsp_fetcomp"
    const val PREF_CURE = "dsp_cure"
    const val PREF_VIPERBASS = "dsp_viperbass"
    const val PREF_VREVERB = "dsp_vreverb"
    const val PREF_SPEAKEROPT = "dsp_speakeropt"
    const val PREF_PITCHSHIFT = "dsp_pitchshift"
    const val PREF_ECHODELAY = "dsp_echodelay"
    const val PREF_MULTIBANDDIST = "dsp_multibanddist"
    const val PREF_MAXIMIZER = "dsp_maximizer"
    const val PREF_DYNAMICEQ = "dsp_dynamiceq"
    const val PREF_IMAGING = "dsp_imaging"
    const val PREF_TRANSIENT = "dsp_transient"
    const val PREF_LOWEND = "dsp_lowend"
    const val PREF_EXCITER = "dsp_exciter"
    const val PREF_TAPE = "dsp_tape"
    const val PREF_VINYL = "dsp_vinyl"
    const val PREF_BALANCE = "dsp_balance"
    const val PREF_CHAIN_ORDER = "dsp_chain_order"
    const val PREF_FILELIBRARY = "dsp_filelibrary"
    const val KEY_CHAIN_ORDER = "order"

    // Default string values
    const val DEFAULT_CONVOLVER_ADVIMP = "-80;-100;0;0;0;0"
    const val DEFAULT_GEQ = "GraphicEQ: "
    const val DEFAULT_PEQ = "PEQ: "

    /**
     * One low-pass corner, so the card opens on the most useful thing it can
     * do: distort the bass and leave everything above it alone.
     */
    const val DEFAULT_MBD_BANDS = "PEQ: 250.00 48.000000 0.7100 3; "
    const val DEFAULT_EQ = "25.0;40.0;63.0;100.0;160.0;250.0;400.0;630.0;1000.0;1600.0;2500.0;4000.0;6300.0;10000.0;16000.0;0.0;0.0;0.0;0.0;0.0;0.0;0.0;0.0;0.0;0.0;0.0;0.0;0.0;0.0;0.0"

    // Intent actions
    const val ACTION_PREFERENCES_UPDATED = BuildConfig.APPLICATION_ID + ".action.preferences.UPDATED"
    const val ACTION_SAMPLE_RATE_UPDATED = BuildConfig.APPLICATION_ID + ".action.sample_rate.UPDATED"
    const val ACTION_PRESET_LOADED = BuildConfig.APPLICATION_ID + ".action.preset.LOADED"
    const val ACTION_GRAPHIC_EQ_CHANGED = BuildConfig.APPLICATION_ID + ".action.preferences.graphiceq.CHANGED"
    const val ACTION_PARAMETRIC_EQ_CHANGED = BuildConfig.APPLICATION_ID + ".action.preferences.parametriceq.CHANGED"
    const val ACTION_SESSION_CHANGED = BuildConfig.APPLICATION_ID + ".action.session.CHANGED"
    const val ACTION_SERVICE_STARTED = BuildConfig.APPLICATION_ID + ".action.service.STARTED"
    const val ACTION_SERVICE_STOPPED = BuildConfig.APPLICATION_ID + ".action.service.STOPPED"
    const val ACTION_SERVICE_RELOAD_LIVEPROG = BuildConfig.APPLICATION_ID + ".action.service.RELOAD_LIVEPROG"
    const val ACTION_SERVICE_HARD_REBOOT_CORE = BuildConfig.APPLICATION_ID + ".action.service.HARD_REBOOT_CORE"
    const val ACTION_SERVICE_SOFT_REBOOT_CORE = BuildConfig.APPLICATION_ID + ".action.service.SOFT_REBOOT_CORE"
    const val ACTION_PROCESSOR_MESSAGE = BuildConfig.APPLICATION_ID + ".action.service.PROCESSOR_MESSAGE"
    const val ACTION_DISCARD_AUTHORIZATION = BuildConfig.APPLICATION_ID + ".action.service.DISCARD_AUTHORIZATION"
    const val ACTION_REPORT_SAMPLE_RATE = BuildConfig.APPLICATION_ID + ".action.service.REPORT_SAMPLE_RATE"
    const val ACTION_BACKUP_RESTORED = BuildConfig.APPLICATION_ID + ".action.backup.RESTORED"

    // Intent extras
    const val EXTRA_SAMPLE_RATE = BuildConfig.APPLICATION_ID + ".extra.service.SAMPLE_RATE"
}