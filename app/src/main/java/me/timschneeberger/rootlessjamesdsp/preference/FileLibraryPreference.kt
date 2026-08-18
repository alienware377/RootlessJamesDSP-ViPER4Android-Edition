package me.timschneeberger.rootlessjamesdsp.preference

import android.content.Context
import android.content.res.TypedArray
import android.util.AttributeSet
import androidx.preference.ListPreference
import androidx.preference.Preference.SummaryProvider
import me.timschneeberger.rootlessjamesdsp.R
import me.timschneeberger.rootlessjamesdsp.model.preset.Preset
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.toast
import timber.log.Timber
import java.io.File
import java.io.InputStream


class FileLibraryPreference(context: Context, attrs: AttributeSet?) :
    ListPreference(context, attrs,
        androidx.preference.R.attr.dialogPreferenceStyle,
        androidx.preference.R.attr.preferenceFragmentListStyle
    ) {

    var directory: File? = null
    var type: String = "unknown"
        set(value) {
            field = value

            summaryProvider = SummaryProvider<ListPreference> {
                if(it.entry.isNullOrBlank())
                    if(isLiveprog()) context.getString(R.string.liveprog_no_script_selected) else context.getString(
                        R.string.filelibrary_no_file_selected)
                else
                    it.entry
            }

            directory = File(context.getExternalFilesDir(null), type)
            // "unknown" is the placeholder the init block sets before the real
            // type arrives. It names a directory that deliberately is not
            // created, so listing it can only ever produce an empty result -
            // and an empty result posted late lands on top of a good one.
            if(type.lowercase() != "unknown") {
                directory?.mkdir()
                // Off the main thread: this runs while the preference inflates,
                // and listing a populated library over external storage is slow
                // - the DDC card measured three seconds on its own, which is
                // most of the stutter when scrolling in after a cold open. The
                // dialog paths refresh synchronously before showing, so entries
                // are always current by the time they are actually needed.
                refreshAsync()
            }
        }

    init {
        with(context.obtainStyledAttributes(attrs, R.styleable.FileLibraryPreference)) {
            type = getString(R.styleable.FileLibraryPreference_type) ?: "unknown"
        }
    }

    override fun onGetDefaultValue(a: TypedArray, index: Int): Any {
        return a.getString(index) as String
    }

    override fun onSetInitialValue(defaultValue: Any?) {
        // Convert old full path convention to new relative paths
        val init = getPersistedString((defaultValue as? String) ?: "")
        value = if(init.startsWith("/"))
            File(init).toRelativeString(context.getExternalFilesDir(null)!!)
        else
            init
    }

    override fun onClick() {
        refresh()
        super.onClick()
    }

    fun showDialog() {
        refresh()
        preferenceManager.showDialog(this)
    }

    /**
     * Builds the file list on a background thread and applies it on the main
     * one, since entries and entryValues belong to the view layer.
     */
    fun refreshAsync() {
        val dir = directory ?: return
        Thread {
            val built = runCatching { buildFileList(dir) }.getOrNull()
            if (built != null) {
                android.os.Handler(android.os.Looper.getMainLooper()).post {
                    // A build that finished after the directory moved on is
                    // describing somewhere the preference no longer points, so
                    // applying it would overwrite a newer, correct list.
                    if (directory != dir) return@post
                    entries = built.first
                    entryValues = built.second
                    notifyChanged()
                }
            }
        }.apply { priority = Thread.MIN_PRIORITY }.start()
    }

    fun refresh() {
        if(directory == null)
        {
            context.toast(context.getString(R.string.filelibrary_access_fail), false)
            return
        }

        initFileList()
    }

    private fun initFileList() {
        val dir = directory ?: return
        val (names, paths) = buildFileList(dir)
        entries = names
        entryValues = paths
    }

    /** Pure file work, safe to call from any thread. */
    private fun buildFileList(dir: File): Pair<Array<String>, Array<String>> {
        val base = context.getExternalFilesDir(null)
        val result = hashMapOf<String, String>()
        dir.list()?.forEach {
            if(hasCorrectExtension(it))
            {
                val name = it.substringBeforeLast('.')
                val path = File(dir, it).toRelativeString(base!!)
                result[name] = path
            }
        }
        val sorted = result.toSortedMap()
        return sorted.keys.toTypedArray() to sorted.values.toTypedArray()
    }

    fun hasCorrectExtension(it: String): Boolean {
        return (isIrs() && hasIrsExtension(it)) ||
                (isVdc() && hasVdcExtension(it)) ||
                (isLiveprog() && hasLiveprogExtension(it)) ||
                (isPreset() && hasPresetExtension(it))
    }

    fun hasValidContent(stream: InputStream): Boolean {
        return if (isPreset())
            Preset.validate(stream)
        else
            true
    }

    fun isLiveprog(): Boolean {
        return type.lowercase() == "liveprog"
    }
    fun isVdc(): Boolean {
        return type.lowercase() == "ddc"
    }
    fun isIrs(): Boolean {
        return type.lowercase() == "convolver"
    }
    fun isPreset(): Boolean {
        return type.lowercase() == "presets"
    }

    companion object {
        val types = mapOf(
            "Convolver" to listOf(".flac", ".wav", ".irs"),
            "Liveprog" to listOf(".eel"),
            "DDC" to listOf(".vdc"),
            "Presets" to listOf(".tar")
        )

        fun hasIrsExtension(it: String): Boolean {
            return types["Convolver"]!!.any { ext -> it.endsWith(ext) }
        }
        fun hasLiveprogExtension(it: String): Boolean {
            return types["Liveprog"]!!.any { ext -> it.endsWith(ext) }
        }
        fun hasVdcExtension(it: String): Boolean {
            return types["DDC"]!!.any { ext -> it.endsWith(ext) }
        }
        fun hasPresetExtension(it: String): Boolean {
            return types["Presets"]!!.any { ext -> it.endsWith(ext) }
        }


        /**
         * If path starts at root, it is already a full path.
         * If path is relative to the external files dir, it needs to be changed to use a full path.
         */
        fun createFullPathCompat(context: Context, path: String): String {
            return if(path.startsWith("/"))
                path
            else {
                val externalDir = context.getExternalFilesDir(null)
                externalDir ?: Timber.e("getExternalFilesDir returned null")
                externalDir?.let { it.absolutePath + "/" + path } ?: ""
            }
        }
        fun createFullPathNullCompat(context: Context, path: String?): String? {
            path ?: return null
            return createFullPathCompat(context, path)
        }
    }
}