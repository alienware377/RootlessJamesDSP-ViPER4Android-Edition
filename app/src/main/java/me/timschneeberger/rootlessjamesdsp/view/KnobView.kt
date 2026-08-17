package me.timschneeberger.rootlessjamesdsp.view

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.util.AttributeSet
import android.util.TypedValue
import android.view.MotionEvent
import android.view.View
import androidx.core.content.withStyledAttributes
import me.timschneeberger.rootlessjamesdsp.R
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.exp
import kotlin.math.ln
import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.math.sin

/**
 * A rotary control, like the knobs on a hardware EQ.
 *
 * Drag up/down to change the value. Frequency-style knobs can use a
 * logarithmic scale so the low end isn't crammed into a few degrees.
 */
class KnobView(context: Context, attrs: AttributeSet?) : View(context, attrs) {

    // Declared before init, which applies the paints
    private var accentOverride: Int? = null

    private val trackPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
    }
    private val valuePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
    }
    private val indicatorPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeCap = Paint.Cap.ROUND
    }
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
    }
    private val valueTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        typeface = Typeface.DEFAULT_BOLD
    }

    private val arcRect = RectF()

    var minValue = 0f
    var maxValue = 100f
    var precision = 1
    var unit: String = ""
    var label: String = ""
        set(newValue) {
            if (field != newValue) {
                field = newValue
                invalidate()
            }
        }
    var logScale = false

    private var listener: (() -> Unit)? = null
    private var lastY = 0f
    private var dragging = false

    var value: Float = 0f
        set(newValue) {
            val clamped = newValue.coerceIn(minValue, maxValue)
            if (field != clamped) {
                field = clamped
                invalidate()
            }
        }

    init {
        val density = context.resources.displayMetrics.density
        trackPaint.strokeWidth = 6f * density
        valuePaint.strokeWidth = 6f * density
        indicatorPaint.strokeWidth = 3f * density

        labelPaint.textSize = TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_SP, 11f, context.resources.displayMetrics
        )
        valueTextPaint.textSize = TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_SP, 14f, context.resources.displayMetrics
        )

        context.withStyledAttributes(attrs, R.styleable.KnobView) {
            minValue = getFloat(R.styleable.KnobView_knobMin, 0f)
            maxValue = getFloat(R.styleable.KnobView_knobMax, 100f)
            precision = getInt(R.styleable.KnobView_knobPrecision, 1)
            unit = getString(R.styleable.KnobView_knobUnit) ?: ""
            label = getString(R.styleable.KnobView_knobLabel) ?: ""
            logScale = getBoolean(R.styleable.KnobView_knobLogScale, false)
            value = getFloat(R.styleable.KnobView_knobValue, minValue)
        }

        applyThemeColors()
        isClickable = true
    }

    /** Overrides the arc/indicator colour; used by hand-set themes. */
    fun setAccentColor(color: Int) {
        accentOverride = color
        applyThemeColors()
        invalidate()
    }

    private fun applyThemeColors() {
        val accent = accentOverride ?: themeColor(androidx.appcompat.R.attr.colorAccent)
        val onSurface = themeColor(com.google.android.material.R.attr.colorOnSurface)
        val outline = themeColor(com.google.android.material.R.attr.colorOutline)
        valuePaint.color = accent
        indicatorPaint.color = accent
        trackPaint.color = outline
        trackPaint.alpha = 110
        labelPaint.color = onSurface
        labelPaint.alpha = 170
        valueTextPaint.color = onSurface
    }

    private fun themeColor(attr: Int): Int {
        if (isInEditMode) return 0xFF888888.toInt()
        var color = 0
        context.withStyledAttributes(TypedValue().data, intArrayOf(attr)) {
            color = getColor(0, 0xFF888888.toInt())
        }
        return color
    }

    fun setOnValueChangedListener(callback: () -> Unit) {
        listener = callback
    }

    /** Kept for parity with the text inputs this replaced; a knob can't be invalid. */
    fun isCurrentValueValid() = true

    /** 0..1 position of the current value, honouring the log scale. */
    private fun fraction(): Float {
        if (maxValue <= minValue) return 0f
        return if (logScale && minValue > 0f) {
            ((ln(value.toDouble()) - ln(minValue.toDouble())) /
                    (ln(maxValue.toDouble()) - ln(minValue.toDouble()))).toFloat()
        } else {
            (value - minValue) / (maxValue - minValue)
        }.coerceIn(0f, 1f)
    }

    private fun valueForFraction(f: Float): Float {
        val clamped = f.coerceIn(0f, 1f)
        return if (logScale && minValue > 0f) {
            exp(
                ln(minValue.toDouble()) +
                        clamped * (ln(maxValue.toDouble()) - ln(minValue.toDouble()))
            ).toFloat()
        } else {
            minValue + clamped * (maxValue - minValue)
        }
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val desired = (92 * context.resources.displayMetrics.density).toInt()
        setMeasuredDimension(
            resolveSize(desired, widthMeasureSpec),
            resolveSize(desired, heightMeasureSpec)
        )
    }

    override fun onDraw(canvas: Canvas) {
        val density = context.resources.displayMetrics.density
        val padding = 10f * density
        val textRoom = 26f * density
        val size = min(width.toFloat(), height - textRoom) - padding * 2
        val cx = width / 2f
        val cy = padding + size / 2f
        val radius = size / 2f

        arcRect.set(cx - radius, cy - radius, cx + radius, cy + radius)

        // 270 degree sweep, opening at the bottom
        val startAngle = 135f
        val sweep = 270f
        canvas.drawArc(arcRect, startAngle, sweep, false, trackPaint)

        val f = fraction()
        canvas.drawArc(arcRect, startAngle, sweep * f, false, valuePaint)

        // pointer
        val angle = Math.toRadians((startAngle + sweep * f).toDouble())
        val inner = radius * 0.35f
        val outer = radius * 0.78f
        canvas.drawLine(
            cx + (cos(angle) * inner).toFloat(),
            cy + (sin(angle) * inner).toFloat(),
            cx + (cos(angle) * outer).toFloat(),
            cy + (sin(angle) * outer).toFloat(),
            indicatorPaint
        )

        // Text scales with the knob so it stays legible and inside the dial at
        // any size the layout gives us.
        valueTextPaint.textSize = (size * 0.27f).coerceIn(9f * density, 15f * density)
        var valueText = formatValue()
        while (valueTextPaint.measureText(valueText) > size * 0.92f &&
               valueTextPaint.textSize > 7f * density) {
            valueTextPaint.textSize -= 1f
        }
        canvas.drawText(valueText, cx, cy + radius * 0.15f, valueTextPaint)

        labelPaint.textSize = (size * 0.19f).coerceIn(8f * density, 11f * density)
        var labelText = label
        while (labelPaint.measureText(labelText) > width - 2f * density &&
               labelPaint.textSize > 6.5f * density) {
            labelPaint.textSize -= 0.5f
        }
        canvas.drawText(labelText, cx, height - 5f * density, labelPaint)
    }

    private fun formatValue(): String {
        val text = when {
            precision <= 0 -> value.roundToInt().toString()
            else -> String.format("%.${precision}f", value)
        }
        return if (unit.isEmpty()) text else "$text$unit"
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        // A custom View that consumes touches itself never reaches
        // View.onTouchEvent, which is where Android's disabled early-out lives.
        // Without this, a dimmed knob still spins under the finger, still
        // writes its preference and still re-syncs the engine while doing
        // nothing audible - which turns the one honest signal these panels
        // have, dimming what cannot currently work, back into the complaint it
        // exists to prevent.
        if (!isEnabled) return false
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastY = event.y
                dragging = true
                parent?.requestDisallowInterceptTouchEvent(true)
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                if (!dragging) return false
                val delta = lastY - event.y
                if (abs(delta) < 0.5f) return true
                lastY = event.y
                // A full sweep takes roughly 260dp of travel, so it stays controllable
                val travel = 260f * context.resources.displayMetrics.density
                value = valueForFraction(fraction() + delta / travel)
                listener?.invoke()
                return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                dragging = false
                parent?.requestDisallowInterceptTouchEvent(false)
                performClick()
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    override fun performClick(): Boolean {
        super.performClick()
        return true
    }
}
