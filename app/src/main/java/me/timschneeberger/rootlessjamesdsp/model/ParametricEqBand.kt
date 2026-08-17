package me.timschneeberger.rootlessjamesdsp.model

import java.io.Serializable
import java.util.*

enum class ParametricEqFilterType(val code: Int, val apoLabel: String, val displayLabel: String) {
    PEAKING(0, "PK", "PK"),
    LOW_SHELF(1, "LSC", "LS"),
    HIGH_SHELF(2, "HSC", "HS"),
    // Cutoffs. EqualizerAPO writes these as LPQ/HPQ for the Q-specified form,
    // which is the one that round-trips with a biquad; the plain LP/HP labels
    // are read as well since exports in the wild use both.
    LOW_PASS(3, "LPQ", "LP"),
    HIGH_PASS(4, "HPQ", "HP");

    companion object {
        fun fromCode(code: Int) = entries.firstOrNull { it.code == code } ?: PEAKING
        fun fromApoLabel(label: String) = when (label.uppercase()) {
            "PK" -> PEAKING
            "LSC", "LS" -> LOW_SHELF
            "HSC", "HS" -> HIGH_SHELF
            "LPQ", "LP" -> LOW_PASS
            "HPQ", "HP" -> HIGH_PASS
            else -> null
        }
    }
}

/**
 * A parametric EQ band definition.
 *
 * [uuid] is excluded from equals/hashCode so that two bands with the
 * same audio parameters compare as equal regardless of identity.
 */
class ParametricEqBand(
    var frequency: Double,
    var gain: Double,
    var q: Double,
    var filterType: ParametricEqFilterType = ParametricEqFilterType.PEAKING,
    val uuid: UUID = UUID.randomUUID()
) : Serializable {

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is ParametricEqBand) return false
        return frequency == other.frequency &&
                gain == other.gain &&
                q == other.q &&
                filterType == other.filterType
    }

    override fun hashCode(): Int {
        var result = frequency.hashCode()
        result = 31 * result + gain.hashCode()
        result = 31 * result + q.hashCode()
        result = 31 * result + filterType.hashCode()
        return result
    }

    override fun toString(): String =
        "ParametricEqBand(frequency=$frequency, gain=$gain, q=$q, filterType=$filterType, uuid=$uuid)"
}
