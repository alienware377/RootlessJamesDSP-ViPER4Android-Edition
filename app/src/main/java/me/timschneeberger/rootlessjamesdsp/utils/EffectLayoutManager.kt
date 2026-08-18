package me.timschneeberger.rootlessjamesdsp.utils

import android.content.Context
import android.text.InputType
import android.widget.Button
import android.widget.EditText
import androidx.appcompat.app.AlertDialog
import androidx.core.widget.TextViewCompat
import android.view.DragEvent
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.core.view.ViewCompat
import androidx.core.view.isVisible
import me.timschneeberger.rootlessjamesdsp.R

/**
 * Lets the user reorder, hide and group the effect cards on the main screen.
 *
 * The cards themselves stay declared in the layout; this class only moves the
 * card views around inside their [LinearLayout] container and persists the
 * resulting order/visibility.
 */
class EffectLayoutManager(
    private val context: Context,
    private val container: LinearLayout,
    private val items: MutableList<Item>
) {
    /**
     * @param key stable identifier used for persistence
     * @param viewId id of the card's inner container (or the header view itself)
     * @param titleRes display name shown while editing
     * @param isHeader true for group headers, which own the cards below them
     */
    data class Item(
        val key: String,
        val viewId: Int,
        val titleRes: Int,
        val isHeader: Boolean = false
    )

    private val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
    private val editRows = HashMap<String, View>()

    var editMode = false
        private set

    init {
        // Orders saved by earlier builds were derived from a wrong default and
        // scrambled the layout; drop them once so everyone starts clean.
        if (prefs.getInt(KEY_SCHEMA, 1) < SCHEMA_VERSION) {
            prefs.edit()
                .remove(KEY_ORDER)
                .putInt(KEY_SCHEMA, SCHEMA_VERSION)
                .apply()
        }
    }

    var onEditModeChanged: ((Boolean) -> Unit)? = null

    private var addGroupButton: Button? = null

    private fun showAddGroupButton() {
        val button = Button(context).apply {
            text = context.getString(R.string.effect_group_add)
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
            setOnClickListener {
                val input = EditText(context).apply {
                    inputType = InputType.TYPE_CLASS_TEXT
                    setPadding(dp(24), dp(16), dp(24), dp(8))
                }
                AlertDialog.Builder(context)
                    .setTitle(R.string.effect_group_add)
                    .setView(input)
                    .setPositiveButton(android.R.string.ok) { _, _ ->
                        val name = input.text.toString().trim()
                        if (name.isNotEmpty()) addGroup(name)
                    }
                    .setNegativeButton(android.R.string.cancel, null)
                    .show()
            }
        }
        // Top of the list so it is discoverable in edit mode
        container.addView(button, 0)
        addGroupButton = button
    }

    // ---------------------------------------------------------------- helpers

    private fun dp(value: Int) = (value * context.resources.displayMetrics.density).toInt()

    /** The view that actually sits in the container (the card, not its content). */
    private fun viewFor(item: Item): View? {
        val inner = container.findViewById<View>(item.viewId) ?: return null
        return if (item.isHeader) inner else inner.parent as? View
    }

    private fun itemForView(view: View): Item? =
        items.firstOrNull { viewFor(it) === view }

    private fun hiddenKeys(): MutableSet<String> =
        HashSet(prefs.getStringSet(KEY_HIDDEN, emptySet()) ?: emptySet())

    /** The order the cards currently sit in inside the container. */
    private fun containerOrder(): List<String> {
        val keys = ArrayList<String>()
        for (i in 0 until container.childCount) {
            itemForView(container.getChildAt(i))?.let { keys.add(it.key) }
        }
        return keys
    }

    /**
     * Returns the user's saved order, or null if they've never customised the
     * layout - in which case the layout is left exactly as declared.
     */
    private fun storedOrder(): List<String>? {
        val saved = prefs.getString(KEY_ORDER, null)
            ?.split(",")
            ?.filter { it.isNotBlank() }
            ?: return null

        val natural = containerOrder()
        val result = ArrayList(saved.filter { natural.contains(it) })
        // Effects added by an app update aren't in the saved order yet; slot
        // them in at their declared position instead of dumping them at the end.
        natural.forEachIndexed { index, key ->
            if (!result.contains(key)) {
                result.add(index.coerceAtMost(result.size), key)
            }
        }
        return result
    }

    // ----------------------------------------------------------------- groups

    /** Custom group headers the user created, in the form "key|name;key|name". */
    private fun storedGroups(): MutableList<Pair<String, String>> {
        val raw = prefs.getString(KEY_GROUPS, "") ?: ""
        val list = ArrayList<Pair<String, String>>()
        raw.split(";").filter { it.isNotBlank() }.forEach { entry ->
            val parts = entry.split("|", limit = 2)
            if (parts.size == 2) list.add(parts[0] to parts[1])
        }
        return list
    }

    private fun saveGroups(list: List<Pair<String, String>>) {
        prefs.edit()
            .putString(KEY_GROUPS, list.joinToString(";") { "${it.first}|${it.second}" })
            .apply()
    }

    /**
     * Display name for a header, honouring any rename the user made.
     *
     * Custom groups have no title resource - titleRes is 0 and the name lives
     * in prefs - so the resource lookup is guarded. getString(0) throws
     * Resources.NotFoundException, and a group can genuinely lose its stored
     * name underneath us when a preset replaces the layout file.
     */
    fun groupName(item: Item): String =
        prefs.getString(KEY_NAME_PREFIX + item.key, null)
            ?: storedGroups().firstOrNull { it.first == item.key }?.second
            ?: item.titleRes.takeIf { it != 0 }?.let(context::getString)
            ?: (viewFor(item) as? TextView)?.text?.toString()
            ?: item.key

    /** Hides or shows every group heading (used by V4A-only mode). */
    fun setHeadersVisible(visible: Boolean) {
        items.filter { it.isHeader }.forEach { item ->
            container.findViewById<View>(item.viewId)?.isVisible = visible
        }
    }

    private fun makeHeaderView(text: String): TextView =
        TextView(context).apply {
            id = View.generateViewId()
            tag = TAG_HEADER
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
            gravity = Gravity.CENTER
            setPadding(0, dp(20), 0, dp(8))
            this.text = text
            TextViewCompat.setTextAppearance(
                this, com.google.android.material.R.style.TextAppearance_Material3_TitleMedium
            )
        }

    /** Creates views for any custom groups that don't have one yet. */
    private fun ensureGroupViews() {
        storedGroups().forEach { (key, name) ->
            if (items.any { it.key == key }) return@forEach
            val view = makeHeaderView(name)
            container.addView(view)
            items.add(Item(key, view.id, 0, isHeader = true))
        }
        // Refresh titles (renames) on every apply
        items.filter { it.isHeader }.forEach { item ->
            (viewFor(item) as? TextView)?.text = groupName(item)
        }
    }

    /** The cards that belong to a header: everything until the next header. */
    private fun membersOf(headerKey: String): List<Item> {
        val order = containerOrder()
        val start = order.indexOf(headerKey)
        if (start < 0) return emptyList()
        val members = ArrayList<Item>()
        for (i in start + 1 until order.size) {
            val item = items.firstOrNull { it.key == order[i] } ?: continue
            if (item.isHeader) break
            members.add(item)
        }
        return members
    }

    fun addGroup(name: String) {
        val list = storedGroups()
        val key = "group_custom_" + System.currentTimeMillis()
        list.add(key to name)
        saveGroups(list)
        val view = makeHeaderView(name)
        container.addView(view)
        items.add(Item(key, view.id, 0, isHeader = true))
        saveOrderFromContainer()
        if (editMode) {
            collapseCard(view, items.last(), false)
            view.setOnLongClickListener { beginDrag(view); true }
        }
    }

    fun renameGroup(item: Item, name: String) {
        prefs.edit().putString(KEY_NAME_PREFIX + item.key, name).apply()
        val list = storedGroups()
        val index = list.indexOfFirst { it.first == item.key }
        if (index >= 0) {
            list[index] = item.key to name
            saveGroups(list)
        }
    }

    fun deleteGroup(item: Item) {
        // Only user-created groups can be removed; cards fall into the group above.
        if (!item.key.startsWith("group_custom_")) return
        saveGroups(storedGroups().filterNot { it.first == item.key })
        prefs.edit().remove(KEY_NAME_PREFIX + item.key).apply()
        viewFor(item)?.let { container.removeView(it) }
        items.removeAll { it.key == item.key }
        saveOrderFromContainer()
    }

    private fun promptRename(item: Item) {
        val input = EditText(context).apply {
            setText(groupName(item))
            inputType = InputType.TYPE_CLASS_TEXT
            setPadding(dp(24), dp(16), dp(24), dp(8))
        }
        val builder = AlertDialog.Builder(context)
            .setTitle(R.string.effect_group_rename)
            .setView(input)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                val name = input.text.toString().trim()
                if (name.isNotEmpty()) {
                    renameGroup(item, name)
                    (viewFor(item) as? TextView)?.text = name
                    editRows[item.key]?.let { row ->
                        ((row as LinearLayout).getChildAt(1) as? TextView)?.text = name
                    }
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
        if (item.key.startsWith("group_custom_")) {
            builder.setNeutralButton(R.string.effect_group_delete) { _, _ -> deleteGroup(item) }
        }
        builder.show()
    }

    // ------------------------------------------------------------------ apply

    /** Applies the saved order and visibility to the container. */
    fun applyLayout() {
        ensureGroupViews()
        val order = storedOrder()
        if (order == null) {
            // Untouched layout: keep the declared order as-is
            applyVisibility()
            return
        }
        val ordered = order.mapNotNull { key ->
            items.firstOrNull { it.key == key }?.let { item -> viewFor(item) }
        }
        if (ordered.isEmpty()) return

        val startIndex = ordered
            .map { container.indexOfChild(it) }
            .filter { it >= 0 }
            .minOrNull() ?: return

        // Detach from whatever currently holds each view, not just from the
        // root container, and do it again immediately before each add rather
        // than only in one pass up front - an add earlier in the loop can
        // re-parent a view that `ordered` names twice. Together this makes a
        // repeated applyLayout harmless.
        ordered.forEach { (it.parent as? ViewGroup)?.removeView(it) }
        var index = startIndex
        ordered.forEach { view ->
            (view.parent as? ViewGroup)?.removeView(view)
            container.addView(view, index.coerceAtMost(container.childCount))
            index++
        }

        applyVisibility()
    }

    private fun applyVisibility() {
        val hidden = HashSet(hiddenKeys())
        // A hidden group takes its cards with it
        items.filter { it.isHeader && hidden.contains(it.key) }.forEach { header ->
            membersOf(header.key).forEach { hidden.add(it.key) }
        }
        items.forEach { item ->
            viewFor(item)?.isVisible = editMode || !hidden.contains(item.key)
        }
    }

    private fun saveOrderFromContainer() {
        val keys = ArrayList<String>()
        for (i in 0 until container.childCount) {
            itemForView(container.getChildAt(i))?.let { keys.add(it.key) }
        }
        if (keys.isNotEmpty()) {
            prefs.edit().putString(KEY_ORDER, keys.joinToString(",")).apply()
        }
    }

    // -------------------------------------------------------------- edit mode

    fun toggleEditMode() = if (editMode) exitEditMode() else enterEditMode()

    fun enterEditMode() {
        if (editMode) return
        editMode = true
        applyVisibility()

        val hidden = hiddenKeys()
        items.forEach { item ->
            val view = viewFor(item) ?: return@forEach
            collapseCard(view, item, hidden.contains(item.key))
            view.setOnLongClickListener {
                beginDrag(view)
                true
            }
        }

        container.setOnDragListener(dragListener)
        showAddGroupButton()
        onEditModeChanged?.invoke(true)
    }

    /**
     * Re-read the layout from disk and re-apply it.
     *
     * Loading a preset replaces effect_layout.xml underneath a running app, but
     * [prefs] is the copy this process loaded at startup and keeps serving -
     * and the next drag or eye-toggle would write that stale copy straight back
     * over what the preset restored. Re-entering getSharedPreferences with
     * MODE_MULTI_PROCESS is what makes the platform notice the file changed;
     * the mode on the cached instance alone does nothing.
     *
     * Deliberately not a computed property: isHidden() is called once per card
     * on every keystroke of the effect search, and that would put a stat() per
     * card per character on the UI thread.
     */
    fun reload() {
        @Suppress("DEPRECATION")
        context.getSharedPreferences(PREFS, Context.MODE_MULTI_PROCESS)
        val live = storedGroups().map { it.first }.toSet()
        items.filter { it.isHeader && it.titleRes == 0 && it.key !in live }
            .forEach { stale ->
                viewFor(stale)?.let { container.removeView(it) }
                items.removeAll { it.key == stale.key }
            }
        applyLayout()
    }

    fun exitEditMode() {
        if (!editMode) return
        editMode = false

        items.forEach { item ->
            val view = viewFor(item) ?: return@forEach
            view.scaleX = 1f
            view.scaleY = 1f
            view.alpha = 1f
            view.setOnLongClickListener(null)
            view.isLongClickable = false
            expandCard(view, item)
        }

        container.setOnDragListener(null)
        addGroupButton?.let { container.removeView(it) }
        addGroupButton = null
        saveOrderFromContainer()
        applyVisibility()
        onEditModeChanged?.invoke(false)
    }

    /** Replaces the card's content with a compact one-line editing row. */
    private fun collapseCard(view: View, item: Item, isHidden: Boolean) {
        if (item.isHeader) {
            // Headers aren't containers - show the editing affordances inline
            (view as? TextView)?.let { header ->
                header.text = context.getString(R.string.effect_group_header_editing, groupName(item))
                header.setOnClickListener { promptRename(item) }
                header.alpha = if (isHidden) 0.45f else 1f
            }
            return
        }

        val group = view as? ViewGroup ?: return

        if (!item.isHeader) {
            container.findViewById<View>(item.viewId)?.isVisible = false
        } else {
            (view as? TextView)?.alpha = 0.4f
        }

        val row = LinearLayout(context).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(12), dp(10), dp(4), dp(10))
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }

        row.addView(ImageView(context).apply {
            setImageResource(R.drawable.ic_twotone_drag_handle_24dp)
            layoutParams = LinearLayout.LayoutParams(dp(28), dp(28))
        })

        row.addView(TextView(context).apply {
            text = context.getString(item.titleRes)
            setPadding(dp(12), 0, dp(8), 0)
            maxLines = 1
            alpha = if (isHidden) 0.45f else 1f
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        })

        val eye = ImageButton(context).apply {
            setImageResource(
                if (isHidden) R.drawable.ic_twotone_visibility_off_24dp
                else R.drawable.ic_twotone_visibility_24dp
            )
            background = null
            layoutParams = LinearLayout.LayoutParams(dp(44), dp(44))
        }
        eye.setOnClickListener {
            val hidden = hiddenKeys()
            val nowHidden = !hidden.contains(item.key)
            if (nowHidden) hidden.add(item.key) else hidden.remove(item.key)
            prefs.edit().putStringSet(KEY_HIDDEN, hidden).apply()
            eye.setImageResource(
                if (nowHidden) R.drawable.ic_twotone_visibility_off_24dp
                else R.drawable.ic_twotone_visibility_24dp
            )
            (row.getChildAt(1) as? TextView)?.alpha = if (nowHidden) 0.45f else 1f
        }
        if (item.isHeader) {
            val rename = ImageButton(context).apply {
                setImageResource(R.drawable.ic_twotone_edit_24dp)
                background = null
                layoutParams = LinearLayout.LayoutParams(dp(44), dp(44))
            }
            rename.setOnClickListener { promptRename(item) }
            row.addView(rename)
            if (item.key.startsWith("group_custom_")) {
                val delete = ImageButton(context).apply {
                    setImageResource(R.drawable.ic_twotone_delete_24dp)
                    background = null
                    layoutParams = LinearLayout.LayoutParams(dp(44), dp(44))
                    setOnClickListener {
                        AlertDialog.Builder(context)
                            .setTitle(R.string.effect_group_delete)
                            .setMessage(groupName(item))
                            .setPositiveButton(android.R.string.ok) { _, _ -> deleteGroup(item) }
                            .setNegativeButton(android.R.string.cancel, null)
                            .show()
                    }
                }
                row.addView(delete)
            }
        }
        row.addView(eye)

        group.addView(row)
        editRows[item.key] = row
    }

    private fun expandCard(view: View, item: Item) {
        editRows.remove(item.key)?.let { (view as? ViewGroup)?.removeView(it) }
        if (!item.isHeader) {
            container.findViewById<View>(item.viewId)?.isVisible = true
        } else {
            (view as? TextView)?.let {
                it.alpha = 1f
                it.text = groupName(item)
                it.setOnClickListener(null)
                it.isClickable = false
            }
        }
    }

    private fun beginDrag(view: View) {
        view.alpha = 0.65f
        view.animate().scaleX(1.04f).scaleY(1.04f).setDuration(120).start()
        ViewCompat.startDragAndDrop(view, null, View.DragShadowBuilder(view), view, 0)
    }

    private val dragListener = View.OnDragListener { _, event ->
        val dragged = event.localState as? View ?: return@OnDragListener false
        when (event.action) {
            DragEvent.ACTION_DRAG_LOCATION -> {
                val target = movableChildUnder(event.y, dragged)
                if (target != null) {
                    val to = container.indexOfChild(target)
                    val from = container.indexOfChild(dragged)
                    if (to >= 0 && from >= 0 && to != from) {
                        container.removeView(dragged)
                        container.addView(dragged, to)
                    }
                }
                true
            }
            DragEvent.ACTION_DROP, DragEvent.ACTION_DRAG_ENDED -> {
                dragged.alpha = 1f
                dragged.animate().scaleX(1f).scaleY(1f).setDuration(120).start()
                saveOrderFromContainer()
                true
            }
            else -> true
        }
    }

    private fun movableChildUnder(y: Float, exclude: View): View? {
        for (i in 0 until container.childCount) {
            val child = container.getChildAt(i)
            if (child === exclude || itemForView(child) == null) continue
            if (y >= child.top && y <= child.bottom) return child
        }
        return null
    }

    /** True when the card is currently hidden by the user. */
    fun isHidden(key: String) = hiddenKeys().contains(key)

    fun resetLayout() {
        prefs.edit().remove(KEY_ORDER).remove(KEY_HIDDEN).apply()
        applyLayout()
    }

    companion object {
        private const val TAG_HEADER = "effect_group_header"

        private const val PREFS = "effect_layout"
        private const val KEY_ORDER = "order"
        private const val KEY_HIDDEN = "hidden"
        private const val KEY_GROUPS = "groups"
        private const val KEY_NAME_PREFIX = "name_"
        private const val KEY_SCHEMA = "schema"
        private const val SCHEMA_VERSION = 2
    }
}
