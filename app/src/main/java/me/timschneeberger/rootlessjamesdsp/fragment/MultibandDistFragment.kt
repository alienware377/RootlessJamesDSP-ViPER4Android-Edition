package me.timschneeberger.rootlessjamesdsp.fragment

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.os.Bundle
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import com.google.android.material.button.MaterialButtonToggleGroup
import me.timschneeberger.rootlessjamesdsp.R
import me.timschneeberger.rootlessjamesdsp.databinding.FragmentMbdPanelBinding
import me.timschneeberger.rootlessjamesdsp.model.ParametricEqBand
import me.timschneeberger.rootlessjamesdsp.model.ParametricEqBandList
import me.timschneeberger.rootlessjamesdsp.model.ParametricEqFilterType
import me.timschneeberger.rootlessjamesdsp.utils.Constants
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.sendLocalBroadcast
import me.timschneeberger.rootlessjamesdsp.view.KnobView
import java.util.UUID

/**
 * Editor for the multiband distortion.
 *
 * The band half is the parametric equaliser's graph, handles and knobs, driving
 * a separate band list: what the user draws here is the filter that selects
 * what gets distorted, not an equaliser curve applied to the output. The rest
 * is a knob panel in the same shape as the delay's.
 *
 * Deliberately its own fragment rather than a parameterised
 * [ParametricEqualizerFragment]. That one carries the equaliser's preamp,
 * EqualizerAPO import and export, its own activity and its own broadcast;
 * bending all of it around a second owner would put a working screen at risk
 * for no gain, since what is worth sharing here - the surface, the band model
 * and the knob view - are already separate components.
 */
class MultibandDistFragment : Fragment() {
    private lateinit var binding: FragmentMbdPanelBinding
    private val prefs: SharedPreferences by lazy {
        requireContext().getSharedPreferences(Constants.PREF_MULTIBANDDIST, Context.MODE_PRIVATE)
    }

    private val bands = ParametricEqBandList()

    private var selectedUuid: UUID? = null
    private var suppressBandWrite = false

    private val undoStack = ArrayDeque<String>()
    private val redoStack = ArrayDeque<String>()

    private val filterButtons by lazy {
        listOf(
            ParametricEqFilterType.PEAKING to binding.mbdFilterPeaking.id,
            ParametricEqFilterType.LOW_SHELF to binding.mbdFilterLowShelf.id,
            ParametricEqFilterType.HIGH_SHELF to binding.mbdFilterHighShelf.id,
            ParametricEqFilterType.LOW_PASS to binding.mbdFilterLowPass.id,
            ParametricEqFilterType.HIGH_PASS to binding.mbdFilterHighPass.id,
        )
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        binding = FragmentMbdPanelBinding.inflate(inflater, container, false)

        loadBands()
        setupSurface()
        setupChips()
        setupBandKnobs()
        setupPanelKnobs()
        setupToggles()

        refreshSurface()
        selectBand(bands.indexOfFirst { true }.takeIf { it >= 0 })
        refreshUsability()

        return binding.root
    }

    // ------------------------------------------------------------- bands

    private fun loadBands() {
        bands.deserialize(prefs.getString(getString(R.string.key_mbd_bands), Constants.DEFAULT_MBD_BANDS)!!)
    }

    private fun saveBands() {
        prefs.edit()
            .putString(getString(R.string.key_mbd_bands), bands.serialize())
            .commit()
        notifyEngine()
    }

    private fun notifyEngine() {
        requireContext().sendLocalBroadcast(Intent(Constants.ACTION_PREFERENCES_UPDATED))
    }

    private fun refreshSurface() {
        binding.mbdSurface.setBands(bands, 0.0)
    }

    private fun snapshot() = bands.serialize()

    private fun pushHistory(previous: String) {
        if (undoStack.lastOrNull() == previous) return
        undoStack.addLast(previous)
        while (undoStack.size > 50) undoStack.removeFirst()
        redoStack.clear()
        refreshUsability()
    }

    private fun applySerialized(value: String) {
        bands.deserialize(value)
        refreshSurface()
        saveBands()
        selectBand(if (bands.isEmpty()) null else 0)
        refreshUsability()
    }

    // ----------------------------------------------------------- surface

    private fun setupSurface() {
        binding.mbdSurface.interactive = true

        // The graph lives inside a scrolling page, so a vertical drag on a
        // handle would otherwise be taken by the scroll view and the handle
        // would not move. Claiming the gesture on touch-down keeps the drag
        // where it was aimed, and releasing it again lets the page scroll
        // normally everywhere else.
        binding.mbdSurface.setOnTouchListener { v, ev ->
            when (ev.actionMasked) {
                MotionEvent.ACTION_DOWN ->
                    v.parent?.requestDisallowInterceptTouchEvent(true)
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL ->
                    v.parent?.requestDisallowInterceptTouchEvent(false)
            }
            false
        }

        binding.mbdSurface.onBandsChanged = {
            // Fired continuously through a drag; the knobs follow so they do
            // not sit on stale numbers the moment the drag ends.
            syncKnobsFromSelection()
            saveBands()
        }
        binding.mbdSurface.onDragFinished = {
            sortBandsByFrequency()
            refreshSurface()
            syncKnobsFromSelection()
            saveBands()
        }
        binding.mbdSurface.onBandSelected = { index ->
            pushHistory(snapshot())
            selectBand(index)
        }
        binding.mbdSurface.onSelectionCleared = {
            selectBand(null)
        }
        binding.mbdSurface.onBandAddRequested = { freq, gain ->
            pushHistory(snapshot())
            val band = ParametricEqBand(freq, gain, 0.71, currentFilterType())
            bands.add(band)
            sortBandsByFrequency()
            refreshSurface()
            saveBands()
            selectBand(bands.indexOfFirst { it.uuid == band.uuid })
        }
        binding.mbdSurface.onBandRemoveRequested = { index ->
            pushHistory(snapshot())
            if (index in bands.indices) bands.removeAt(index)
            refreshSurface()
            saveBands()
            selectBand(null)
        }
    }

    @SuppressLint("NotifyDataSetChanged")
    private fun sortBandsByFrequency() {
        if (bands.size < 2) return
        val sorted = bands.sortedBy { it.frequency }
        if (sorted == bands.toList()) return
        bands.clear()
        bands.addAll(sorted)
    }

    private fun selectBand(index: Int?) {
        selectedUuid = index?.let { bands.getOrNull(it)?.uuid }
        if (index != null) binding.mbdSurface.selectBand(index)
        syncKnobsFromSelection()
        refreshUsability()
    }

    private fun selectedBand(): ParametricEqBand? =
        selectedUuid?.let { uuid -> bands.firstOrNull { it.uuid == uuid } }

    private fun syncKnobsFromSelection() {
        val band = selectedBand() ?: return
        suppressBandWrite = true
        binding.mbdBandFreq.value = band.frequency.toFloat()
        binding.mbdBandQ.value = band.q.toFloat()
        binding.mbdBandGain.value = band.gain.toFloat()
        filterButtons.firstOrNull { it.first == band.filterType }
            ?.let { binding.mbdFilterGroup.check(it.second) }
        suppressBandWrite = false
    }

    private fun currentFilterType(): ParametricEqFilterType =
        filterButtons.firstOrNull { it.second == binding.mbdFilterGroup.checkedButtonId }?.first
            ?: ParametricEqFilterType.LOW_PASS

    private fun writeSelectedBand() {
        if (suppressBandWrite) return
        val band = selectedBand() ?: return
        band.frequency = binding.mbdBandFreq.value.toDouble()
        band.q = binding.mbdBandQ.value.toDouble()
        band.gain = binding.mbdBandGain.value.toDouble()
        band.filterType = currentFilterType()
        refreshSurface()
        saveBands()
        refreshUsability()
    }

    // -------------------------------------------------------------- chips

    private fun setupChips() {
        binding.mbdAdd.setOnClickListener {
            pushHistory(snapshot())
            // A new handle lands an octave above the highest one there, or at
            // 250 Hz on an empty graph - somewhere it can be seen and grabbed
            // rather than stacked under an existing handle.
            val freq = (bands.maxOfOrNull { it.frequency }?.times(2.0) ?: 250.0)
                .coerceIn(20.0, 20000.0)
            val band = ParametricEqBand(freq, 0.0, 0.71, currentFilterType())
            bands.add(band)
            sortBandsByFrequency()
            refreshSurface()
            saveBands()
            selectBand(bands.indexOfFirst { it.uuid == band.uuid })
        }
        binding.mbdRemove.setOnClickListener {
            val uuid = selectedUuid ?: return@setOnClickListener
            pushHistory(snapshot())
            bands.removeAll { it.uuid == uuid }
            refreshSurface()
            saveBands()
            selectBand(null)
        }
        binding.mbdUndo.setOnClickListener {
            val previous = undoStack.removeLastOrNull() ?: return@setOnClickListener
            redoStack.addLast(snapshot())
            applySerialized(previous)
        }
        binding.mbdRedo.setOnClickListener {
            val next = redoStack.removeLastOrNull() ?: return@setOnClickListener
            undoStack.addLast(snapshot())
            applySerialized(next)
        }
        binding.mbdReset.setOnClickListener {
            pushHistory(snapshot())
            applySerialized(Constants.DEFAULT_MBD_BANDS)
        }
    }

    // -------------------------------------------------------------- knobs

    private fun setupBandKnobs() {
        listOf(binding.mbdBandFreq, binding.mbdBandQ, binding.mbdBandGain).forEach { knob ->
            knob.setOnValueChangedListener { writeSelectedBand() }
        }
    }

    private fun bindKnob(knob: KnobView, keyRes: Int, default: Float) {
        val key = getString(keyRes)
        knob.value = prefs.getFloat(key, default)
        knob.setOnValueChangedListener {
            prefs.edit().putFloat(key, knob.value).apply()
            notifyEngine()
            refreshUsability()
        }
    }

    private fun setupPanelKnobs() {
        bindKnob(binding.knobDrive, R.string.key_mbd_drive, 35f)
        bindKnob(binding.knobBias, R.string.key_mbd_bias, 0f)
        bindKnob(binding.knobShape, R.string.key_mbd_shape, 50f)
        bindKnob(binding.knobBits, R.string.key_mbd_bits, 16f)
        bindKnob(binding.knobDownsample, R.string.key_mbd_downsample, 0f)
        bindKnob(binding.knobTone, R.string.key_mbd_tone, 50f)
        bindKnob(binding.knobBandGain, R.string.key_mbd_band_gain, 100f)
        bindKnob(binding.knobChorusMix, R.string.key_mbd_chorus_mix, 0f)
        bindKnob(binding.knobRate, R.string.key_mbd_chorus_rate, 0.6f)
        bindKnob(binding.knobDepth, R.string.key_mbd_chorus_depth, 6f)
        bindKnob(binding.knobFeedback, R.string.key_mbd_chorus_feedback, 0f)
        bindKnob(binding.knobSpread, R.string.key_mbd_chorus_spread, 50f)
        bindKnob(binding.knobMix, R.string.key_mbd_mix, 100f)

        // Voices is stored as a list index like the segmented controls, so it
        // reads back the same way whether it is set here or from a preference
        // screen; the dial shows a count, which is one higher.
        val voicesKey = getString(R.string.key_mbd_chorus_voices)
        binding.knobVoices.value =
            ((prefs.getString(voicesKey, "1") ?: "1").toIntOrNull() ?: 1) + 1f
        binding.knobVoices.setOnValueChangedListener {
            prefs.edit()
                .putString(voicesKey, (binding.knobVoices.value.toInt() - 1).coerceAtLeast(0).toString())
                .apply()
            notifyEngine()
        }
    }

    private fun bindToggle(
        group: MaterialButtonToggleGroup,
        keyRes: Int,
        default: String,
        ids: IntArray
    ) {
        val key = getString(keyRes)
        val current = (prefs.getString(key, default) ?: default).toIntOrNull() ?: 0
        ids.getOrNull(current)?.let { group.check(it) }
        group.addOnButtonCheckedListener { _, checkedId, isChecked ->
            if (!isChecked) return@addOnButtonCheckedListener
            val index = ids.indexOf(checkedId)
            if (index >= 0) {
                prefs.edit().putString(key, index.toString()).apply()
                notifyEngine()
                refreshUsability()
            }
        }
    }

    private fun setupToggles() {
        bindToggle(
            binding.mbdRoutingGroup, R.string.key_mbd_routing, "0",
            intArrayOf(binding.routingSplit.id, binding.routingParallel.id)
        )
        bindToggle(
            binding.mbdModelGroup, R.string.key_mbd_model, "0",
            intArrayOf(
                binding.modelSoft.id, binding.modelHard.id, binding.modelTube.id,
                binding.modelOverdrive.id, binding.modelFold.id, binding.modelFuzz.id,
                binding.modelRectify.id, binding.modelCrush.id
            )
        )
        binding.mbdFilterGroup.addOnButtonCheckedListener { _, _, isChecked ->
            if (isChecked) writeSelectedBand()
        }
    }

    // --------------------------------------------------------- usability

    /**
     * Dims what cannot currently do anything, rather than hiding it: hiding
     * reflows the panel under the finger that just moved a dial.
     */
    private fun refreshUsability() {
        fun dim(view: View, usable: Boolean) {
            view.isEnabled = usable
            view.alpha = if (usable) 1f else 0.35f
        }

        val hasSelection = selectedBand() != null
        binding.mbdRemove.isEnabled = hasSelection
        dim(binding.mbdBandFreq, hasSelection)
        dim(binding.mbdBandQ, hasSelection)
        // A cutoff has a corner, not a gain, and the coefficients ignore the
        // gain term outright.
        val cutoff = currentFilterType().let {
            it == ParametricEqFilterType.LOW_PASS || it == ParametricEqFilterType.HIGH_PASS
        }
        dim(binding.mbdBandGain, hasSelection && !cutoff)

        binding.mbdUndo.isEnabled = undoStack.isNotEmpty()
        binding.mbdRedo.isEnabled = redoStack.isNotEmpty()

        val model = (prefs.getString(getString(R.string.key_mbd_model), "0") ?: "0").toIntOrNull() ?: 0
        val crush = model == MODEL_CRUSH
        dim(binding.knobBits, crush)
        dim(binding.knobDownsample, crush)

        val driven = binding.knobDrive.value > 0f
        dim(binding.knobShape, driven)
        dim(binding.knobBias, driven)
        // Character selects between shapers that are only reached once there
        // is drive to feed them. Leaving the row lit at zero drive is what
        // made this look broken: the buttons respond, and nothing happens.
        dim(binding.mbdModelGroup, driven)
        for (i in 0 until binding.mbdModelGroup.childCount)
            binding.mbdModelGroup.getChildAt(i).isEnabled = driven

        val chorusOn = binding.knobChorusMix.value > 0f
        dim(binding.knobRate, chorusOn)
        dim(binding.knobDepth, chorusOn)
        dim(binding.knobFeedback, chorusOn)
        dim(binding.knobSpread, chorusOn)
        dim(binding.knobVoices, chorusOn)
    }

    companion object {
        /** Matches MBD_MODEL_CRUSH in the engine. */
        private const val MODEL_CRUSH = 7

        fun newInstance() = MultibandDistFragment()
    }
}
