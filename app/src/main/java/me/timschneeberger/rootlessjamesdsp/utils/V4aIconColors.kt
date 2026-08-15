package me.timschneeberger.rootlessjamesdsp.utils

import android.content.Context
import android.util.TypedValue
import me.timschneeberger.rootlessjamesdsp.R

/**
 * The ViPER4Android classic theme tints every effect icon a single purple,
 * matching the original app's one-accent look, rather than the per-effect
 * colours tried earlier.
 */
object V4aIconColors {
    /** True when the active theme asks for purple effect icons. */
    fun isEnabled(context: Context): Boolean {
        val tv = TypedValue()
        return context.theme.resolveAttribute(R.attr.v4aColorfulIcons, tv, true) && tv.data != 0
    }

    /**
     * True when the active theme wants the classic ViPER4Android *layout*:
     * flat cards, top-right menus, device picker in the footer, master limiter
     * doubling as the power switch. This is presentation only - it never
     * removes a feature, so it is safe outside V4A-only mode.
     */
    /** Preference key for using the classic layout with any theme. */
    const val KEY_LAYOUT = "v4a_classic_layout"

    /** Cached for the same reason as V4aMode: this is read during row binding. */
    @Volatile private var layoutPref: Boolean? = null

    fun invalidate() { layoutPref = null }

    fun isClassicLayout(context: Context): Boolean {
        // Either the classic theme asks for it, or the user turned it on for
        // whatever theme they're using. Structure and colour are separate
        // choices, so a custom theme keeps its own surface and background.
        val pref = layoutPref ?: context.getSharedPreferences(
            me.timschneeberger.rootlessjamesdsp.utils.Constants.PREF_APP,
            Context.MODE_PRIVATE
        ).getBoolean(KEY_LAYOUT, false).also { layoutPref = it }
        if (pref) return true
        val tv = TypedValue()
        return context.theme.resolveAttribute(R.attr.v4aClassicLayout, tv, true) && tv.data != 0
    }

    fun tint(context: Context): Int {
        val tv = TypedValue()
        context.theme.resolveAttribute(com.google.android.material.R.attr.colorPrimary, tv, true)
        return tv.data
    }
}
