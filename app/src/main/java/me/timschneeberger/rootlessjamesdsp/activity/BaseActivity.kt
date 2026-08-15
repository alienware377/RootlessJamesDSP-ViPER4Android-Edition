package me.timschneeberger.rootlessjamesdsp.activity

import android.app.ActivityManager
import android.content.SharedPreferences
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.getSystemService
import me.timschneeberger.rootlessjamesdsp.MainApplication
import me.timschneeberger.rootlessjamesdsp.R
import me.timschneeberger.rootlessjamesdsp.delegates.ThemingDelegate
import me.timschneeberger.rootlessjamesdsp.delegates.ThemingDelegateImpl
import me.timschneeberger.rootlessjamesdsp.utils.preferences.Preferences
import org.koin.android.ext.android.inject
import org.koin.core.component.KoinComponent

abstract class BaseActivity :
    AppCompatActivity(),
    SharedPreferences.OnSharedPreferenceChangeListener,
    KoinComponent,
    ThemingDelegate by ThemingDelegateImpl() {

    /* Preferences */
    protected val prefsApp: Preferences.App by inject()
    protected val prefsVar: Preferences.Var by inject()

    protected open val disableAppTheme = false

    protected val app
        get() = application as MainApplication

    override fun onCreate(savedInstanceState: Bundle?) {
        if(!disableAppTheme)
            applyAppTheme(this)
        prefsApp.registerOnSharedPreferenceChangeListener(this)
        super.onCreate(savedInstanceState)
    }

    override fun onDestroy() {
        prefsApp.unregisterOnSharedPreferenceChangeListener(this)
        super.onDestroy()
    }

    override fun onResume() {
        super.onResume()

        // Exclude from recents if enabled
        getSystemService<ActivityManager>()?.appTasks?.takeIf { it.isNotEmpty() }?.forEach {
            it.setExcludeFromRecents(prefsApp.get<Boolean>(R.string.key_exclude_app_from_recents))
        }
    }

    override fun onContentChanged() {
        super.onContentChanged()
        // Hand-set colours can't live in a runtime theme, so they're painted on
        // after the layout exists. No-op unless manual mode is on.
        me.timschneeberger.rootlessjamesdsp.utils.ManualThemeTint.apply(this)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        if(key == getString(R.string.key_appearance_pure_black) ||
            key == getString(R.string.key_appearance_app_theme) ||
            key == me.timschneeberger.rootlessjamesdsp.utils.V4aMode.KEY ||
            key == me.timschneeberger.rootlessjamesdsp.utils.V4aIconColors.KEY_LAYOUT) {
            // Drop the cached copies before the screen rebuilds with them
            me.timschneeberger.rootlessjamesdsp.utils.V4aMode.invalidate()
            me.timschneeberger.rootlessjamesdsp.utils.V4aIconColors.invalidate()
            if(!disableAppTheme)
                ActivityCompat.recreate(this)
        }
    }
}
