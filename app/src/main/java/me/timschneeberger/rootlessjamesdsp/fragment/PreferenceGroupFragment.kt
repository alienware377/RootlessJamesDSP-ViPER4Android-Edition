package me.timschneeberger.rootlessjamesdsp.fragment

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.SharedPreferences
import android.os.Bundle
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.annotation.XmlRes
import androidx.appcompat.app.AlertDialog
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.Preference.SummaryProvider
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.PreferenceScreen
import androidx.recyclerview.widget.RecyclerView
import me.timschneeberger.rootlessjamesdsp.MainApplication
import me.timschneeberger.rootlessjamesdsp.R
import me.timschneeberger.rootlessjamesdsp.activity.GraphicEqualizerActivity
import me.timschneeberger.rootlessjamesdsp.activity.LiveprogEditorActivity
import me.timschneeberger.rootlessjamesdsp.activity.ParametricEqualizerActivity
import me.timschneeberger.rootlessjamesdsp.activity.EchoPanelActivity
import me.timschneeberger.rootlessjamesdsp.activity.MultibandDistActivity
import me.timschneeberger.rootlessjamesdsp.activity.LiveprogParamsActivity
import me.timschneeberger.rootlessjamesdsp.adapter.RoundedRipplePreferenceGroupAdapter
import me.timschneeberger.rootlessjamesdsp.liveprog.EelParser
import me.timschneeberger.rootlessjamesdsp.preference.CompanderPreference
import me.timschneeberger.rootlessjamesdsp.preference.EqualizerPreference
import me.timschneeberger.rootlessjamesdsp.preference.FileLibraryPreference
import me.timschneeberger.rootlessjamesdsp.preference.MaterialSeekbarPreference
import me.timschneeberger.rootlessjamesdsp.preference.SwitchPreferenceGroup
import me.timschneeberger.rootlessjamesdsp.utils.AudioSampleRateDetector
import me.timschneeberger.rootlessjamesdsp.utils.ConvolverSampleRateFiles
import me.timschneeberger.rootlessjamesdsp.utils.Constants
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.registerLocalReceiver
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.sendLocalBroadcast
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.unregisterLocalReceiver
import me.timschneeberger.rootlessjamesdsp.utils.preferences.Preferences
import org.koin.core.component.KoinComponent
import org.koin.core.component.inject
import timber.log.Timber
import java.io.File
import kotlin.math.roundToInt


class PreferenceGroupFragment : PreferenceFragmentCompat(), KoinComponent {
    private val prefsApp: Preferences.App by inject()
    private val eelParser = EelParser()
    private var recyclerView: RecyclerView? = null
    private var reportedProcessingSampleRate: Int? = null
    private var convolverStatusUpdater: (() -> Unit)? = null

    private val listener =
        SharedPreferences.OnSharedPreferenceChangeListener { _, _ ->
            requireContext().sendLocalBroadcast(Intent(Constants.ACTION_PREFERENCES_UPDATED))
            convolverStatusUpdater?.invoke()
        }

    private val listenerApp =
        SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
            when(key) {
                context?.resources?.getString(R.string.key_appearance_show_icons) -> updateIconState()
            }
        }

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when(intent?.action) {
                Constants.ACTION_PRESET_LOADED -> {
                    val id = this@PreferenceGroupFragment.id
                    Timber.d("Reloading group fragment for ${this@PreferenceGroupFragment.preferenceManager.sharedPreferencesName}")
                    (requireParentFragment() as DspFragment).restartFragment(id, cloneInstance(this@PreferenceGroupFragment))
                }
                Constants.ACTION_REPORT_SAMPLE_RATE -> {
                    reportedProcessingSampleRate = intent
                        .getFloatExtra(Constants.EXTRA_SAMPLE_RATE, 0f)
                        .roundToInt()
                    convolverStatusUpdater?.invoke()
                }
            }
        }
    }

    private fun updateIconState() {
        if(preferenceScreen.preferenceCount > 0) {
            (preferenceScreen.getPreference(0) as? SwitchPreferenceGroup?)
                ?.setIsIconVisible(prefsApp.get<Boolean>(R.string.key_appearance_show_icons))
        }
    }

    /**
     * The power row is bound before the activity's view exists during restore,
     * so re-read the real state once we're resumed.
     */
    override fun onDestroyView() {
        super.onDestroyView()
        phaseSyncListener?.let {
            preferenceManager.sharedPreferences?.unregisterOnSharedPreferenceChangeListener(it)
        }
        phaseSyncListener = null
    }

    override fun onResume() {
        super.onResume()
        if (arguments?.getInt(BUNDLE_XML_RES) != R.xml.dsp_output_control_preferences) return
        if (!me.timschneeberger.rootlessjamesdsp.utils.V4aIconColors.isClassicLayout(requireContext())) return
        val host = activity as? me.timschneeberger.rootlessjamesdsp.activity.MainActivity ?: return
        val row = preferenceScreen.getPreference(0)
                as? me.timschneeberger.rootlessjamesdsp.preference.SwitchPreferenceGroup ?: return
        row.setValue(host.isPowerOn)
        // Keep following it: the service reports in after this point, so a
        // single read here would leave the switch showing the opposite.
        host.onPowerStateChanged = { on -> row.setValue(on) }
    }

    override fun onPause() {
        super.onPause()
        if (arguments?.getInt(BUNDLE_XML_RES) == R.xml.dsp_output_control_preferences)
            (activity as? me.timschneeberger.rootlessjamesdsp.activity.MainActivity)
                ?.onPowerStateChanged = null
    }

    private var phaseSyncListener: SharedPreferences.OnSharedPreferenceChangeListener? = null

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        val args = requireArguments()
        preferenceManager.sharedPreferencesName = args.getString(BUNDLE_PREF_NAME)
        @Suppress("DEPRECATION")
        preferenceManager.sharedPreferencesMode = Context.MODE_MULTI_PROCESS
        addPreferencesFromResource(args.getInt(BUNDLE_XML_RES))

        // Collapsible "What is this?" info rows (single line when collapsed)
        // V4A-only mode mirrors the original app, which had no explanations
        if (me.timschneeberger.rootlessjamesdsp.utils.V4aMode.isOn(requireContext()))
            findPreference<Preference>("section_info")?.isVisible = false
        findPreference<Preference>("section_info")?.let { p ->
            val full = p.summary
            var expanded = false
            p.title = null
            p.setIcon(R.drawable.ic_twotone_info_24dp)
            p.summary = getString(R.string.section_info_tap_hint)
            p.isSelectable = true
            p.setOnPreferenceClickListener {
                expanded = !expanded
                p.summary = if (expanded) full else getString(R.string.section_info_tap_hint)
                true
            }
        }

        requireContext().registerLocalReceiver(receiver, IntentFilter().apply {
            addAction(Constants.ACTION_PRESET_LOADED)
            addAction(Constants.ACTION_REPORT_SAMPLE_RATE)
        })

        // The ViPER4Android classic theme tints every effect icon purple.
        // The mutated drawable must be assigned back - mutate() can hand back
        // a fresh instance, and tinting that without reassigning left the icon
        // untinted (and, for shared drawables, blank).
        if (me.timschneeberger.rootlessjamesdsp.utils.V4aIconColors.isEnabled(requireContext()) &&
            preferenceScreen.preferenceCount > 0) {
            val pref = preferenceScreen.getPreference(0)
            pref.icon?.let { icon ->
                val tinted = icon.mutate()
                tinted.setTint(me.timschneeberger.rootlessjamesdsp.utils.V4aIconColors.tint(requireContext()))
                pref.icon = tinted
            }
        }

        // Phase mode belongs to the combined FIR filter, so it appears on both
        // EQ cards. They live in different namespaces, so mirror the value to
        // keep them showing one truth.
        findPreference<androidx.preference.TwoStatePreference>(
            getString(R.string.key_geq_linear_phase))?.let { pref ->
            val phaseKey = getString(R.string.key_geq_linear_phase)
            val other = if (args.getInt(BUNDLE_XML_RES) == R.xml.dsp_graphiceq_preferences)
                Constants.PREF_PEQ else Constants.PREF_GEQ
            pref.setOnPreferenceChangeListener { _, v ->
                requireContext().getSharedPreferences(other, Context.MODE_MULTI_PROCESS)
                    .edit().putBoolean(phaseKey, v as Boolean).apply()
                true
            }
            // Both EQ cards can be on screen together, and a write behind a
            // live PreferenceScreen doesn't refresh it - so watch this card's
            // own namespace and move the switch when the other card mirrors in.
            phaseSyncListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
                if (key == phaseKey) pref.isChecked = sp.getBoolean(phaseKey, false)
            }
            preferenceManager.sharedPreferences
                ?.registerOnSharedPreferenceChangeListener(phaseSyncListener)
        }

        when(args.getInt(BUNDLE_XML_RES)) {
            R.xml.dsp_vdynbass_preferences -> {
                val customKeys = arrayOf(
                    R.string.key_vdynbass_x1, R.string.key_vdynbass_x2,
                    R.string.key_vdynbass_y1, R.string.key_vdynbass_y2,
                    R.string.key_vdynbass_sgx, R.string.key_vdynbass_sgy)
                    .map { getString(it) }.toSet()
                val group = findPreference<SwitchPreferenceGroup>(getString(R.string.key_vdynbass_enable))
                val modePref = findPreference<androidx.preference.ListPreference>(getString(R.string.key_vdynbass_mode))
                var isCustom = modePref?.value == "19"
                group?.childVisibilityFilter = { p -> p.key !in customKeys || isCustom }
                group?.refreshChildrenVisibility()
                modePref?.setOnPreferenceChangeListener { _, newValue ->
                    isCustom = newValue == "19"
                    group?.refreshChildrenVisibility()
                    true
                }
            }
            R.xml.dsp_bassex_preferences -> {
                val band2Keys = arrayOf(
                    R.string.key_bassex_cutoff2, R.string.key_bassex_intensity2, R.string.key_bassex_mix2)
                    .map { getString(it) }.toSet()
                val group = findPreference<SwitchPreferenceGroup>(getString(R.string.key_bassex_enable))
                val band2Pref = findPreference<androidx.preference.TwoStatePreference>(getString(R.string.key_bassex_band2_enable))
                var band2On = band2Pref?.isChecked == true
                group?.childVisibilityFilter = { p -> p.key !in band2Keys || band2On }
                group?.refreshChildrenVisibility()
                band2Pref?.setOnPreferenceChangeListener { _, newValue ->
                    band2On = newValue == true
                    group?.refreshChildrenVisibility()
                    true
                }
            }
            R.xml.dsp_convolver_preferences -> setupConvolverSampleRateFiles()
            R.xml.dsp_compander_preferences -> {
                findPreference<MaterialSeekbarPreference>(getString(R.string.key_compander_granularity))?.valueLabelOverride =
                    fun(it: Float): String {
                        return when(it.roundToInt()) {
                            0 -> getString(R.string.compander_granularity_very_low)
                            1 -> getString(R.string.compander_granularity_low)
                            2 -> getString(R.string.compander_granularity_medium)
                            3 -> getString(R.string.compander_granularity_high)
                            4 -> getString(R.string.compander_granularity_extreme)
                            else -> it.roundToInt().toString()
                        }
                    }
            }
            R.xml.dsp_stereowide_preferences -> {
                findPreference<MaterialSeekbarPreference>(getString(R.string.key_stereowide_mode))?.valueLabelOverride =
                    fun(it: Float): String {
                        return if (it in 49.0..51.0)
                            getString(R.string.stereowide_level_none)
                        else if(it >= 60)
                            getString(R.string.stereowide_level_very_wide)
                        else if(it >= 51)
                            getString(R.string.stereowide_level_wide)
                        else if(it <= 40)
                            getString(R.string.stereowide_level_very_narrow)
                        else if(it <= 49)
                            getString(R.string.stereowide_level_narrow)
                        else
                            it.toString()
                    }
            }
            R.xml.dsp_echo_preferences -> {
                findPreference<Preference>(getString(R.string.key_echo_open_panel))
                    ?.setOnPreferenceClickListener {
                        startActivity(Intent(requireContext(), EchoPanelActivity::class.java))
                        true
                    }
            }
            R.xml.dsp_multibanddist_preferences -> {
                findPreference<Preference>(getString(R.string.key_mbd_open_panel))
                    ?.setOnPreferenceClickListener {
                        startActivity(Intent(requireContext(), MultibandDistActivity::class.java))
                        true
                    }
            }
            R.xml.dsp_output_control_preferences -> {
                val v4a = me.timschneeberger.rootlessjamesdsp.utils.V4aMode.isOn(requireContext())
                // Classic layout: this card's header switch is the master power
                // control, exactly as V4A's Master limiter row was.
                if (me.timschneeberger.rootlessjamesdsp.utils.V4aIconColors.isClassicLayout(requireContext())) {
                    (preferenceScreen.getPreference(0) as? me.timschneeberger.rootlessjamesdsp.preference.SwitchPreferenceGroup)?.apply {
                        isEnabled = true
                        isSelectable = true
                        val host = activity as? me.timschneeberger.rootlessjamesdsp.activity.MainActivity
                        // Safe even during restore: isPowerOn reports false
                        // until the activity's view exists, and onResume syncs.
                        setValue(host?.isPowerOn ?: false)
                        // The service (and permission flow) decide the real
                        // state, so route the tap rather than setting it here.
                        onUserToggle = {
                            val a = activity as? me.timschneeberger.rootlessjamesdsp.activity.MainActivity
                            a?.requestPowerToggle()
                            // No optimistic state here - onPowerStateChanged
                            // reports the real result back to this switch.
                        }
                    }
                }
                val modePref = findPreference<ListPreference>(getString(R.string.key_limiter_mode))
                if (v4a) {
                    // Original V4A master limiter: output gain, limit threshold,
                    // limit release - no mode selector (V4A is a peak limiter).
                    (preferenceScreen.getPreference(0) as? Preference)
                        ?.setTitle(R.string.v4a_master_limiter)
                    findPreference<Preference>(getString(R.string.key_output_postgain))
                        ?.setTitle(R.string.v4a_output_gain)
                    findPreference<Preference>(getString(R.string.key_limiter_threshold))
                        ?.setTitle(R.string.v4a_limit_threshold)
                    findPreference<Preference>(getString(R.string.key_limiter_release))
                        ?.setTitle(R.string.v4a_limit_release)
                    modePref?.isVisible = false
                } else {
                    // Hide limiter params the selected mode ignores: Off uses
                    // neither, soft saturation shapes by threshold only.
                    fun apply(mode: String) {
                        findPreference<Preference>(getString(R.string.key_limiter_threshold))?.isVisible = mode != "2"
                        findPreference<Preference>(getString(R.string.key_limiter_release))?.isVisible = mode == "0" || mode == "3"
                    }
                    apply(modePref?.value ?: "0")
                    modePref?.setOnPreferenceChangeListener { _, v ->
                        apply(v as? String ?: "0"); true
                    }
                }
            }
            R.xml.dsp_equalizer_preferences -> {
                if (me.timschneeberger.rootlessjamesdsp.utils.V4aMode.isOn(requireContext()))
                    findPreference<Preference>(getString(R.string.key_eq_enable))
                        ?.setTitle(R.string.v4a_fir_equalizer)
            }
            R.xml.dsp_vreverb_preferences -> {
                if (me.timschneeberger.rootlessjamesdsp.utils.V4aMode.isOn(requireContext())) {
                    findPreference<ListPreference>(getString(R.string.key_vreverb_model))
                        ?.isVisible = false
                    arrayOf(R.string.key_vreverb_predelay, R.string.key_vreverb_decay,
                        R.string.key_vreverb_diffusion, R.string.key_vreverb_mod,
                        R.string.key_vreverb_bass, R.string.key_vreverb_er)
                        .forEach { findPreference<Preference>(getString(it))?.isVisible = false }
                    // No early return here: icon visibility is applied after
                    // this block, and skipping it hid the reverb icon.
                } else {
                // Each room type exposes only the controls it actually uses, so
                // the card never shows a slider that does nothing.
                fun applyModelVisibility(model: String) {
                    val plate = model == "1"
                    val hall = model == "2"
                    val room = model == "3"
                    val advanced = plate || hall || room
                    findPreference<Preference>(getString(R.string.key_vreverb_predelay))?.isVisible = advanced
                    findPreference<Preference>(getString(R.string.key_vreverb_decay))?.isVisible = advanced
                    findPreference<Preference>(getString(R.string.key_vreverb_diffusion))?.isVisible = plate
                    findPreference<Preference>(getString(R.string.key_vreverb_mod))?.isVisible = plate
                    findPreference<Preference>(getString(R.string.key_vreverb_bass))?.isVisible = hall || room
                    findPreference<Preference>(getString(R.string.key_vreverb_er))?.isVisible = room
                }
                val modelPref = findPreference<ListPreference>(getString(R.string.key_vreverb_model))
                applyModelVisibility(modelPref?.value ?: "0")
                modelPref?.setOnPreferenceChangeListener { _, newValue ->
                    applyModelVisibility(newValue as? String ?: "0")
                    true
                }
                }
            }
            R.xml.dsp_liveprog_preferences,
            R.xml.dsp_liveprog2_preferences,
            R.xml.dsp_liveprog3_preferences,
            R.xml.dsp_liveprog4_preferences -> {
                // Each chained slot has its own file/params/edit keys but shares
                // all of the behaviour below.
                val slotKeys = when (args.getInt(BUNDLE_XML_RES)) {
                    R.xml.dsp_liveprog2_preferences -> Triple(
                        R.string.key_liveprog2_file, R.string.key_liveprog2_params, R.string.key_liveprog2_edit)
                    R.xml.dsp_liveprog3_preferences -> Triple(
                        R.string.key_liveprog3_file, R.string.key_liveprog3_params, R.string.key_liveprog3_edit)
                    R.xml.dsp_liveprog4_preferences -> Triple(
                        R.string.key_liveprog4_file, R.string.key_liveprog4_params, R.string.key_liveprog4_edit)
                    else -> Triple(
                        R.string.key_liveprog_file, R.string.key_liveprog_params, R.string.key_liveprog_edit)
                }
                val liveprogParams = findPreference<Preference>(getString(slotKeys.second))
                val liveprogEdit = findPreference<Preference>(getString(slotKeys.third))
                val liveprogFile = findPreference<FileLibraryPreference>(getString(slotKeys.first))

                // Slots 2-4 are driven by the multi-selection made in the first
                // card, so they display their script but don't open a picker.
                if (args.getInt(BUNDLE_XML_RES) != R.xml.dsp_liveprog_preferences) {
                    liveprogFile?.isSelectable = false
                    liveprogFile?.setTitle(R.string.liveprog_file)
                }

                fun updateLiveprog(newValue: String) {
                    eelParser.load(FileLibraryPreference.createFullPathCompat(requireContext(), newValue))
                    val count = eelParser.properties.size
                    val filePresent = eelParser.contents != null
                    val uiUpdate = {
                        liveprogEdit?.isEnabled = filePresent

                        liveprogParams?.isEnabled = count > 0

                        try {
                            liveprogParams?.summary = if (count > 0)
                                resources.getQuantityString(R.plurals.custom_parameters, count, count)
                            else
                                getString(R.string.liveprog_additional_params_not_supported)
                        }
                        catch(ex: IllegalStateException) {
                            /* Because this lambda is executed async, it is possible that it is called
                               while the fragment is destroyed, leading to accessing a detached context */
                            Timber.d(ex)
                        }
                    }

                    if (recyclerView == null)
                        // Recycler view doesn't exist yet, directly setup the preference
                        uiUpdate()
                    else
                        // Recycler view does exist, queue on UI thread
                        recyclerView!!.post(uiUpdate)
                }

                liveprogFile?.summaryProvider = SummaryProvider<FileLibraryPreference> {
                    updateLiveprog(it.value)
                    if(it.value == null || it.value.isBlank() || !eelParser.isFileLoaded) {
                        getString(R.string.liveprog_no_script_selected)
                    }
                    else
                        eelParser.description
                }

                FileLibraryPreference.createFullPathNullCompat(requireContext(), liveprogFile?.value)?.let {
                    updateLiveprog(it)
                }

                liveprogFile?.setOnPreferenceChangeListener { _, newValue ->
                    updateLiveprog(newValue as String)
                    true
                }

                liveprogParams?.setOnPreferenceClickListener {
                    val intent = Intent(requireContext(), LiveprogParamsActivity::class.java)
                    intent.putExtra(LiveprogParamsActivity.EXTRA_TARGET_FILE, FileLibraryPreference.createFullPathNullCompat(requireContext(), liveprogFile?.value))
                    startActivity(intent)
                    true
                }

                liveprogEdit?.setOnPreferenceClickListener {
                    val intent = Intent(requireContext(), LiveprogEditorActivity::class.java)
                    intent.putExtra(LiveprogEditorActivity.EXTRA_TARGET_FILE, FileLibraryPreference.createFullPathNullCompat(requireContext(), liveprogFile?.value))
                    startActivity(intent)
                    true
                }
            }
            R.xml.dsp_graphiceq_preferences -> {
                findPreference<Preference>(getString(R.string.key_geq_nodes))?.setOnPreferenceClickListener {
                    val intent = Intent(requireContext(), GraphicEqualizerActivity::class.java)
                    startActivity(intent)
                    true
                }
            }
            R.xml.dsp_parametriceq_preferences -> {
                findPreference<Preference>(getString(R.string.key_peq_bands))?.setOnPreferenceClickListener {
                    val intent = Intent(requireContext(), ParametricEqualizerActivity::class.java)
                    startActivity(intent)
                    true
                }
            }
        }

        updateIconState()

        preferenceManager.sharedPreferences?.registerOnSharedPreferenceChangeListener(listener)
        prefsApp.registerOnSharedPreferenceChangeListener(listenerApp)
    }

    private fun setupConvolverSampleRateFiles() {
        val assignmentPreference = findPreference<Preference>(
            getString(R.string.key_convolver_sample_rate_files)
        ) ?: return
        val filePreference = findPreference<FileLibraryPreference>(
            getString(R.string.key_convolver_file)
        ) ?: return
        val processingPreference = findPreference<Preference>(
            getString(R.string.key_convolver_processing_rate)
        ) ?: return
        val sharedPreferences = preferenceManager.sharedPreferences ?: return
        val assignmentKey = getString(R.string.key_convolver_sample_rate_files)

        fun assignments() = ConvolverSampleRateFiles.decode(
            sharedPreferences.getString(assignmentKey, "").orEmpty()
        )

        fun processingRate(): Int = reportedProcessingSampleRate
            ?: (requireActivity().application as MainApplication).engineSampleRate.roundToInt()

        fun selectedImpulseResponse(rate: Int): String {
            if (!sharedPreferences.getBoolean(getString(R.string.key_convolver_enable), false)) {
                return getString(R.string.convolver_sample_rate_disabled)
            }

            val fallbackFile = filePreference.value.orEmpty()
            val mappedFile = ConvolverSampleRateFiles.resolve(
                sharedPreferences.getString(assignmentKey, "").orEmpty(),
                rate,
                fallbackFile,
            )
            val selectedFile = mappedFile.takeIf {
                File(FileLibraryPreference.createFullPathCompat(requireContext(), it)).isFile
            } ?: fallbackFile
            return selectedFile.takeIf(String::isNotBlank)
                ?.let { File(it).nameWithoutExtension }
                ?: getString(R.string.convolver_sample_rate_no_file)
        }

        fun updateSummary() {
            val count = assignments().size
            val rate = processingRate()
            processingPreference.summary = if (rate > 0) {
                getString(
                    R.string.convolver_processing_rate_status,
                    ConvolverSampleRateFiles.formatKilohertz(rate),
                    selectedImpulseResponse(rate),
                )
            } else {
                getString(R.string.convolver_sample_rate_processing_inactive)
            }
            val assignmentsSummary = if (count == 0)
                getString(R.string.convolver_sample_rate_files_summary)
            else
                getString(R.string.convolver_sample_rate_files_count, count)
            assignmentPreference.summary = assignmentsSummary
        }

        convolverStatusUpdater = ::updateSummary

        assignmentPreference.setOnPreferenceClickListener {
            filePreference.refresh()
            val assignedFiles = assignments()
            val detectedRates = AudioSampleRateDetector.getOutputSampleRates(
                requireContext(),
            )
            val sampleRates = (detectedRates + assignedFiles.keys)
                .filter(ConvolverSampleRateFiles::isSupportedSampleRate)
                .distinct()
                .sorted()
            val rateLabels = sampleRates.map { rate ->
                val fileName = assignedFiles[rate]
                    ?.let { File(it).nameWithoutExtension }
                    ?: getString(R.string.convolver_sample_rate_unassigned)
                getString(
                    R.string.convolver_sample_rate_label,
                    ConvolverSampleRateFiles.formatKilohertz(rate),
                    fileName,
                )
            }.toTypedArray()

            AlertDialog.Builder(requireContext())
                .setTitle(R.string.convolver_sample_rate_files)
                .setItems(rateLabels) { _, rateIndex ->
                    val rate = sampleRates[rateIndex]
                    val choices = arrayOf(getString(R.string.convolver_sample_rate_use_default)) +
                        filePreference.entries.map(CharSequence::toString)

                    AlertDialog.Builder(requireContext())
                        .setTitle(
                            getString(
                                R.string.convolver_sample_rate_select_file,
                                ConvolverSampleRateFiles.formatKilohertz(rate),
                            )
                        )
                        .setItems(choices) { _, fileIndex ->
                            val updatedAssignments = assignments().toMutableMap()
                            if (fileIndex == 0) {
                                updatedAssignments.remove(rate)
                            } else {
                                updatedAssignments[rate] =
                                    filePreference.entryValues[fileIndex - 1].toString()
                            }
                            sharedPreferences.edit()
                                .putString(
                                    assignmentKey,
                                    ConvolverSampleRateFiles.encode(updatedAssignments),
                                )
                                .apply()
                            updateSummary()
                        }
                        .setNegativeButton(android.R.string.cancel, null)
                        .show()
                }
                .setNegativeButton(android.R.string.cancel, null)
                .show()
            true
        }

        updateSummary()
    }

    override fun onCreateRecyclerView(
        inflater: LayoutInflater,
        parent: ViewGroup,
        savedInstanceState: Bundle?,
    ): RecyclerView {
        return super.onCreateRecyclerView(inflater, parent, savedInstanceState).apply {
            itemAnimator = null // Fix to prevent RecyclerView crash if group is toggled rapidly
            isNestedScrollingEnabled = false

            this@PreferenceGroupFragment.recyclerView = this
        }
    }

    override fun onCreateAdapter(preferenceScreen: PreferenceScreen): RecyclerView.Adapter<*> {
        return RoundedRipplePreferenceGroupAdapter(preferenceScreen)
    }

    override fun onDestroy() {
        convolverStatusUpdater = null
        super.onDestroy()
        requireContext().unregisterLocalReceiver(receiver)
        prefsApp.unregisterOnSharedPreferenceChangeListener(listener)
        preferenceManager.sharedPreferences?.unregisterOnSharedPreferenceChangeListener(listener)
    }

    @Suppress("DEPRECATION")
    override fun onDisplayPreferenceDialog(preference: Preference) {
        when (preference) {
            is EqualizerPreference -> {
                val dialogFragment = EqualizerDialogFragment.newInstance(preference.key)
                dialogFragment.setTargetFragment(this, 0)
                dialogFragment.show(parentFragmentManager, null)
            }
            is CompanderPreference -> {
                val dialogFragment = CompanderDialogFragment.newInstance(preference.key)
                dialogFragment.setTargetFragment(this, 0)
                dialogFragment.show(parentFragmentManager, null)
            }
            is FileLibraryPreference -> {
                val dialogFragment = FileLibraryDialogFragment.newInstance(preference.key)
                dialogFragment.setTargetFragment(this, 0)
                dialogFragment.show(parentFragmentManager, null)
            }
            else -> super.onDisplayPreferenceDialog(preference)
        }
    }

    companion object {
        private const val BUNDLE_PREF_NAME = "preferencesName"
        private const val BUNDLE_XML_RES = "preferencesXmlRes"

        fun newInstance(preferencesName: String?, @XmlRes preferencesXmlRes: Int): PreferenceGroupFragment {
            return PreferenceGroupFragment().apply {
                arguments = Bundle().apply {
                    putString(BUNDLE_PREF_NAME, preferencesName)
                    putInt(BUNDLE_XML_RES, preferencesXmlRes)
                }
            }
        }

        fun cloneInstance(fragment: PreferenceGroupFragment): PreferenceGroupFragment {
            return fragment.requireArguments().let { args ->
                 newInstance(args.getString(BUNDLE_PREF_NAME), args.getInt(BUNDLE_XML_RES))
            }
        }
    }
}
