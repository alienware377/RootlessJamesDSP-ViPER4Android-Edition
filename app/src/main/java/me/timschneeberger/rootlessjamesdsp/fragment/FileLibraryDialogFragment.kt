package me.timschneeberger.rootlessjamesdsp.fragment

import android.animation.LayoutTransition
import android.annotation.SuppressLint
import android.app.Activity
import android.app.Dialog
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.media.AudioManager
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.Filter
import android.widget.Filterable
import android.widget.ListAdapter
import android.widget.TextView
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.annotation.StringRes
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.widget.PopupMenu
import androidx.core.content.FileProvider
import androidx.core.content.getSystemService
import androidx.core.view.isVisible
import androidx.preference.DialogPreference.TargetFragment
import androidx.preference.ListPreferenceDialogFragmentCompat
import androidx.preference.Preference
import com.google.android.material.chip.Chip
import kotlinx.coroutines.*
import me.timschneeberger.rootlessjamesdsp.BuildConfig
import me.timschneeberger.rootlessjamesdsp.MainApplication
import me.timschneeberger.rootlessjamesdsp.R
import me.timschneeberger.rootlessjamesdsp.activity.LiveprogEditorActivity
import me.timschneeberger.rootlessjamesdsp.databinding.DialogFilelibraryBinding
import me.timschneeberger.rootlessjamesdsp.interop.JdspImpResToolbox
import me.timschneeberger.rootlessjamesdsp.liveprog.EelParser
import me.timschneeberger.rootlessjamesdsp.utils.Constants
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.sendLocalBroadcast
import me.timschneeberger.rootlessjamesdsp.utils.FileLibraryLayout
import me.timschneeberger.rootlessjamesdsp.utils.GithubLibraryDownloader
import me.timschneeberger.rootlessjamesdsp.utils.LiveprogSlots
import me.timschneeberger.rootlessjamesdsp.model.preset.Preset
import me.timschneeberger.rootlessjamesdsp.preference.FileLibraryPreference
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.showAlert
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.showInputAlert
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.toast
import me.timschneeberger.rootlessjamesdsp.utils.storage.StorageUtils
import timber.log.Timber
import java.io.File
import java.util.Locale
import kotlin.math.roundToInt


class FileLibraryDialogFragment : ListPreferenceDialogFragmentCompat(), TargetFragment {

    private val fileLibPreference by lazy {
        preference as FileLibraryPreference
    }

    private var clickedEntryValue: CharSequence? = null
    private lateinit var dialog: AlertDialog
    private lateinit var importLauncher: ActivityResultLauncher<Intent>
    private lateinit var binding: DialogFilelibraryBinding

    private val eelParser = EelParser()
    private val scriptScannerScope = CoroutineScope(Dispatchers.IO)
    private var currentTag: String? = null
    /** Multi-select is offered only by the first Liveprog card. */
    private val isSlotHost by lazy {
        fileLibPreference.isLiveprog() &&
            fileLibPreference.key == getString(R.string.key_liveprog_file)
    }
    /** Liveprog always picks several scripts at once; other libraries don't. */
    private val multiMode get() = isSlotHost
    private var currentTagScripts: List<String>? = null

    /** Convolver and DDC libraries get search, custom sorting, hiding and groups. */
    private val libEditable by lazy {
        fileLibPreference.isIrs() || fileLibPreference.isVdc() || fileLibPreference.isPreset()
    }
    private val libLayout by lazy { FileLibraryLayout(requireContext(), fileLibPreference.key) }
    private var libEditMode = false
    private var libSearch = ""
    private var revealStartY = -1f

    /** GitHub repository search is parked until its flow is hardened. */
    private val githubSearchEnabled = false

    /**
     * Unpack a preset over the current settings and say so.
     *
     * Failure has to be visible: the archive can be a backup rather than a
     * preset, or too new for this build, and load() signals all of that by
     * throwing. Swallowing it would look identical to a preset that simply
     * changed nothing.
     */
    private fun loadPreset(entry: Entry) {
        try {
            Preset(File(entry.value.toString()).name).load()
            requireContext().toast(getString(R.string.filelibrary_preset_loaded, entry.name))
        }
        catch (ex: Exception) {
            Timber.e(ex, "Failed to load preset ${entry.name}")
            requireContext().showAlert(
                getString(R.string.filelibrary_corrupted_title),
                ex.localizedMessage ?: getString(R.string.filelibrary_preset_load_failed, entry.name))
        }
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        dialog = super.onCreateDialog(savedInstanceState) as AlertDialog
        // Workaround to prevent the button from closing the dialog
        dialog.setOnShowListener {
            if (multiMode)
                binding.multiSelectHint.isVisible = true

            if(fileLibPreference.isPreset() && dialog.listView.adapter.isEmpty) {
                requireContext().toast(getString(R.string.filelibrary_no_presets))
            }

            dialog.getButton(AlertDialog.BUTTON_NEUTRAL)?.setOnClickListener {
                if(fileLibPreference.isPreset()) {
                    val popupMenu = PopupMenu(requireContext(), it)
                    popupMenu.menuInflater.inflate(R.menu.menu_filelibrary_add_context, popupMenu.menu)

                    popupMenu.setOnMenuItemClickListener { menuItem ->
                        when (menuItem.itemId) {
                            R.id.preset_import -> { import() }
                            R.id.preset_new -> {
                                showFileNamePrompt(
                                    R.string.filelibrary_context_new_preset_long,
                                    Preset("Untitled.tar").file(),
                                    autofill = false,
                                    allowOverwrite = true
                                ) { file ->
                                    val overwritten = file.exists()
                                    val success = Preset(file.name).save()
                                    if(overwritten && success)
                                        requireContext().toast(getString(R.string.filelibrary_preset_overwritten, file.nameWithoutExtension))
                                    else if(!overwritten && success)
                                        requireContext().toast(getString(R.string.filelibrary_preset_created, file.nameWithoutExtension))
                                    else
                                        requireContext().toast(getString(R.string.filelibrary_preset_save_failed))

                                    refresh()
                                }
                            }
                        }
                        true
                    }
                    popupMenu.show()
                }
                else if (libEditable && !fileLibPreference.isPreset()) {
                    val popup = PopupMenu(requireContext(), it)
                    popup.menu.add(0, 1, 0, R.string.action_import)
                    popup.menu.add(0, 2, 1, R.string.filelib_download_more)
                    popup.setOnMenuItemClickListener { mi ->
                        when (mi.itemId) { 1 -> import(); 2 -> showDownloadSources() }
                        true
                    }
                    popup.show()
                }
                else
                    import()
            }
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).isVisible = false

            if (libEditable) setupLibraryBar()

            if (multiMode) {
                // Numbered badges replace the single-choice radio buttons
                dialog.listView.choiceMode = android.widget.ListView.CHOICE_MODE_NONE
            }

            // In multi mode a tap assigns/frees a slot and the dialog stays open
            dialog.listView.setOnItemClickListener { _, _, position, _ ->
                if (libEditMode) return@setOnItemClickListener
                val entry = dialog.listView.adapter.getItem(position) as Entry
                if (entry.isHeader) return@setOnItemClickListener
                if (multiMode) {
                    val assigned = LiveprogSlots.toggle(requireContext(), entry.value.toString())
                    if (assigned < 0 &&
                        LiveprogSlots.slotNumberOf(requireContext(), entry.value.toString()) == null &&
                        LiveprogSlots.read(requireContext()).none { it.isBlank() }) {
                        requireContext().toast(getString(R.string.liveprog_multi_full))
                    }
                    (dialog.listView.adapter as? ListItemAdapter)?.notifyDataSetChanged()
                    // Keep this preference in step with slot 1 so its summary
                    // doesn't keep showing the previously chosen script.
                    val slot1 = LiveprogSlots.read(requireContext())[0]
                    if (fileLibPreference.value != slot1) {
                        if (fileLibPreference.callChangeListener(slot1))
                            fileLibPreference.value = slot1
                    }
                    notifySlotsChanged()
                }
                else {
                    // Presets load from here. This listener is installed when
                    // the dialog shows, which replaces the one AlertController
                    // wired up from builder.setAdapter - and that lambda was
                    // the only place the load ever happened, so tapping a
                    // preset merely closed the dialog and applied nothing.
                    if (fileLibPreference.isPreset())
                        loadPreset(entry)
                    clickedEntryValue = entry.value
                    onDialogClosed(true)
                    dialog.dismiss()
                }
            }
        }

        dialog.listView.setOnItemLongClickListener {
                _, view, position, _ ->
            if (libEditMode) return@setOnItemLongClickListener true
            val item = dialog.listView.adapter.getItem(position) as Entry
            if (item.isHeader) return@setOnItemLongClickListener true
            val name = item.name
            val path = FileLibraryPreference.createFullPathCompat(requireContext(), item.value.toString())

            val popupMenu = PopupMenu(requireContext(), view)
            popupMenu.menuInflater.inflate(R.menu.menu_filelibrary_context, popupMenu.menu)
            popupMenu.menu.findItem(R.id.duplicate_selection).isVisible =
                fileLibPreference.isLiveprog() || fileLibPreference.isPreset()
            popupMenu.menu.findItem(R.id.edit_selection).isVisible = fileLibPreference.isLiveprog()
            popupMenu.menu.findItem(R.id.overwrite_selection).isVisible = fileLibPreference.isPreset()
            popupMenu.menu.findItem(R.id.resample_selection).isVisible = fileLibPreference.isIrs()

            popupMenu.setOnMenuItemClickListener { menuItem ->
                val selectedFile = File(path.toString())
                when (menuItem.itemId) {
                    R.id.resample_selection -> {
                        if(fileLibPreference.isIrs()) {
                            var targetRate = (requireActivity().application as MainApplication).engineSampleRate.roundToInt()
                            if (targetRate <= 0) {
                                targetRate = requireContext().getSystemService<AudioManager>()
                                    ?.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)
                                    ?.let { str -> Integer.parseInt(str).takeUnless { it == 0 } } ?: 48000
                                Timber.w("resample: engine sample rate is zero, using HAL rate instead")
                            }

                            Timber.d("resample: Resampling ${selectedFile.name} to ${targetRate}Hz")

                            CoroutineScope(Dispatchers.IO).launch {
                                val newName = JdspImpResToolbox.OfflineAudioResample(
                                    (selectedFile.absoluteFile.parentFile?.absolutePath + "/"),
                                    selectedFile.name,
                                    targetRate
                                )

                                withContext(Dispatchers.Main) {
                                    try {
                                        if (newName == "Invalid")
                                            requireContext().toast(getString(R.string.filelibrary_resample_failed))
                                        else
                                            requireContext().toast(getString(R.string.filelibrary_resample_complete,
                                                targetRate))
                                        refresh()
                                    }
                                    catch (_: IllegalStateException) {
                                        // Context may not be attached to fragment at this point
                                    }
                                }
                            }
                        }
                        refresh()
                    }
                    R.id.overwrite_selection -> {
                        if(fileLibPreference.isPreset()) {
                            if(Preset(selectedFile.name).save())
                                requireContext().toast(getString(R.string.filelibrary_preset_overwritten, name))
                            else
                                requireContext().toast(getString(R.string.filelibrary_preset_save_failed))
                        }
                        refresh()
                    }
                    R.id.edit_selection -> {
                        if(fileLibPreference.isLiveprog()) {
                            val intent = Intent(requireContext(), LiveprogEditorActivity::class.java)
                            intent.putExtra(LiveprogEditorActivity.EXTRA_TARGET_FILE, selectedFile.absolutePath)
                            startActivity(intent)
                        }
                        dismiss()
                    }
                    R.id.rename_selection -> {
                        showFileNamePrompt(
                            R.string.filelibrary_context_rename,
                            selectedFile,
                            autofill = true,
                            allowOverwrite = false
                        ) {
                            selectedFile.renameTo(it)
                            requireContext().toast(getString(R.string.filelibrary_renamed, it.nameWithoutExtension))
                            refresh()
                        }
                    }
                    R.id.delete_selection -> {
                        selectedFile.delete()
                        requireContext().toast(getString(R.string.filelibrary_deleted, name))
                        refresh()

                        // If this file was active, we need to reset the selection to null
                        if (fileLibPreference.callChangeListener("")) {
                            fileLibPreference.value = ""
                        }
                    }
                    R.id.duplicate_selection -> {
                        showFileNamePrompt(
                            R.string.filelibrary_context_duplicate,
                            selectedFile,
                            autofill = true,
                            allowOverwrite = false
                        ) {
                            selectedFile.copyTo(it)
                            refresh()
                        }
                    }
                    R.id.share_selection -> {
                        val uri = FileProvider.getUriForFile(
                            requireContext(),
                            BuildConfig.APPLICATION_ID + ".file_library_provider",
                            selectedFile
                        )
                        val shareIntent = Intent(Intent.ACTION_SEND)
                        shareIntent.putExtra(Intent.EXTRA_STREAM, uri)
                        shareIntent.flags = Intent.FLAG_GRANT_READ_URI_PERMISSION
                        shareIntent.type = "application/octet-stream"
                        startActivity(Intent.createChooser(shareIntent, getString(R.string.filelibrary_context_share)))
                    }
                }
                true
            }
            popupMenu.show()
            true
        }

        refreshSelection()

        return dialog
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        importLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
            if (result.resultCode != Activity.RESULT_OK)
                return@registerForActivityResult

            result?.data?.data?.let { uri ->
                val correctType = fileLibPreference.hasCorrectExtension(
                    StorageUtils.queryName(
                        requireContext(),
                        uri
                    ) ?: "INVALID"
                )
                if(!correctType)
                {
                    requireContext().showAlert(R.string.filelibrary_unsupported_format_title,
                        R.string.filelibrary_unsupported_format)
                    return@let
                }

                StorageUtils.openInputStreamSafe(requireContext(), uri)?.use {
                    if(!fileLibPreference.hasValidContent(it)) {
                        Timber.e("File rejected due to invalid content")
                        requireContext().showAlert(R.string.filelibrary_corrupted_title,
                            R.string.filelibrary_corrupted)
                        return@let
                    }
                }

                val file = StorageUtils.importFile(requireContext(),
                    fileLibPreference.directory?.absolutePath ?: "", uri)
                if(file == null)
                {
                    Timber.e("Failed to import file")
                    return@let
                }

                CoroutineScope(Dispatchers.Main).launch {
                    delay(150L)
                    refresh()
                }
            }
        }

        return super.onCreateView(inflater, container, savedInstanceState)
    }

    override fun onDialogClosed(positiveResult: Boolean) {
        scriptScannerScope.cancel()

        if (positiveResult && !clickedEntryValue.isNullOrEmpty()) {
            val value = clickedEntryValue.toString()

            if (fileLibPreference.callChangeListener(value)) {
                fileLibPreference.value = value
            }
        }
    }

    private fun showFileNamePrompt(
        @StringRes title: Int,
        selectedFile: File,
        autofill: Boolean,
        allowOverwrite: Boolean,
        callback: (File) -> Unit,
    ) {
        requireContext().showInputAlert(
            layoutInflater,
            title,
            R.string.filelibrary_new_file_name,
            if (autofill) selectedFile.nameWithoutExtension else "",
            false,
            null
        ) {
            if (it != null) {
                val newFile =
                    File(selectedFile.absoluteFile.parentFile!!.absolutePath + File.separator + it + "." + selectedFile.extension)
                if (newFile.exists() && !allowOverwrite) {
                    requireContext().toast(getString(R.string.filelibrary_file_exists))
                    return@showInputAlert
                }
                callback.invoke(newFile)
            }
        }
    }

    /** Tells the DSP screen to add/remove the extra Liveprog cards. */
    private fun notifySlotsChanged() {
        requireContext().sendLocalBroadcast(Intent(Constants.ACTION_LIVEPROG_SLOTS_CHANGED))
    }

    private fun refresh() {
        fileLibPreference.refresh()
        dialog.listView.adapter = createAdapter()

        onTagClicked(currentTag, currentTagScripts)
        refreshSelection()
    }

    private fun refreshSelection() {
        if (fileLibPreference.isPreset())
            return

        val selectedIndex = (dialog.listView.adapter as? ListItemAdapter)?.indexOf(fileLibPreference.value) ?: -1
        if (selectedIndex >= 0) {
            dialog.listView.setItemChecked(selectedIndex, true)
            dialog.listView.setSelection(selectedIndex)
        }
        else {
            dialog.listView.setItemChecked(-1, true)
        }
    }

    private fun onTagClicked(tag: String?, scripts: List<String>?) {
        currentTag = tag
        currentTagScripts = scripts
        Timber.e(tag)
        Timber.e(scripts?.joinToString(";"))

        (dialog.listView.adapter as Filterable).filter.filter(scripts?.joinToString(";"))
    }

    private fun scanScriptMetadata() {
        if(!fileLibPreference.isLiveprog())
            return

        scriptScannerScope.launch {
            binding.tags.removeAllViews()

            val untaggedScripts = mutableListOf<String>()
            val foundTags = mutableMapOf<String /* tag */, MutableList<String> /* scripts */>()

            fileLibPreference.entryValues.forEach { path ->
                context?.let {
                    eelParser.load(
                        FileLibraryPreference.createFullPathCompat(it, path.toString()),
                        skipProperties = true
                    )
                } ?: return@forEach

                if(eelParser.tags.isEmpty())
                    eelParser.fileName?.let(untaggedScripts::add)
                eelParser.tags.forEach { tag ->
                    val prettyfied = tag.lowercase()
                        .replaceFirstChar { if (it.isLowerCase()) it.titlecase(Locale.getDefault()) else it.toString() }
                        .run { if(length <= 3) uppercase() else this }


                    eelParser.fileName?.let {
                        if (foundTags.containsKey(prettyfied))
                            foundTags[prettyfied]?.add(it)
                        else
                            foundTags[prettyfied] = mutableListOf(it)
                    }
                }
            }

            if(untaggedScripts.isNotEmpty())
                foundTags["Untagged"] = ((foundTags["Untagged"] ?: listOf()) + untaggedScripts).toMutableList()

            withContext(Dispatchers.Main) {
                val sorted = foundTags.entries
                    .sortedWith(compareByDescending<Map.Entry<String, List<String>>> { it.value.size }
                        .thenBy { it.key })

                for((tag, scripts) in sorted) {
                    binding.tags.addView(Chip(dialog.context, null,
                        com.google.android.material.R.style.Widget_Material3_Chip_Assist_Elevated)
                        .apply {
                            text = tag
                            isCheckable = true
                            setOnClickListener { if(isChecked) onTagClicked(tag, scripts) }
                        })
                }
                binding.tags.setOnCheckedStateChangeListener { _, checkedIds ->
                    if(checkedIds.isEmpty())
                        onTagClicked(null, null)
                }
            }
        }
    }

    @SuppressLint("PrivateResource")
    private fun createAdapter(): ListAdapter {
        val raw = fileLibPreference.entries.zip(fileLibPreference.entryValues) {
                a, b -> Entry(a, b)
        }
        val entries =
            if (!libEditable) raw.toTypedArray()
            else {
                libLayout.sync(raw.map { it.name.toString() })
                val byName = raw.associateBy { it.name.toString() }
                val q = libSearch.trim().lowercase(Locale.getDefault())
                val out = mutableListOf<Entry>()
                libLayout.tokens.forEach { token ->
                    if (libLayout.isHeader(token)) {
                        if (q.isEmpty()) out.add(Entry(libLayout.headerName(token), "", isHeader = true))
                    } else if (!libLayout.hidden.contains(token) &&
                        (q.isEmpty() || token.lowercase(Locale.getDefault()).contains(q))) {
                        byName[token]?.let { out.add(it) }
                    }
                }
                // Drop headers that ended up with nothing under them
                val cleaned = mutableListOf<Entry>()
                out.forEachIndexed { i, e ->
                    if (e.isHeader && (i + 1 >= out.size || out[i + 1].isHeader)) return@forEachIndexed
                    cleaned.add(e)
                }
                cleaned.toTypedArray()
            }
        return ListItemAdapter(
            requireContext(),
            if (fileLibPreference.isPreset()) R.layout.item_preset_list
            else if (multiMode) R.layout.item_liveprog_multi
            else com.google.android.material.R.layout.select_dialog_singlechoice_material,
            android.R.id.text1,
            entries,
            fileLibPreference.isLiveprog()
        ) {
            refreshSelection()
        }
    }

    override fun onPrepareDialogBuilder(builder: AlertDialog.Builder) {
        super.onPrepareDialogBuilder(builder)
        binding = DialogFilelibraryBinding.inflate(layoutInflater)
        binding.tags.isSingleSelection = true
        binding.tags.layoutTransition = LayoutTransition().apply {
            enableTransitionType(LayoutTransition.CHANGING)
        }
        binding.root.layoutTransition = LayoutTransition().apply {
            enableTransitionType(LayoutTransition.CHANGING)
        }

        builder.setView(binding.root)
        builder.setNeutralButton(getString(R.string.action_import)) { _, _ -> }

        if(fileLibPreference.isPreset()) {
            builder.setNeutralButton(getString(R.string.add)) { _, _ -> }
            builder.setNegativeButton(getString(R.string.close)) { _, _ -> }
            builder.setTitle(getString(R.string.action_presets))
        }

        builder.setAdapter(createAdapter()) { _, position ->
            // Normally unreachable: the listener installed in setOnShowListener
            // replaces this one. Kept as the fallback for the case where the
            // dialog is shown without that pass, and sharing loadPreset so the
            // two paths cannot drift apart again.
            val item = dialog.listView.adapter.getItem(position) as Entry

            if(fileLibPreference.isPreset())
                loadPreset(item)

            clickedEntryValue = item.value

            // Simulate positive button press and dismiss
            this.onClick(dialog, DialogInterface.BUTTON_POSITIVE)
            dialog.dismiss()
        }

        scanScriptMetadata()
    }

    private fun import() {
        try {
            importLauncher.launch(Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "*/*"
            })
        }
        catch(ex: Exception) {
            Timber.e("No activity found")
            Timber.i(ex)
            requireContext().toast(R.string.no_activity_found)
        }
    }


    // ================= library search / edit / download (IRS + DDC) =========

    @SuppressLint("ClickableViewAccessibility")
    private fun setupLibraryBar() {
        binding.libEditToggle.setOnClickListener { toggleLibEdit() }
        binding.libSearchInput.addTextChangedListener(object : android.text.TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun afterTextChanged(e: android.text.Editable?) {
                libSearch = e?.toString() ?: ""
                if (!libEditMode) refresh()
            }
        })
        binding.libHideAll.setOnClickListener {
            libLayout.setAllHidden(libLayout.tokens.filter { !libLayout.isHeader(it) }, true)
            refreshEditList()
        }
        binding.libShowAll.setOnClickListener {
            libLayout.setAllHidden(libLayout.tokens.filter { !libLayout.isHeader(it) }, false)
            refreshEditList()
        }
        binding.libAddGroup.setOnClickListener {
            requireContext().showInputAlert(
                layoutInflater, R.string.effect_group_add, R.string.effect_group_add, "", false, null
            ) { name ->
                if (!name.isNullOrBlank()) { libLayout.addGroup(name.trim()); refreshEditList() }
            }
        }
        // Pulling down while already at the top reveals the search bar,
        // matching the main page.
        dialog.listView.setOnTouchListener { _, ev ->
            when (ev.actionMasked) {
                android.view.MotionEvent.ACTION_DOWN -> revealStartY = ev.y
                android.view.MotionEvent.ACTION_MOVE -> {
                    val atTop = dialog.listView.firstVisiblePosition == 0 &&
                        (dialog.listView.getChildAt(0)?.top ?: 0) >= 0
                    if (atTop && !binding.libSearchBar.isVisible &&
                        revealStartY >= 0 && ev.y - revealStartY > 90) {
                        binding.libSearchBar.isVisible = true
                    }
                }
            }
            false
        }
    }

    private fun toggleLibEdit() {
        libEditMode = !libEditMode
        binding.libEditToggle.setImageResource(
            if (libEditMode) R.drawable.ic_twotone_check_24dp else R.drawable.ic_twotone_edit_24dp)
        binding.libSearchBar.isVisible = true
        binding.libEditActions.isVisible = libEditMode
        binding.libSearchInput.isEnabled = !libEditMode
        if (libEditMode) {
            dialog.listView.choiceMode = android.widget.ListView.CHOICE_MODE_NONE
            refreshEditList()
        } else {
            dialog.listView.choiceMode = android.widget.ListView.CHOICE_MODE_SINGLE
            refresh()
        }
    }

    private fun refreshEditList() {
        dialog.listView.adapter = LibEditAdapter()
    }

    /** Every token (headers + files, hidden included) with move/hide/group controls. */
    private inner class LibEditAdapter : android.widget.BaseAdapter() {
        private val tokens get() = libLayout.tokens
        override fun getCount() = tokens.size
        override fun getItem(position: Int) = tokens[position]
        override fun getItemId(position: Int) = position.toLong()

        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            val row = convertView
                ?: layoutInflater.inflate(R.layout.item_filelib_edit, parent, false)
            val token = tokens[position]
            val isHeader = libLayout.isHeader(token)
            val name = row.findViewById<TextView>(R.id.edit_name)
            val eye = row.findViewById<android.widget.ImageButton>(R.id.edit_eye)
            val more = row.findViewById<android.widget.ImageButton>(R.id.edit_more)

            name.text = if (isHeader) libLayout.headerName(token) else token
            name.setTypeface(null, if (isHeader) android.graphics.Typeface.BOLD else android.graphics.Typeface.NORMAL)
            val hidden = !isHeader && libLayout.hidden.contains(token)
            name.alpha = if (hidden) 0.4f else 1f

            row.findViewById<View>(R.id.edit_up).setOnClickListener {
                libLayout.move(token, up = true); notifyDataSetChanged()
            }
            row.findViewById<View>(R.id.edit_down).setOnClickListener {
                libLayout.move(token, up = false); notifyDataSetChanged()
            }
            eye.isVisible = !isHeader
            eye.setImageResource(
                if (hidden) R.drawable.ic_twotone_visibility_off_24dp
                else R.drawable.ic_twotone_visibility_24dp)
            eye.setOnClickListener {
                libLayout.setHidden(token, !libLayout.hidden.contains(token))
                notifyDataSetChanged()
            }
            more.setOnClickListener { v -> showTokenMenu(v, token, isHeader) }
            return row
        }
    }

    private fun showTokenMenu(anchor: View, token: String, isHeader: Boolean) {
        val menu = PopupMenu(requireContext(), anchor)
        if (isHeader) {
            menu.menu.add(0, 1, 0, R.string.effect_group_rename)
            menu.menu.add(0, 2, 1, R.string.effect_group_delete)
        } else {
            menu.menu.add(0, 3, 0, R.string.filelib_move_to_group)
        }
        menu.setOnMenuItemClickListener { item ->
            when (item.itemId) {
                1 -> requireContext().showInputAlert(
                    layoutInflater, R.string.effect_group_rename, R.string.effect_group_rename,
                    libLayout.headerName(token), false, null
                ) { name ->
                    if (!name.isNullOrBlank()) { libLayout.renameGroup(token, name.trim()); refreshEditList() }
                }
                2 -> { libLayout.removeGroup(token); refreshEditList() }
                3 -> {
                    val groups = libLayout.tokens.filter { libLayout.isHeader(it) }
                    val labels = (listOf(getString(R.string.filelib_no_group)) +
                        groups.map { libLayout.headerName(it) }).toTypedArray()
                    AlertDialog.Builder(requireContext())
                        .setTitle(R.string.filelib_move_to_group)
                        .setItems(labels) { _, which ->
                            libLayout.assignToGroup(token, if (which == 0) null else groups[which - 1])
                            refreshEditList()
                        }
                        .show()
                }
            }
            true
        }
        menu.show()
    }

    // ------------------------------ downloading -----------------------------

    private fun libExtensions() =
        if (fileLibPreference.isVdc()) listOf(".vdc") else listOf(".irs", ".wav", ".flac")

    private fun showDownloadSources() {
        val defaults = GithubLibraryDownloader.defaultSources()
        val customs = GithubLibraryDownloader.customSources(requireContext())
        val all = defaults + customs
        val labels = all.map { it.label } +
            (if (githubSearchEnabled) listOf(getString(R.string.filelib_source_search)) else emptyList()) +
            getString(R.string.filelib_source_add)
        val dlg = AlertDialog.Builder(requireContext())
            .setTitle(R.string.filelib_download_title)
            .setItems(labels.toTypedArray()) { _, which ->
                when {
                    which < all.size -> browseSource(all[which])
                    githubSearchEnabled && which == all.size -> promptRepoSearch()
                    else -> promptAddSource()
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
            .create()
        // Long-press removes a source the user added
        dlg.listView?.setOnItemLongClickListener { _, _, pos, _ ->
            if (pos >= defaults.size && pos < all.size) {
                val src = all[pos]
                GithubLibraryDownloader.saveCustomSources(
                    requireContext(), customs.filterNot { it.repo == src.repo })
                dlg.dismiss()
                requireContext().toast(getString(R.string.filelib_source_removed, src.label))
                true
            } else false
        }
        dlg.show()
    }

    private fun promptAddSource() {
        requireContext().showInputAlert(
            layoutInflater,
            R.string.filelib_source_add,
            R.string.filelib_source_add_hint,
            "", false, null
        ) { input ->
            if (!isAdded) return@showInputAlert
            val parsed = input?.let { GithubLibraryDownloader.parseRepoInput(it) }
            if (parsed == null) {
                requireContext().toast(getString(R.string.filelib_source_invalid))
                return@showInputAlert
            }
            val (repo, branchIn) = parsed
            scriptScannerScope.launch {
                val branch = branchIn ?: GithubLibraryDownloader.defaultBranch(repo)
                withContext(Dispatchers.Main) {
                    val c = context ?: return@withContext
                    if (!isAdded) return@withContext
                    val customs = GithubLibraryDownloader.customSources(c)
                    if (customs.none { it.repo == repo }) {
                        customs.add(GithubLibraryDownloader.Source(repo, branch, repo))
                        GithubLibraryDownloader.saveCustomSources(c, customs)
                    }
                    browseSource(GithubLibraryDownloader.Source(repo, branch, repo))
                }
            }
        }
    }

    private fun promptRepoSearch() {
        requireContext().showInputAlert(
            layoutInflater,
            R.string.filelib_source_search,
            R.string.filelib_source_search_hint,
            if (fileLibPreference.isVdc()) "viper ddc" else "viper irs impulse",
            false, null
        ) { query ->
            if (!isAdded || query.isNullOrBlank()) return@showInputAlert
            libNetwork(getString(R.string.filelib_searching), {
                GithubLibraryDownloader.searchRepositories(query.trim())
            }) { repos ->
                if (repos.isEmpty()) {
                    requireContext().toast(getString(R.string.filelibrary_no_presets)); return@libNetwork
                }
                AlertDialog.Builder(requireContext())
                    .setTitle(R.string.filelib_source_search)
                    .setItems(repos.map { it.label }.toTypedArray()) { _, which ->
                        browseSource(repos[which])
                    }
                    .setNegativeButton(android.R.string.cancel, null)
                    .show()
            }
        }
    }

    private fun browseSource(source: GithubLibraryDownloader.Source) {
        libNetwork(getString(R.string.filelib_loading_list), {
            GithubLibraryDownloader.listFiles(source.repo, source.branch, libExtensions())
        }) { files ->
            val localNames = fileLibPreference.directory?.list()
                ?.map { it.lowercase(Locale.getDefault()) } ?: emptyList()
            val fresh = files.filter { !localNames.contains(it.name.lowercase(Locale.getDefault())) }
            if (fresh.isEmpty()) {
                requireContext().toast(getString(R.string.filelib_all_downloaded)); return@libNetwork
            }
            val checked = BooleanArray(fresh.size)
            AlertDialog.Builder(requireContext())
                .setTitle(getString(R.string.filelib_pick_files, fresh.size))
                .setMultiChoiceItems(fresh.map { it.name }.toTypedArray(), checked) { _, i, v ->
                    checked[i] = v
                }
                .setPositiveButton(R.string.filelib_download) { _, _ ->
                    val picked = fresh.filterIndexed { i, _ -> checked[i] }
                    if (picked.isNotEmpty()) downloadFiles(source, picked)
                }
                .setNegativeButton(android.R.string.cancel, null)
                .show()
        }
    }

    private fun downloadFiles(
        source: GithubLibraryDownloader.Source,
        files: List<GithubLibraryDownloader.RemoteFile>
    ) {
        val dir = fileLibPreference.directory ?: return
        context?.toast(getString(R.string.filelib_downloading, files.size)) ?: return
        scriptScannerScope.launch {
            var ok = 0
            files.forEach { f ->
                runCatching {
                    if (GithubLibraryDownloader.download(source.repo, source.branch, f, dir)) ok++
                }
            }
            withContext(Dispatchers.Main) {
                val c = context ?: return@withContext
                if (!isAdded) return@withContext
                c.toast(c.getString(R.string.filelib_downloaded, ok))
                refresh()
            }
        }
    }

    /** Runs a network call off the main thread with a toast + friendly errors. */
    private fun <T> libNetwork(message: String, work: () -> T, done: (T) -> Unit) {
        context?.toast(message) ?: return
        scriptScannerScope.launch {
            val result = runCatching { work() }
            withContext(Dispatchers.Main) {
                // The dialog may have been dismissed while the request ran
                val c = context ?: return@withContext
                if (!isAdded) return@withContext
                result.fold(done) { e ->
                    c.toast(
                        if (e is GithubLibraryDownloader.RateLimitException) e.message!!
                        else c.getString(R.string.filelib_network_failed))
                }
            }
        }
    }

    data class Entry(val name: CharSequence, val value: CharSequence, val isHeader: Boolean = false) {
        override fun toString() = name.toString()
    }

    private inner class ListItemAdapter(
        context: Context, resource: Int, textViewResourceId: Int, val allItems: Array<Entry>,
        val allowFilter: Boolean = false, val onFiltered: () -> Unit
    ) : ArrayAdapter<Entry>(context, resource, textViewResourceId, allItems), Filterable {
        private var items: Array<Entry> = allItems

        fun indexOf(value: String): Int {
            return items.map { it.value }.indexOf(value)
        }
        override fun getViewTypeCount(): Int = 2
        override fun getItemViewType(position: Int): Int = if (items[position].isHeader) 1 else 0
        override fun areAllItemsEnabled(): Boolean = false
        override fun isEnabled(position: Int): Boolean = !items[position].isHeader

        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            if (items[position].isHeader) {
                val header = convertView
                    ?: layoutInflater.inflate(R.layout.item_filelib_header, parent, false)
                header.findViewById<TextView>(R.id.header_text).text = items[position].name
                return header
            }
            val view = if (convertView?.findViewById<TextView>(R.id.header_text) == null)
                super.getView(position, convertView, parent)
            else super.getView(position, null, parent)
            if (multiMode) {
                val badge = view.findViewById<TextView>(R.id.slot_badge)
                val slot = LiveprogSlots.slotNumberOf(context, items[position].value.toString())
                if (badge != null) {
                    badge.text = slot?.toString() ?: ""
                    badge.setBackgroundResource(
                        if (slot != null) R.drawable.bg_slot_badge
                        else R.drawable.bg_slot_badge_empty
                    )
                }
            }
            return view
        }

        override fun hasStableIds(): Boolean = true
        override fun getCount(): Int = items.size
        override fun getItem(position: Int): Entry = items[position]
        override fun getItemId(position: Int): Long = position.toLong()
        override fun getFilter(): Filter {
            return object : Filter() {
                @Suppress("UNCHECKED_CAST")
                override fun publishResults(charSequence: CharSequence?, filterResults: FilterResults) {
                    items = filterResults.values as Array<Entry>
                    notifyDataSetChanged()
                    onFiltered.invoke()
                }

                override fun performFiltering(charSequence: CharSequence?): FilterResults {
                    if(!allowFilter)
                        return FilterResults().apply { values = allItems }

                    val query = charSequence?.toString()?.split(";")
                    return FilterResults().apply {
                        values = if (query.isNullOrEmpty())
                            allItems
                        else
                            allItems.filter { query.contains(it.name) }.toTypedArray()
                    }
                }
            }
        }
    }

    @Suppress("UNCHECKED_CAST")
    override fun <T : Preference?> findPreference(key: CharSequence): T? {
        return if(key == arguments?.getString(BUNDLE_KEY))
            fileLibPreference as? T
        else
            null
    }

    companion object {
        private const val BUNDLE_KEY = "key"

        fun newInstance(key: String): FileLibraryDialogFragment {
            val fragment = FileLibraryDialogFragment()

            val args = Bundle()
            args.putString(BUNDLE_KEY, key)
            fragment.arguments = args
            return fragment
        }
    }
}
