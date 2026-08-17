package me.timschneeberger.rootlessjamesdsp.fragment

import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.res.Configuration
import android.content.res.Configuration.ORIENTATION_LANDSCAPE
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import me.timschneeberger.rootlessjamesdsp.R
import me.timschneeberger.rootlessjamesdsp.activity.ParametricEqualizerActivity
import me.timschneeberger.rootlessjamesdsp.adapter.ParametricEqBandAdapter
import me.timschneeberger.rootlessjamesdsp.databinding.FragmentParametricEqBinding
import me.timschneeberger.rootlessjamesdsp.model.ParametricEqBand
import me.timschneeberger.rootlessjamesdsp.model.ParametricEqBandList
import me.timschneeberger.rootlessjamesdsp.model.ParametricEqFilterType
import me.timschneeberger.rootlessjamesdsp.utils.BiquadUtils
import me.timschneeberger.rootlessjamesdsp.utils.Constants
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.registerLocalReceiver
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.sendLocalBroadcast
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.showInputAlert
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.showYesNoAlert
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.toast
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.unregisterLocalReceiver
import timber.log.Timber
import java.util.UUID

class ParametricEqualizerFragment : Fragment() {
    private lateinit var binding: FragmentParametricEqBinding

    private val adapter: ParametricEqBandAdapter
        get() = binding.bandList.adapter as ParametricEqBandAdapter

    private var editorBandBackup: ParametricEqBand? = null
    private var editorBandUuid: UUID? = null
    private var editorActive = false
        set(value) {
            field = value
            binding.add.isEnabled = !value
            // Remove acts on the selected band, so it is only usable while one
            // is actually selected.
            binding.remove.isEnabled = value
            binding.reset.isEnabled = !value
            binding.importFile.isEnabled = !value
            binding.exportFile.isEnabled = !value
            binding.editString.isEnabled = !value
        }

    private val importFileLauncher =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
            uri ?: return@registerForActivityResult
            try {
                val text = requireContext().contentResolver.openInputStream(uri)
                    ?.bufferedReader()?.use { it.readText() } ?: return@registerForActivityResult
                val result = adapter.bands.fromApoString(text)

                // Apply imported preamp
                binding.preampInput.value = result.preampDb.toFloat()
                binding.equalizerSurface.setPreampDb(result.preampDb)

                sortBandsByFrequency()
                binding.equalizerSurface.setBands(adapter.bands, result.preampDb)
                adapter.notifyDataSetChanged()
                save()
                updateViewState()
                val msg = getString(R.string.peq_import_success, adapter.bands.size)
                if (result.skippedFilters > 0) {
                    requireContext().toast("$msg (${result.skippedFilters} unsupported filters skipped)")
                } else {
                    requireContext().toast(msg)
                }
            } catch (e: Exception) {
                Timber.e(e, "Failed to import PEQ file")
                requireContext().toast(R.string.peq_import_error)
            }
        }

    private val exportFileLauncher =
        registerForActivityResult(ActivityResultContracts.CreateDocument("text/plain")) { uri ->
            uri ?: return@registerForActivityResult
            try {
                val apoString = adapter.bands.toApoString(binding.preampInput.value.toDouble())
                requireContext().contentResolver.openOutputStream(uri)?.bufferedWriter()?.use {
                    it.write(apoString)
                }
                requireContext().toast(R.string.peq_export_success)
            } catch (e: Exception) {
                Timber.e(e, "Failed to export PEQ file")
                requireContext().toast("Export failed: ${e.message}")
            }
        }

    private val broadcastReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                Constants.ACTION_PRESET_LOADED -> {
                    activity?.finish()
                    startActivity(Intent(requireContext(), ParametricEqualizerActivity::class.java).apply {
                        flags = Intent.FLAG_ACTIVITY_CLEAR_TOP
                    })
                }
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        requireContext().registerLocalReceiver(broadcastReceiver, IntentFilter(Constants.ACTION_PRESET_LOADED))
        super.onCreate(savedInstanceState)
    }

    override fun onDestroy() {
        requireContext().unregisterLocalReceiver(broadcastReceiver)
        super.onDestroy()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        binding = FragmentParametricEqBinding.inflate(layoutInflater, container, false)

        binding.previewCard.setOnClickListener {
            if (resources.configuration.orientation != ORIENTATION_LANDSCAPE) {
                val newState = !binding.equalizerSurface.isVisible
                collapsePreview(newState)
            }
        }

        binding.reset.setOnClickListener {
            requireContext().showYesNoAlert(
                R.string.peq_reset_confirm_title,
                R.string.peq_reset_confirm,
            ) {
                if (it) {
                    adapter.bands.deserialize(Constants.DEFAULT_PEQ)
                    binding.preampInput.value = 0f
                    binding.equalizerSurface.setPreampDb(0.0)
                    updateViewState()
                    editorDiscard()
                    save()
                }
            }
        }

        binding.editString.setOnClickListener {
            requireContext().showInputAlert(
                layoutInflater,
                R.string.peq_edit_as_string,
                R.string.peq_edit_hint,
                adapter.bands.toApoString(binding.preampInput.value.toDouble()),
                false,
                null
            ) {
                it?.let { text ->
                    val result = adapter.bands.fromApoString(text)
                    // Apply parsed preamp
                    binding.preampInput.value = result.preampDb.toFloat()
                    binding.equalizerSurface.setPreampDb(result.preampDb)
                }
                save()
            }
        }

        binding.add.setOnClickListener {
            if (editorActive) return@setOnClickListener

            editorBandBackup = null
            editorBandUuid = null
            editorActive = true

            binding.freqInput.value = 1000f
            binding.gainInput.value = 0f
            binding.qInput.value = 1.41f
            setFilterTypeSelection(ParametricEqFilterType.PEAKING)
            updateViewState()
        }

        binding.remove.isEnabled = false
        binding.remove.setOnClickListener {
            // Mirrors long-pressing a handle, for anyone who hasn't found that
            val target = selectedForEdit
            if (target == null) {
                requireContext().toast(R.string.peq_remove_none)
                return@setOnClickListener
            }
            pushHistory(snapshot())
            adapter.bands.remove(target)
            selectedForEdit = null
            editorActive = false
            editorBandUuid = null
            editorActive = false
            sortBandsByFrequency()
            binding.equalizerSurface.setBands(adapter.bands, binding.preampInput.value.toDouble())
            binding.equalizerSurface.selectBand(-1)
            adapter.notifyDataSetChanged()
            updateViewState()
            save()
        }

        binding.importFile.setOnClickListener {
            importFileLauncher.launch(arrayOf("text/plain", "text/*"))
        }

        binding.exportFile.setOnClickListener {
            exportFileLauncher.launch("parametric_eq.txt")
        }

        binding.freqInput.setOnValueChangedListener { editorApply() }
        binding.gainInput.setOnValueChangedListener { editorApply() }
        binding.qInput.setOnValueChangedListener { editorApply() }

        binding.filterTypeGroup.addOnButtonCheckedListener { _, _, isChecked ->
            if (isChecked) editorApply()
        }

        binding.confirm.setOnClickListener {
            editorSave()
        }

        binding.cancel.setOnClickListener {
            editorDiscard()
        }

        // Preamp listener
        binding.preampInput.setOnValueChangedListener {
            binding.equalizerSurface.setPreampDb(binding.preampInput.value.toDouble())
            savePreamp()
        }

        // Load band data
        binding.bandList.layoutManager = LinearLayoutManager(requireContext())
        loadBands(savedInstanceState)
        setupInteractiveGraph()

        updateViewState()
        return binding.root
    }

    // ------------------------------------------------------- undo / redo

    private val undoStack = ArrayDeque<String>()
    private val redoStack = ArrayDeque<String>()
    private var restoringHistory = false

    private fun snapshot() = adapter.bands.serialize()

    /** Records the state *before* a change so it can be undone. */
    private fun pushHistory(previous: String) {
        if (restoringHistory) return
        if (undoStack.lastOrNull() == previous) return
        undoStack.addLast(previous)
        while (undoStack.size > 50) undoStack.removeFirst()
        redoStack.clear()
        updateHistoryButtons()
    }

    private fun applySerialized(value: String) {
        restoringHistory = true
        adapter.bands.deserialize(value)
        adapter.notifyDataSetChanged()
        binding.equalizerSurface.setBands(adapter.bands, binding.preampInput.value.toDouble())
        updateViewState()
        save()
        restoringHistory = false
        updateHistoryButtons()
    }

    private fun undo() {
        val previous = undoStack.removeLastOrNull() ?: return
        redoStack.addLast(snapshot())
        applySerialized(previous)
    }

    private fun redo() {
        val next = redoStack.removeLastOrNull() ?: return
        undoStack.addLast(snapshot())
        applySerialized(next)
    }

    private fun updateHistoryButtons() {
        binding.undo.isEnabled = undoStack.isNotEmpty()
        binding.redo.isEnabled = redoStack.isNotEmpty()
        binding.undo.alpha = if (undoStack.isNotEmpty()) 1f else 0.4f
        binding.redo.alpha = if (redoStack.isNotEmpty()) 1f else 0.4f
    }

    /** Makes the response graph directly editable: drag, pinch, long-press. */
    private fun setupInteractiveGraph() {
        val surface = binding.equalizerSurface
        surface.interactive = true

        surface.onBandsChanged = { bands ->
            // A drag is one continuous gesture; snapshot only when it starts
            binding.equalizerSurface.setBands(bands, binding.preampInput.value.toDouble())
            adapter.notifyDataSetChanged()
            save()
        }

        surface.onDragFinished = {
            sortBandsByFrequency()
            binding.equalizerSurface.setBands(adapter.bands, binding.preampInput.value.toDouble())
            // The knobs still hold the values from when the band was selected;
            // without this, touching a knob right after a drag writes those
            // stale values back and the handle jumps to its old position.
            selectedForEdit?.let { band ->
                binding.freqInput.value = band.frequency.toFloat()
                binding.gainInput.value = band.gain.toFloat()
                binding.qInput.value = band.q.toFloat()
            }
        }

        surface.onBandSelected = { index ->
            pushHistory(snapshot())
            val band = adapter.bands.getOrNull(index)
            if (band != null) {
                // Show this band's knobs straight away - no extra tap needed
                selectedForEdit = band
                editorBandUuid = band.uuid
                editorBandBackup = null
                editorActive = true
                binding.freqInput.value = band.frequency.toFloat()
                binding.gainInput.value = band.gain.toFloat()
                binding.qInput.value = band.q.toFloat()
                updateViewState()
            }
        }

        surface.onSelectionCleared = {
            if (editorActive) {
                // Tapped away from every handle: back to the frequency list
                editorBandUuid = null
                editorBandBackup = null
                editorActive = false
                sortBandsByFrequency()
                updateViewState()
            }
        }

        surface.onBandAddRequested = { frequency, gain ->
            pushHistory(snapshot())
            adapter.bands.add(
                ParametricEqBand(
                    frequency.coerceIn(20.0, 20000.0),
                    gain.coerceIn(-30.0, 30.0),
                    1.41
                )
            )
            sortBandsByFrequency()
            adapter.notifyDataSetChanged()
            surface.setBands(adapter.bands, binding.preampInput.value.toDouble())
            updateViewState()
            save()
        }

        surface.onBandRemoveRequested = { index ->
            if (index in adapter.bands.indices) {
                pushHistory(snapshot())
                adapter.bands.removeAt(index)
                adapter.notifyDataSetChanged()
                surface.setBands(adapter.bands, binding.preampInput.value.toDouble())
                updateViewState()
                save()
            }
        }

        binding.undo.setOnClickListener { undo() }
        binding.redo.setOnClickListener { redo() }
        updateHistoryButtons()
    }

    private var selectedForEdit: ParametricEqBand? = null

    private fun loadBands(savedInstanceState: Bundle?) {
        val bands = ParametricEqBandList()
        val prefs = requireContext().getSharedPreferences(Constants.PREF_PEQ, Context.MODE_PRIVATE)
        val dataSaved = savedInstanceState?.getBundle(STATE_BANDS)
        if (dataSaved != null) {
            bands.fromBundle(dataSaved)
        } else {
            val bandString = prefs?.getString(getString(R.string.key_peq_bands), Constants.DEFAULT_PEQ)!!
            bands.deserialize(bandString)
        }

        // Load preamp
        val preampDb = prefs?.getFloat(getString(R.string.key_peq_preamp), 0f) ?: 0f
        binding.preampInput.value = preampDb
        binding.equalizerSurface.setBands(bands, preampDb.toDouble())

        binding.bandList.adapter = ParametricEqBandAdapter(bands).apply {
            onItemsChanged = {
                binding.equalizerSurface.setBands(it.bands, binding.preampInput.value.toDouble())
                updateViewState()
                save()
            }

            onItemClicked = { band: ParametricEqBand, _: Int ->
                editorBandBackup = band
                editorBandUuid = band.uuid
                editorActive = true

                binding.freqInput.value = band.frequency.toFloat()
                binding.qInput.value = band.q.toFloat()
                // Type before gain: the gain knob's range depends on the type,
                // and the knob clamps whatever it is given to the range it has
                // at that moment.
                setFilterTypeSelection(band.filterType)
                applyGainRole(band.gain)
                updateViewState()
            }
        }
    }

    /**
     * Point the gain dial at whichever quantity the current filter type has.
     *
     * A cutoff has no gain - the coefficients ignore the term - but it does have
     * a slope, and without a control for it a cutoff is stuck at 12 dB/octave,
     * which reads as a tilt across the whole spectrum rather than a corner. So
     * the dial becomes the slope for those types instead of being dimmed. The
     * band's gain field is what carries the slope; see BiquadUtils.cascadeFor.
     *
     * @param gain the band's stored value, or null to keep what the dial shows
     */
    private fun applyGainRole(gain: Double? = null) {
        val cutoff = getSelectedFilterType().let {
            it == ParametricEqFilterType.LOW_PASS || it == ParametricEqFilterType.HIGH_PASS
        }
        val wasSlope = binding.gainInput.unit == SLOPE_UNIT
        with(binding.gainInput) {
            if (cutoff) {
                // Carrying a gain over into the slope would read as a slope
                // nobody picked, so a fresh cutoff starts at the default.
                val v = gain ?: if (wasSlope) value.toDouble() else 0.0
                label = getString(R.string.mbd_knob_slope)
                unit = SLOPE_UNIT
                precision = 0
                minValue = 12f
                maxValue = BiquadUtils.MAX_CUTOFF_SLOPE.toFloat()
                this.value = if (v < 6.0) 48f else v.toFloat()
            } else {
                val v = gain ?: if (wasSlope) 0.0 else value.toDouble()
                label = getString(R.string.peq_gain)
                unit = "dB"
                precision = 1
                minValue = -30f
                maxValue = 30f
                this.value = v.toFloat()
            }
            isEnabled = true
            alpha = 1f
        }
    }

    private fun getSelectedFilterType(): ParametricEqFilterType {
        return when (binding.filterTypeGroup.checkedButtonId) {
            R.id.filter_low_shelf -> ParametricEqFilterType.LOW_SHELF
            R.id.filter_high_shelf -> ParametricEqFilterType.HIGH_SHELF
            R.id.filter_low_pass -> ParametricEqFilterType.LOW_PASS
            R.id.filter_high_pass -> ParametricEqFilterType.HIGH_PASS
            else -> ParametricEqFilterType.PEAKING
        }
    }

    private fun setFilterTypeSelection(type: ParametricEqFilterType) {
        val buttonId = when (type) {
            ParametricEqFilterType.PEAKING -> R.id.filter_peaking
            ParametricEqFilterType.LOW_SHELF -> R.id.filter_low_shelf
            ParametricEqFilterType.HIGH_SHELF -> R.id.filter_high_shelf
            ParametricEqFilterType.LOW_PASS -> R.id.filter_low_pass
            ParametricEqFilterType.HIGH_PASS -> R.id.filter_high_pass
        }
        binding.filterTypeGroup.check(buttonId)
        applyGainRole()
    }

    /** Keeps the band list in ascending frequency order. */
    @SuppressLint("NotifyDataSetChanged")
    private fun sortBandsByFrequency() {
        if (adapter.bands.size < 2) return
        val sorted = adapter.bands.sortedBy { it.frequency }
        if (sorted == adapter.bands.toList()) return
        adapter.bands.clear()
        adapter.bands.addAll(sorted)
        adapter.notifyDataSetChanged()
    }

    private fun updateViewState() {
        val empty = adapter.bands.isEmpty()
        binding.emptyView.isVisible = empty && !editorActive
        binding.bandList.isVisible = !empty && !editorActive
        binding.bandEdit.isVisible = editorActive
        // Knob changes apply live - Undo is the safety net, so no confirm/discard
        binding.bandDetailContextButtons.visibility = View.GONE
        binding.editCardTitle.text = getString(if (editorActive) R.string.peq_band_editor else R.string.peq_band_list)
    }

    override fun onStop() {
        // Edits are applied as they're made, so there is nothing to discard
        super.onStop()
    }

    private fun editorCanSave(): Boolean {
        val freqValid = binding.freqInput.isCurrentValueValid()
        val gainValid = binding.gainInput.isCurrentValueValid()
        val qValid = binding.qInput.isCurrentValueValid()
        return freqValid && gainValid && qValid
    }

    private fun editorApply() {
        if (editorCanSave()) {
            val uuid = editorBandUuid
            val freq = binding.freqInput.value.toDouble()
            val gain = binding.gainInput.value.toDouble()
            val q = binding.qInput.value.toDouble()
            val filterType = getSelectedFilterType()

            if (uuid == null) {
                val band = ParametricEqBand(freq, gain, q, filterType)
                adapter.bands.add(band)
                editorBandUuid = band.uuid
                Timber.d("editorApply: tracking new band $editorBandUuid for $freq Hz $gain dB Q$q $filterType")
            } else {
                Timber.d("editorApply: modifying band $editorBandUuid")
                val index = adapter.bands.indexOfFirst { it.uuid == uuid }
                if (index < 0)
                    Timber.e("editorApply: failed to find matching band UUID")
                else
                    adapter.bands[index] = ParametricEqBand(freq, gain, q, filterType, uuid)
            }
        }
    }

    private fun editorDiscard() {
        val uuid = editorBandUuid
        if (editorBandBackup != null && uuid != null) {
            Timber.d("editorDiscard: reverting modifications to band $uuid")
            val index = adapter.bands.indexOfFirst { it.uuid == uuid }
            if (index < 0)
                Timber.e("editorDiscard: failed to find matching band UUID")
            else
                adapter.bands[index] = editorBandBackup
        } else if (uuid != null) {
            Timber.d("editorDiscard: reverting addition of band $uuid")
            adapter.bands.removeAll { it.uuid == uuid }
        }

        editorBandBackup = null
        editorBandUuid = null
        editorActive = false
        updateViewState()
    }

    @SuppressLint("NotifyDataSetChanged")
    private fun editorSave() {
        if (!editorCanSave()) {
            requireContext().showYesNoAlert(
                R.string.peq_discard_changes_title,
                R.string.peq_discard_changes
            ) {
                if (it) {
                    editorDiscard()
                }
            }
            return
        }

        Timber.d("editorSave: confirming changes to band $editorBandUuid")
        editorBandBackup = null
        editorBandUuid = null
        editorActive = false

        adapter.notifyDataSetChanged()
        updateViewState()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        if (newConfig.orientation == ORIENTATION_LANDSCAPE) {
            collapsePreview(false)
        }
        super.onConfigurationChanged(newConfig)
    }

    private fun collapsePreview(collapsed: Boolean) {
        binding.equalizerSurface.isVisible = collapsed
        binding.previewTitle.text =
            getString(if (collapsed) R.string.peq_preview else R.string.peq_preview_collapsed)
    }

    @SuppressLint("ApplySharedPref")
    private fun save() {
        requireContext().getSharedPreferences(Constants.PREF_PEQ, Context.MODE_PRIVATE)
            .edit()
            .putString(getString(R.string.key_peq_bands), adapter.bands.serialize())
            .putFloat(getString(R.string.key_peq_preamp), binding.preampInput.value)
            .commit()
        requireContext().sendLocalBroadcast(Intent(Constants.ACTION_PARAMETRIC_EQ_CHANGED))
    }

    @SuppressLint("ApplySharedPref")
    private fun savePreamp() {
        requireContext().getSharedPreferences(Constants.PREF_PEQ, Context.MODE_PRIVATE)
            .edit()
            .putFloat(getString(R.string.key_peq_preamp), binding.preampInput.value)
            .commit()
        requireContext().sendLocalBroadcast(Intent(Constants.ACTION_PARAMETRIC_EQ_CHANGED))
    }

    override fun onSaveInstanceState(outState: Bundle) {
        if (editorActive)
            editorDiscard()

        super.onSaveInstanceState(outState)
    }

    companion object {
        const val STATE_BANDS = "bands"

        /** Marks the gain dial as currently showing a cutoff slope. */
        private const val SLOPE_UNIT = "dB/oct"

        fun newInstance(): ParametricEqualizerFragment {
            return ParametricEqualizerFragment()
        }
    }
}
