package me.timschneeberger.rootlessjamesdsp.fragment

import android.animation.LayoutTransition
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle
import android.os.Looper
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.isVisible
import androidx.core.widget.addTextChangedListener
import androidx.fragment.app.Fragment
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import me.timschneeberger.rootlessjamesdsp.R
import me.timschneeberger.rootlessjamesdsp.databinding.FragmentDspBinding
import me.timschneeberger.rootlessjamesdsp.utils.Constants
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.registerLocalReceiver
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.unregisterLocalReceiver
import me.timschneeberger.rootlessjamesdsp.utils.LiveprogSlots
import me.timschneeberger.rootlessjamesdsp.utils.V4aMode
import me.timschneeberger.rootlessjamesdsp.utils.EffectLayoutManager
import com.google.android.material.snackbar.Snackbar
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.showYesNoAlert
import me.timschneeberger.rootlessjamesdsp.utils.preferences.Preferences
import org.koin.android.ext.android.inject
import timber.log.Timber
import java.util.Locale

class DspFragment : Fragment(), SharedPreferences.OnSharedPreferenceChangeListener {
    private val prefsApp: Preferences.App by inject()
    private val prefsVar: Preferences.Var by inject()

    private lateinit var binding: FragmentDspBinding
    private var layoutManager: EffectLayoutManager? = null
    private var updateNoticeOnClick: (() -> Unit)? = null
    private var updateNoticeOnCloseClick: (() -> Unit)? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        prefsApp.registerOnSharedPreferenceChangeListener(this)
        super.onCreate(savedInstanceState)
    }

    override fun onResume() {
        super.onResume()
        applyV4aVisibility()
        requireContext().registerLocalReceiver(
            slotsChangedReceiver,
            android.content.IntentFilter(Constants.ACTION_LIVEPROG_SLOTS_CHANGED)
        )
        requireContext().registerLocalReceiver(
            presetLoadedReceiver,
            android.content.IntentFilter(Constants.ACTION_PRESET_LOADED)
        )
        applyLiveprogSlotVisibility()
    }

    override fun onPause() {
        super.onPause()
        requireContext().unregisterLocalReceiver(slotsChangedReceiver)
        requireContext().unregisterLocalReceiver(presetLoadedReceiver)
    }

    override fun onDestroy() {
        prefsApp.unregisterOnSharedPreferenceChangeListener(this)
        super.onDestroy()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        binding = FragmentDspBinding.inflate(layoutInflater, container, false)

        binding.translationNotice.setOnCloseClickListener(::hideTranslationNotice)
        binding.translationNotice.setOnRootClickListener {
            startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("https://crowdin.com/project/rootlessjamesdsp")))
            hideTranslationNotice()
        }

        binding.updateNotice.setOnCloseClickListener {
            updateNoticeOnCloseClick?.invoke()
        }
        binding.updateNotice.setOnRootClickListener {
            updateNoticeOnClick?.invoke()
        }

        // Should show notice?
        Timber.e(Locale.getDefault().language.toString())
        binding.translationNotice.isVisible =
           prefsVar.get<Long>(R.string.key_snooze_translation_notice) < (System.currentTimeMillis() / 1000L) &&
                    !Locale.getDefault().language.equals("en")
        binding.updateNotice.isVisible = false

        val transition = LayoutTransition()
        transition.enableTransitionType(LayoutTransition.CHANGING)
        // Not DISAPPEARING. Setting a card GONE - which V4A mode, the empty
        // liveprog slots, the effect search and the layout customiser all do -
        // parks it in the container's transitioning views for 300ms, and while
        // it sits there removeView deliberately leaves its parent set. The next
        // addView then throws "The specified child already has a parent". That
        // was a rare race while scrolling; loading a preset now exits edit mode
        // and re-applies the layout in the same looper message, which lands
        // inside that window every time.
        // Cards still slide to their new positions - only the fade-out of a
        // card being hidden is given up.
        transition.disableTransitionType(LayoutTransition.DISAPPEARING)
        binding.cardContainer.layoutTransition = transition
        // Inflating every effect card at once blocks the first frame for
        // seconds. Commit the first few immediately, then let the rest fill
        // in on the next frame so the app opens instantly.
        childFragmentManager.beginTransaction()
            .setReorderingAllowed(true)
            .replace(R.id.card_device_profiles, DeviceProfilesCardFragment.newInstance())
            .replace(
                R.id.card_output_control, PreferenceGroupFragment.newInstance(Constants.PREF_OUTPUT,
                    R.xml.dsp_output_control_preferences
                ))
            .replace(
                R.id.card_compressor, PreferenceGroupFragment.newInstance(Constants.PREF_COMPANDER,
                    R.xml.dsp_compander_preferences
                ))
            .replace(
                R.id.card_bass, PreferenceGroupFragment.newInstance(Constants.PREF_BASS,
                    R.xml.dsp_bass_preferences
                ))
            .replace(
                R.id.card_bassex, PreferenceGroupFragment.newInstance(Constants.PREF_BASSEX,
                    R.xml.dsp_bassex_preferences
                ))
            .replace(
                R.id.card_vdynbass, PreferenceGroupFragment.newInstance(Constants.PREF_VDYNBASS,
                    R.xml.dsp_vdynbass_preferences
                ))
            .commitAllowingStateLoss()

        // The remaining cards are installed only once they're about to scroll
        // into view. Committing them all up front cost ~3.4s of main-thread
        // time (measured), which is what froze the UI on startup.
        deferredCards.clear()
        deferredCards.addAll(deferredCardSpecs)
        // Fresh queue, so the completion step is owed again
        cardsFinalised = false
        cardViews.clear()
        binding.dspScrollview.viewTreeObserver.addOnScrollChangedListener {
            installVisibleCards()
        }
        binding.root.post {
            installVisibleCards()
            // Build the rest during idle time rather than waiting for the user
            // to scroll into them.
            scheduleIdlePrefetch()
        }

        // Load initial preferences
        arrayOf(R.string.key_device_profiles_enable).forEach {
            onSharedPreferenceChanged(null, getString(it))
        }
        setupEffectSearch()
        setupLayoutCustomizer()


        return binding.root
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        when(key) {
            getString(R.string.key_device_profiles_enable) -> {
                (binding.cardDeviceProfiles.parent as ViewGroup).isVisible =
                    prefsApp.get<Boolean>(R.string.key_device_profiles_enable)
            }
        }
    }

    private fun hideTranslationNotice() {
        binding.translationNotice.isVisible = false
        // Set timer +1y
        prefsVar.set<Long>(R.string.key_snooze_translation_notice, (System.currentTimeMillis() / 1000L) + 31536000L)
    }

    fun setUpdateCardVisible(visible: Boolean) {
        binding.updateNotice.isVisible = visible
    }

    fun setUpdateCardTitle(title: String) {
        binding.updateNotice.titleText = title
    }

    fun setUpdateCardOnClick(onClick: () -> Unit) {
        updateNoticeOnClick = onClick
    }

    fun setUpdateCardOnCloseClick(onClick: () -> Unit) {
        updateNoticeOnCloseClick = onClick
    }

    fun restartFragment(id: Int, newFragment: Fragment) {
        CoroutineScope(Dispatchers.Main).launch {
            try {
                childFragmentManager.beginTransaction()
                    .replace(id, newFragment)
                    .commitAllowingStateLoss()
            }
            catch(ex: IllegalStateException) {
                Timber.e("Failed to restart fragment")
                Timber.i(ex)
            }
        }
    }

    companion object {
        fun newInstance(): DspFragment {
            return DspFragment()
        }
    }

    private data class CardEntry(val cardId: Int, val titleRes: Int)

    private val searchableCards = listOf(
        CardEntry(R.id.card_output_control, R.string.output_control_header),
        CardEntry(R.id.card_compressor, R.string.compander_enable_v2),
        CardEntry(R.id.card_bass, R.string.bass_enable),
        CardEntry(R.id.card_bassex, R.string.bassex_enable),
        CardEntry(R.id.card_vdynbass, R.string.v4a_vdynbass_title),
        CardEntry(R.id.card_diffsurround, R.string.diffsurround_enable),
        CardEntry(R.id.card_clarity, R.string.clarity_enable),
        CardEntry(R.id.card_fieldsurround, R.string.fieldsurround_enable),
        CardEntry(R.id.card_hpsurround, R.string.v4a_hpsurround_title),
        CardEntry(R.id.card_fetcomp, R.string.fetcomp_enable),
        CardEntry(R.id.card_cure, R.string.cure_enable),
        CardEntry(R.id.card_viperbass, R.string.viperbass_enable),
        CardEntry(R.id.card_vreverb, R.string.vreverb_enable),
        CardEntry(R.id.card_speakeropt, R.string.speakeropt_enable),
        CardEntry(R.id.card_pitchshift, R.string.pitchshift_enable),
        CardEntry(R.id.card_echo, R.string.echo_enable),
        CardEntry(R.id.card_mbd, R.string.mbd_enable),
        CardEntry(R.id.card_maxr, R.string.maxr_enable),
        CardEntry(R.id.card_dyneq, R.string.dyneq_enable),
        CardEntry(R.id.card_agc, R.string.v4a_agc_title),
        CardEntry(R.id.card_eq, R.string.v4a_eq_title),
        CardEntry(R.id.card_geq, R.string.geq_enable),
        CardEntry(R.id.card_peq, R.string.peq_enable),
        CardEntry(R.id.card_ddc, R.string.v4a_ddc_title),
        CardEntry(R.id.card_convolver, R.string.convolver_enable),
        CardEntry(R.id.card_liveprog, R.string.liveprog_enable),
        CardEntry(R.id.card_liveprog2, R.string.liveprog2_enable),
        CardEntry(R.id.card_liveprog3, R.string.liveprog3_enable),
        CardEntry(R.id.card_liveprog4, R.string.liveprog4_enable),
        CardEntry(R.id.card_tube, R.string.v4a_tube_title),
        CardEntry(R.id.card_spectrumext, R.string.spectrumext_enable),
        CardEntry(R.id.card_stereowide, R.string.stereowide_enable),
        CardEntry(R.id.card_crossfeed, R.string.crossfeed_enable),
        CardEntry(R.id.card_reverb, R.string.reverb_enable),
    )

    /** Hides cards whose title doesn't match the query. Empty query restores everything. */
    private fun applyEffectSearch(query: String) {
        val q = query.trim().lowercase(Locale.getDefault())
        val searching = q.isNotEmpty()
        var matches = 0

        searchableCards.forEach { entry ->
            val card = binding.root.findViewById<View>(entry.cardId)?.parent as? View
            if (card != null) {
                val title = getString(entry.titleRes).lowercase(Locale.getDefault())
                val userHidden = layoutManager?.isHidden(
                    resources.getResourceEntryName(entry.cardId)
                ) == true
                val v4aHidden = V4aMode.isOn(requireContext()) &&
                        entry.cardId in V4aMode.hiddenCardIds
                val visible = !userHidden && !v4aHidden && (!searching || title.contains(q))
                card.isVisible = visible
                if (visible && searching) matches++
            }
        }

        // Group headers and non-effect cards only make sense outside of search
        binding.root.findViewById<View>(R.id.v4a_section_header)?.isVisible = !searching
        binding.root.findViewById<View>(R.id.card_device_profiles)?.let {
            (it.parent as? View)?.isVisible = !searching
        }
        binding.searchEmpty.isVisible = searching && matches == 0
    }

    private data class CardSpec(val viewId: Int, val prefName: String, val xmlRes: Int)

    /** Cards not shown on first paint; installed lazily as the user scrolls. */
    private val deferredCardSpecs = listOf(
        CardSpec(R.id.card_diffsurround, Constants.PREF_DIFFSURROUND, R.xml.dsp_diffsurround_preferences),
        CardSpec(R.id.card_clarity, Constants.PREF_CLARITY, R.xml.dsp_clarity_preferences),
        CardSpec(R.id.card_fieldsurround, Constants.PREF_FIELDSURROUND, R.xml.dsp_fieldsurround_preferences),
        CardSpec(R.id.card_hpsurround, Constants.PREF_HPSURROUND, R.xml.dsp_hpsurround_preferences),
        CardSpec(R.id.card_fetcomp, Constants.PREF_FETCOMP, R.xml.dsp_fetcomp_preferences),
        CardSpec(R.id.card_cure, Constants.PREF_CURE, R.xml.dsp_cure_preferences),
        CardSpec(R.id.card_viperbass, Constants.PREF_VIPERBASS, R.xml.dsp_viperbass_preferences),
        CardSpec(R.id.card_vreverb, Constants.PREF_VREVERB, R.xml.dsp_vreverb_preferences),
        CardSpec(R.id.card_speakeropt, Constants.PREF_SPEAKEROPT, R.xml.dsp_speakeropt_preferences),
        CardSpec(R.id.card_pitchshift, Constants.PREF_PITCHSHIFT, R.xml.dsp_pitchshift_preferences),
        CardSpec(R.id.card_echo, Constants.PREF_ECHODELAY, R.xml.dsp_echo_preferences),
        CardSpec(R.id.card_mbd, Constants.PREF_MULTIBANDDIST, R.xml.dsp_multibanddist_preferences),
        CardSpec(R.id.card_maxr, Constants.PREF_MAXIMIZER, R.xml.dsp_maximizer_preferences),
        CardSpec(R.id.card_dyneq, Constants.PREF_DYNAMICEQ, R.xml.dsp_dynamiceq_preferences),
        CardSpec(R.id.card_liveprog2, Constants.PREF_LIVEPROG2, R.xml.dsp_liveprog2_preferences),
        CardSpec(R.id.card_liveprog3, Constants.PREF_LIVEPROG3, R.xml.dsp_liveprog3_preferences),
        CardSpec(R.id.card_liveprog4, Constants.PREF_LIVEPROG4, R.xml.dsp_liveprog4_preferences),
        CardSpec(R.id.card_agc, Constants.PREF_AGC, R.xml.dsp_agc_preferences),
        CardSpec(R.id.card_eq, Constants.PREF_EQ, R.xml.dsp_equalizer_preferences),
        CardSpec(R.id.card_geq, Constants.PREF_GEQ, R.xml.dsp_graphiceq_preferences),
        CardSpec(R.id.card_peq, Constants.PREF_PEQ, R.xml.dsp_parametriceq_preferences),
        CardSpec(R.id.card_ddc, Constants.PREF_DDC, R.xml.dsp_ddc_preferences),
        CardSpec(R.id.card_convolver, Constants.PREF_CONVOLVER, R.xml.dsp_convolver_preferences),
        CardSpec(R.id.card_liveprog, Constants.PREF_LIVEPROG, R.xml.dsp_liveprog_preferences),
        CardSpec(R.id.card_tube, Constants.PREF_TUBE, R.xml.dsp_tube_preferences),
        CardSpec(R.id.card_spectrumext, Constants.PREF_SPECTRUMEXT, R.xml.dsp_spectrumext_preferences),
        CardSpec(R.id.card_stereowide, Constants.PREF_STEREOWIDE, R.xml.dsp_stereowide_preferences),
        CardSpec(R.id.card_crossfeed, Constants.PREF_CROSSFEED, R.xml.dsp_crossfeed_preferences),
        CardSpec(R.id.card_reverb, Constants.PREF_REVERB, R.xml.dsp_reverb_preferences),
    )

    private val deferredCards = ArrayList<CardSpec>()
    private var installingCards = false

    /**
     * Installs the preference fragment for any pending card that is within one
     * screen height of the viewport. Keeps startup cheap without the user ever
     * seeing an empty card.
     */
    private val cardViews = HashMap<Int, View?>()
    private var idlePrefetchQueued = false

    /**
     * Pause between background card installs. Long enough that frames and
     * touches get serviced in between, so the work is invisible rather than a
     * freeze.
     */
    private val PREFETCH_GAP_MS = 140L

    /**
     * Installs the remaining cards while the UI thread has nothing else to do.
     *
     * Scrolling into a card that hasn't been built yet means inflating it right
     * when frames matter most, which is what the stutter is. An idle handler
     * only runs when no work is pending - including no pending frame - so the
     * cards are quietly built during the pauses instead, and by the time the
     * user scrolls down they already exist. One per pass keeps any single stall
     * to a single card if the user starts scrolling mid-inflation.
     */
    private var cardsFinalised = false

    /**
     * Runs once, after the last card is installed. Both the scroll-driven and
     * idle-driven installers can drain the queue, so without this the layout
     * was applied twice - which is what crashed, since re-parenting views that
     * had already been re-parented is not a no-op.
     */
    private fun onAllCardsInstalled() {
        if (cardsFinalised || !isAdded) return
        cardsFinalised = true
        layoutManager?.applyLayout()
        applyLiveprogSlotVisibility()
    }

    private fun scheduleIdlePrefetch() {
        if (idlePrefetchQueued || deferredCards.isEmpty()) return
        idlePrefetchQueued = true
        Looper.myQueue().addIdleHandler {
            idlePrefetchQueued = false
            if (!isAdded || deferredCards.isEmpty()) return@addIdleHandler false

            val spec = deferredCards.first()
            deferredCards.remove(spec)
            val t0 = android.os.SystemClock.uptimeMillis()
            childFragmentManager.beginTransaction()
                .setReorderingAllowed(true)
                .replace(spec.viewId, PreferenceGroupFragment.newInstance(spec.prefName, spec.xmlRes))
                .commitNowAllowingStateLoss()
            Timber.d("PERF prefetch ${spec.prefName} commit=${android.os.SystemClock.uptimeMillis() - t0}ms remaining=${deferredCards.size}")

            if (deferredCards.isEmpty()) onAllCardsInstalled()
            // Breathe: without this pause the next idle pass fires immediately
            // (nothing else is pending), so every card inflates in one
            // unbroken run and the app is frozen for seconds.
            else binding.root.postDelayed({ scheduleIdlePrefetch() }, PREFETCH_GAP_MS)
            false   // one card per idle pass
        }
    }

    private fun installVisibleCards() {
        if (installingCards || deferredCards.isEmpty() || !isAdded) return
        installingCards = true
        try {
            val scroll = binding.dspScrollview
            val top = scroll.scrollY
            val bottom = top + scroll.height + scroll.height / 2 // half a screen of lookahead

            val ready = deferredCards.filter { spec ->
                // Cached: this runs on every scroll event, and searching the
                // view tree for each of ~26 cards each time is real work during
                // exactly the frames that must stay smooth.
                val card = cardViews.getOrPut(spec.viewId) {
                    binding.root.findViewById<View>(spec.viewId)?.parent as? View
                } ?: return@filter false
                card.top < bottom && card.bottom > top - scroll.height
            }
            if (ready.isEmpty()) return

            // One per pass, not two: a preference screen costs around 145ms to
            // inflate, so a pair of them lands as a ~290ms stall right in the
            // middle of a scroll. Idle prefetch below usually gets there first.
            val batch = ready.take(1)
            val tx = childFragmentManager.beginTransaction().setReorderingAllowed(true)
            batch.forEach { spec ->
                tx.replace(
                    spec.viewId,
                    PreferenceGroupFragment.newInstance(spec.prefName, spec.xmlRes)
                )
            }
            // Commit now rather than scheduling it. A plain commit only queues
            // the transaction, so several can pile up and inflate together in
            // one frame - which is the long frame seen while scrolling. Doing
            // it here bounds the cost to a single card, measured in single-
            // digit milliseconds.
            val t0 = android.os.SystemClock.uptimeMillis()
            tx.commitNowAllowingStateLoss()
            val took = android.os.SystemClock.uptimeMillis() - t0
            if (took > 8) Timber.d("PERF scroll-install ${batch.firstOrNull()?.prefName} ${took}ms")
            deferredCards.removeAll(batch.toSet())

            if (deferredCards.isNotEmpty()) {
                binding.root.postDelayed({ installVisibleCards() }, 48)
                scheduleIdlePrefetch()
            } else {
                onAllCardsInstalled()
            }
        } finally {
            installingCards = false
        }
    }

    /**
     * Extra Liveprog cards exist only while their slot holds a script. Slots keep
     * their identity, so removing the middle script leaves that card hidden and
     * the ones after it where they were.
     */
    /**
     * A preset carries the card layout too, and this fragment owns the manager
     * holding it. Nothing else re-applies it, so without this the restored
     * order and hidden cards stay invisible until the app is killed - and are
     * then overwritten by the first drag.
     */
    private val presetLoadedReceiver = object : android.content.BroadcastReceiver() {
        override fun onReceive(context: android.content.Context?, intent: android.content.Intent?) {
            if (!isAdded) return
            // Re-applying mid-drag would fight the user's finger.
            layoutManager?.let {
                if (it.editMode) it.exitEditMode()
                it.reload()
            }
        }
    }

    private val slotsChangedReceiver = object : android.content.BroadcastReceiver() {
        override fun onReceive(context: android.content.Context?, intent: android.content.Intent?) {
            applyLiveprogSlotVisibility(rebuild = true)
        }
    }

    /** Hides every card the original ViPER4Android didn't have. */
    private fun applyV4aVisibility() {
        if (!isAdded) return
        val on = V4aMode.isOn(requireContext())
        // Classic layout drops the "ViPER4Android effects" divider heading and
        // the gap it occupied - the original had one uninterrupted list.
        if (me.timschneeberger.rootlessjamesdsp.utils.V4aIconColors.isClassicLayout(requireContext())) {
            binding.v4aSectionHeader.isVisible = false
            hideDeviceProfileCard()
            padForFooter()
        }
        // The original V4A had one fixed list: no search, no reordering, no
        // groups. Hide that whole toolbar while the mode is on.
        binding.searchCard.isVisible = !on
        // V4A had a single fixed list, so user-made group headings go too
        layoutManager?.setHeadersVisible(!on)
        V4aMode.hiddenCardIds.forEach { id ->
            val container = binding.root.findViewById<View>(id)?.parent as? View
            if (on) container?.isVisible = false
            else if (container?.isVisible == false && !deferredCards.any { it.viewId == id })
                container.isVisible = true
        }
        if (on) deferredCards.removeAll { it.viewId in V4aMode.hiddenCardIds }
    }

    /**
     * Classic layout shows the device profile in the activity's footer, so the
     * copy in the scrolling list is removed to avoid two live instances.
     */
    /**
     * Keeps the end of the list clear of the persistent footer. Applied to the
     * scrolling parent with clipToPadding off, so newly added groups and cards
     * dragged to the bottom stay reachable instead of hiding underneath it.
     */
    private fun padForFooter() {
        val footer = activity?.findViewById<View>(R.id.classic_footer) ?: return
        val scroll = binding.root.parent as? ViewGroup ?: return
        footer.post {
            val h = footer.height.takeIf { it > 0 } ?: return@post
            scroll.clipToPadding = false
            scroll.setPadding(scroll.paddingLeft, scroll.paddingTop,
                scroll.paddingRight, h)
        }
    }

    private fun hideDeviceProfileCard() {
        if (!isAdded) return
        val container = binding.root.findViewById<View>(R.id.card_device_profiles) ?: return
        (container.parent as? View)?.isVisible = false
        childFragmentManager.findFragmentById(R.id.card_device_profiles)?.let {
            childFragmentManager.beginTransaction().remove(it).commitAllowingStateLoss()
        }
    }

    fun applyLiveprogSlotVisibility(rebuild: Boolean = false) {
        // In V4A-only mode all Liveprog cards stay hidden regardless of the
        // configured slots; the slot writes themselves are preserved.
        if (isAdded && V4aMode.isOn(requireContext())) {
            applyV4aVisibility()
            return
        }
        if (!isAdded) return
        val ids = intArrayOf(R.id.card_liveprog2, R.id.card_liveprog3, R.id.card_liveprog4)
        val prefs = arrayOf(Constants.PREF_LIVEPROG2, Constants.PREF_LIVEPROG3, Constants.PREF_LIVEPROG4)
        val xml = intArrayOf(
            R.xml.dsp_liveprog2_preferences,
            R.xml.dsp_liveprog3_preferences,
            R.xml.dsp_liveprog4_preferences
        )
        val occupied = LiveprogSlots.read(requireContext())
        ids.forEachIndexed { index, id ->
            val container = binding.root.findViewById<View>(id)?.parent as? View
            container?.isVisible = occupied[index + 1].isNotBlank()
        }
        if (!rebuild) return
        // The picker wrote these namespaces directly, so the existing preference
        // screens still hold the old values - rebuild them to re-read.
        val tx = childFragmentManager.beginTransaction().setReorderingAllowed(true)
        ids.forEachIndexed { index, id ->
            if (occupied[index + 1].isNotBlank() && !deferredCards.any { it.viewId == id })
                tx.replace(id, PreferenceGroupFragment.newInstance(prefs[index], xml[index]))
        }
        tx.commitAllowingStateLoss()
    }

    private fun setupEffectSearch() {
        binding.searchInput.addTextChangedListener(
            afterTextChanged = { applyEffectSearch(it?.toString() ?: "") }
        )
        // iOS-style: search sits just above the content, revealed by pulling down
        binding.dspScrollview.post {
            val h = binding.searchCard.height
            if (h > 0 && binding.searchInput.text.isNullOrEmpty()) {
                binding.dspScrollview.scrollTo(0, h)
            }
        }
    }


    private fun setupLayoutCustomizer() {
        val entries = ArrayList<EffectLayoutManager.Item>()
        entries.add(
            EffectLayoutManager.Item(
                "group_v4a", R.id.v4a_section_header, R.string.v4a_section_header, isHeader = true
            )
        )
        searchableCards.forEach { card ->
            entries.add(
                EffectLayoutManager.Item(
                    resources.getResourceEntryName(card.cardId), card.cardId, card.titleRes
                )
            )
        }

        val manager = EffectLayoutManager(requireContext(), binding.cardContainer, entries)
        layoutManager = manager
        manager.applyLayout()

        manager.onEditModeChanged = { editing ->
            binding.editLayoutButton.setImageResource(
                if (editing) R.drawable.ic_twotone_check_24dp else R.drawable.ic_twotone_edit_24dp
            )
            binding.searchInput.isEnabled = !editing
            binding.chainOrderButton.isVisible = editing
            if (editing) {
                Snackbar.make(binding.root, R.string.effect_edit_hint, Snackbar.LENGTH_LONG).show()
            }
        }

        binding.chainOrderButton.setOnClickListener {
            ProcessingOrderDialogFragment.newInstance()
                .show(childFragmentManager, "processing_order")
        }

        binding.editLayoutButton.setOnLongClickListener {
            requireContext().showYesNoAlert(
                R.string.effect_reset_layout,
                R.string.effect_reset_layout_confirm
            ) { confirmed ->
                if (confirmed) {
                    if (manager.editMode) manager.exitEditMode()
                    manager.resetLayout()
                }
            }
            true
        }

        binding.editLayoutButton.setOnClickListener {
            if (!manager.editMode) {
                binding.searchInput.setText("")
                applyEffectSearch("")
            }
            manager.toggleEditMode()
        }
    }

    /** Lets the host activity close edit mode with the back button. */
    fun exitEditModeIfActive(): Boolean {
        val manager = layoutManager ?: return false
        if (!manager.editMode) return false
        manager.exitEditMode()
        return true
    }

}