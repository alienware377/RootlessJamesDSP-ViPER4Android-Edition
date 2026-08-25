package me.timschneeberger.rootlessjamesdsp.preference

import android.animation.ValueAnimator
import android.annotation.SuppressLint
import android.content.Context
import android.content.res.TypedArray
import android.util.AttributeSet
import android.view.View
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.preference.Preference
import androidx.preference.PreferenceGroup
import androidx.preference.PreferenceViewHolder
import androidx.preference.children
import com.google.android.material.materialswitch.MaterialSwitch
import me.timschneeberger.rootlessjamesdsp.R
import me.timschneeberger.rootlessjamesdsp.utils.extensions.animatedValueAs


@SuppressLint("PrivateResource")
class SwitchPreferenceGroup(context: Context, attrs: AttributeSet) : PreferenceGroup(
    context, attrs, androidx.preference.R.attr.preferenceStyle,
    androidx.preference.R.style.Preference_SwitchPreferenceCompat_Material
) {
    private var childrenVisible = false
    private var switch: MaterialSwitch? = null
    private var itemView: View? = null
    private var bgAnimation: ValueAnimator? = null
    private var isIconVisible: Boolean = false

    /**
     * Whether this card's parameters are folded away while the effect stays on.
     *
     * Separate from the switch on purpose. The switch already hides the
     * parameters, but only by turning the effect off, which changes what you
     * are hearing - no use to someone who wants a dozen effects running and a
     * screen they can still scroll. Defaults to expanded, so nothing moves for
     * anyone who does not go looking for it.
     *
     * Persisted beside the effect's own enable flag under its own key, so it
     * travels with a preset and a backup the same way every other setting does.
     */
    private var collapsed = false
    private var collapseToggle: View? = null
    private val collapseKey get() = "${key}_collapsed"

    /**
     * When set, user taps are routed here instead of changing this preference.
     * Needed because this class drives its switch directly and never calls
     * OnPreferenceChangeListener, so that listener can't intercept a toggle.
     */
    var onUserToggle: ((Boolean) -> Unit)? = null
    private var suppressToggleCallback = false
    private var state = false

    init {
        layoutResource = R.layout.preference_switchgroup
        widgetLayoutResource = R.layout.preference_materialswitch
    }

    override fun onSetInitialValue(defaultValue: Any?) {
        // Read before the children are shown, or the card would flash open on
        // every bind and then fold itself.
        collapsed = preferenceManager?.sharedPreferences?.getBoolean(collapseKey, false) ?: false
        setValueInternal(getPersistedBoolean((defaultValue as? Boolean) ?: false), true)
    }

    override fun onGetDefaultValue(a: TypedArray, index: Int): Any = a.getBoolean(index, false)

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)

        itemView = holder.itemView
        itemView?.background = ContextCompat.getDrawable(context, R.drawable.shape_rounded_highlight)
        // Classic layout keeps every row the same colour whether the effect is
        // on or off; only the switch itself shows state. This must NOT skip the
        // rest of binding - the switch, the expand behaviour and the row click
        // listener all live below.
        itemView?.background?.alpha = 0

        bgAnimation = ValueAnimator.ofInt(TRANSITION_MIN, TRANSITION_MAX).apply {
            duration = 200 // milliseconds
            addUpdateListener { animator ->
                itemView?.background?.alpha = animator.animatedValueAs<Int>() ?: 0
            }
        }

        setChildrenVisibility(state)
        animateHeaderState(state)
        setIsIconVisible(isIconVisible)

        switch = (holder.findViewById(R.id.switchWidget) as MaterialSwitch).apply {
            // Apply initial state. Guarded: a recycled switch may still carry a
            // listener, and assigning isChecked would fire it as a user toggle.
            suppressToggleCallback = true
            isChecked = state
            suppressToggleCallback = false
            isVisible = isSelectable

            setOnCheckedChangeListener { _, isChecked ->
                if (suppressToggleCallback) return@setOnCheckedChangeListener
                val handler = onUserToggle
                if (handler != null) handler(isChecked)
                else setValueInternal(isChecked, false)
            }
        }

        collapseToggle = holder.findViewById(R.id.collapseToggle)?.apply {
            // Its own click listener, so tapping the chevron does not fall
            // through to the row and toggle the effect instead.
            setOnClickListener { setCollapsed(!collapsed) }
        }
        refreshCollapseToggle()

        holder.itemView.apply {
            setOnClickListener {
                switch?.toggle()
            }
        }
    }

    private fun setCollapsed(value: Boolean) {
        if (collapsed == value) return
        collapsed = value
        preferenceManager?.sharedPreferences?.edit()?.putBoolean(collapseKey, value)?.apply()
        setChildrenVisibility(state)
        refreshCollapseToggle()
    }

    private fun refreshCollapseToggle() {
        // Nothing to fold away when the card is off, and showing a control that
        // appears to do nothing is worse than not showing it.
        collapseToggle?.isVisible = state && isSelectable
        (collapseToggle as? androidx.appcompat.widget.AppCompatImageView)?.setImageResource(
            if (collapsed) R.drawable.ic_baseline_keyboard_arrow_down_24dp
            else R.drawable.ic_baseline_keyboard_arrow_up_24dp
        )
        collapseToggle?.contentDescription =
            context.getString(if (collapsed) R.string.card_expand else R.string.card_collapse)
    }

    override fun onPrepareAddPreference(preference: Preference): Boolean {
        preference.isVisible = childrenVisible
        return super.onPrepareAddPreference(preference)
    }

    fun setIsIconVisible(value: Boolean) {
        isIconVisible = value
        itemView?.findViewById<View>(R.id.icon_frame)?.isVisible = value
    }

    fun setValue(value: Boolean) {
        setValueInternal(value, true)
    }

    private fun setValueInternal(value: Boolean, notifyChanged: Boolean) {
        setChildrenVisibility(value)
        if (state != value) {
            animateHeaderState(value)

            state = value
            persistBoolean(state)
            if (notifyChanged) {
                notifyChanged()
            }
        }
        // After the state settles, not before: the chevron only belongs on a
        // card that is showing something, so it follows the switch as well as
        // its own state.
        refreshCollapseToggle()
    }

    private fun animateHeaderState(selected: Boolean) {
        // Classic layout: rows never change colour with state. Guarded here
        // rather than at each call site so no path can re-tint them.
        val classic = context?.let {
            me.timschneeberger.rootlessjamesdsp.utils.V4aIconColors.isClassicLayout(it)
        } ?: false
        if (classic) {
            itemView?.background?.alpha = 0
            return
        }
        val current = bgAnimation?.animatedValueAs<Int>() ?: 0
        if(selected && current < TRANSITION_MAX)
            bgAnimation?.start()
        else if(!selected && current > TRANSITION_MIN)
            bgAnimation?.reverse()
    }

    var childVisibilityFilter: ((Preference) -> Boolean)? = null

    fun refreshChildrenVisibility() {
        setChildrenVisibility(state)
    }

    private fun setChildrenVisibility(visible: Boolean) {
        // V4A-only mode has no explanation rows. Enforced here rather than once
        // at setup, because expanding a card re-shows every child and would
        // otherwise undo it - and here it can't be missed by a card that sets
        // its own childVisibilityFilter.
        val hideInfo = context?.let {
            me.timschneeberger.rootlessjamesdsp.utils.V4aMode.isOn(it)
        } ?: false
        children.forEach {
            val allowed = (childVisibilityFilter?.invoke(it) != false) &&
                    !(hideInfo && it.key == KEY_SECTION_INFO)
            it.isVisible = visible && !collapsed && allowed
        }
    }

    companion object {
        private const val KEY_SECTION_INFO = "section_info"

        private const val TRANSITION_MIN = 0
        private const val TRANSITION_MAX = 255
    }
}