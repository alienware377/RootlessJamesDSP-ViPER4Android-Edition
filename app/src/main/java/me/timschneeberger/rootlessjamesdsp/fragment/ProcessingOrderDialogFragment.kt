package me.timschneeberger.rootlessjamesdsp.fragment

import android.app.Dialog
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.fragment.app.DialogFragment
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import me.timschneeberger.rootlessjamesdsp.R
import me.timschneeberger.rootlessjamesdsp.utils.Constants
import me.timschneeberger.rootlessjamesdsp.utils.extensions.ContextExtensions.sendLocalBroadcast
import java.util.Collections

/**
 * Lets the user reorder the *audio* processing chain, as opposed to the visual
 * card order on the main screen. The output limiter is deliberately not listed:
 * it always runs last so it can catch anything the chain overshoots.
 */
class ProcessingOrderDialogFragment : DialogFragment() {

    /** Must stay in sync with enum JdspEffectId in jdsp_header.h */
    private data class ChainEffect(val id: Int, val titleRes: Int)

    private val allEffects = listOf(
        ChainEffect(0, R.string.v4a_tube_title),
        ChainEffect(1, R.string.compander_enable_v2),
        ChainEffect(2, R.string.pitchshift_enable),
        ChainEffect(3, R.string.fetcomp_enable),
        ChainEffect(4, R.string.diffsurround_enable),
        ChainEffect(5, R.string.bass_enable),
        ChainEffect(6, R.string.v4a_vdynbass_title),
        ChainEffect(7, R.string.viperbass_enable),
        ChainEffect(8, R.string.bassex_enable),
        ChainEffect(9, R.string.v4a_eq_title),
        ChainEffect(10, R.string.geq_enable),
        ChainEffect(11, R.string.convolver_enable),
        ChainEffect(12, R.string.v4a_ddc_title),
        ChainEffect(13, R.string.liveprog_enable),
        ChainEffect(14, R.string.liveprog_slot_2),
        ChainEffect(15, R.string.liveprog_slot_3),
        ChainEffect(16, R.string.liveprog_slot_4),
        ChainEffect(17, R.string.crossfeed_enable),
        ChainEffect(18, R.string.cure_enable),
        ChainEffect(19, R.string.stereowide_enable),
        ChainEffect(20, R.string.fieldsurround_enable),
        ChainEffect(21, R.string.v4a_hpsurround_title),
        ChainEffect(22, R.string.spectrumext_enable),
        ChainEffect(23, R.string.clarity_enable),
        ChainEffect(24, R.string.v4a_agc_title),
        ChainEffect(25, R.string.speakeropt_enable),
        ChainEffect(26, R.string.reverb_enable),
        ChainEffect(27, R.string.vreverb_enable),
        ChainEffect(28, R.string.echo_enable),
        ChainEffect(29, R.string.mbd_enable)
    )

    /** Chained Liveprog slots only appear once they actually hold a script. */
    private fun isSlotUnused(id: Int): Boolean {
        val slot = when (id) {
            14 -> 2
            15 -> 3
            16 -> 4
            else -> return false
        }
        val prefName = "dsp_liveprog$slot"
        val keyId = resources.getIdentifier(
            "key_liveprog${slot}_file", "string", requireContext().packageName
        )
        if (keyId == 0) return false
        val value = requireContext()
            .getSharedPreferences(prefName, android.content.Context.MODE_PRIVATE)
            .getString(getString(keyId), "")
        return value.isNullOrBlank()
    }

    private lateinit var order: MutableList<ChainEffect>

    private fun prefs() = requireContext()
        .getSharedPreferences(Constants.PREF_CHAIN_ORDER, Context.MODE_PRIVATE)

    private fun loadOrder(): MutableList<ChainEffect> {
        val saved = prefs().getString(Constants.KEY_CHAIN_ORDER, null)
            ?.split(",")
            ?.mapNotNull { it.trim().toIntOrNull() }
            ?: return allEffects.filterNot { isSlotUnused(it.id) }.toMutableList()
        val result = ArrayList<ChainEffect>()
        saved.forEach { id -> allEffects.firstOrNull { it.id == id }?.let(result::add) }
        // Anything missing (e.g. added by an update) keeps its default position
        allEffects.forEachIndexed { index, effect ->
            if (result.none { it.id == effect.id }) {
                result.add(index.coerceAtMost(result.size), effect)
            }
        }
        result.removeAll { isSlotUnused(it.id) }
        return result
    }

    private fun saveOrder() {
        // Hidden (script-less) slots are appended so the engine still gets a
        // complete chain; with no script loaded their position is irrelevant.
        val ids = order.map { it.id }.toMutableList()
        allEffects.forEach { effect -> if (!ids.contains(effect.id)) ids.add(effect.id) }
        prefs().edit()
            .putString(Constants.KEY_CHAIN_ORDER, ids.joinToString(","))
            .apply()
    }

    private inner class Holder(val root: LinearLayout) : RecyclerView.ViewHolder(root) {
        val title: TextView = root.getChildAt(1) as TextView
    }

    private inner class Adapter : RecyclerView.Adapter<Holder>() {
        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder {
            val density = parent.resources.displayMetrics.density
            fun dp(value: Int) = (value * density).toInt()

            val row = LinearLayout(parent.context).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = android.view.Gravity.CENTER_VERTICAL
                setPadding(dp(16), dp(12), dp(16), dp(12))
                layoutParams = RecyclerView.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                )
            }
            row.addView(ImageView(parent.context).apply {
                setImageResource(R.drawable.ic_twotone_drag_handle_24dp)
                layoutParams = LinearLayout.LayoutParams(dp(28), dp(28))
            })
            row.addView(TextView(parent.context).apply {
                setPadding(dp(16), 0, 0, 0)
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                )
            })
            return Holder(row)
        }

        override fun onBindViewHolder(holder: Holder, position: Int) {
            holder.title.text = getString(order[position].titleRes)
        }

        override fun getItemCount() = order.size
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        order = loadOrder()

        val context = requireContext()
        val density = context.resources.displayMetrics.density
        val container = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
        }

        container.addView(TextView(context).apply {
            setText(R.string.chain_order_explainer)
            setPadding(
                (24 * density).toInt(), (8 * density).toInt(),
                (24 * density).toInt(), (8 * density).toInt()
            )
            setTextColor(
                com.google.android.material.color.MaterialColors.getColor(
                    this, com.google.android.material.R.attr.colorOnSurfaceVariant
                )
            )
        })

        val recycler = RecyclerView(context).apply {
            layoutManager = LinearLayoutManager(context)
            adapter = Adapter()
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                (420 * density).toInt()
            )
        }
        container.addView(recycler)

        val touchHelper = ItemTouchHelper(object : ItemTouchHelper.SimpleCallback(
            ItemTouchHelper.UP or ItemTouchHelper.DOWN, 0
        ) {
            override fun onMove(
                recyclerView: RecyclerView,
                viewHolder: RecyclerView.ViewHolder,
                target: RecyclerView.ViewHolder
            ): Boolean {
                Collections.swap(order, viewHolder.bindingAdapterPosition, target.bindingAdapterPosition)
                recyclerView.adapter?.notifyItemMoved(
                    viewHolder.bindingAdapterPosition, target.bindingAdapterPosition
                )
                return true
            }

            override fun onSwiped(viewHolder: RecyclerView.ViewHolder, direction: Int) = Unit

            override fun isLongPressDragEnabled() = true

            override fun onSelectedChanged(viewHolder: RecyclerView.ViewHolder?, actionState: Int) {
                super.onSelectedChanged(viewHolder, actionState)
                if (actionState == ItemTouchHelper.ACTION_STATE_DRAG) {
                    viewHolder?.itemView?.animate()?.scaleX(1.03f)?.scaleY(1.03f)
                        ?.setDuration(120)?.start()
                }
            }

            override fun clearView(recyclerView: RecyclerView, viewHolder: RecyclerView.ViewHolder) {
                super.clearView(recyclerView, viewHolder)
                viewHolder.itemView.animate().scaleX(1f).scaleY(1f).setDuration(120).start()
                saveOrder()
            }
        })
        touchHelper.attachToRecyclerView(recycler)

        return MaterialAlertDialogBuilder(context)
            .setTitle(R.string.chain_order_title)
            .setView(container)
            .setPositiveButton(android.R.string.ok) { _, _ -> saveOrder() }
            .setNeutralButton(R.string.chain_order_reset) { _, _ ->
                prefs().edit().remove(Constants.KEY_CHAIN_ORDER).apply()
                requireContext().sendLocalBroadcast(Intent(Constants.ACTION_PREFERENCES_UPDATED))
            }
            .create()
    }

    companion object {
        fun newInstance() = ProcessingOrderDialogFragment()
    }
}
