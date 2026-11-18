/* barcode_level.h - barcode black/white from the raw ADC (LINE_ROUTE only).
 *
 * Car measurement 18 Nov 2026 (barcode sensor, raw 12-bit ADC, car still):
 *   white 177, wide black bar 1309, narrow black bar 307, thin white gap 177.
 * Narrow bars barely rise above white, wide bars are very strong. Ken's fixed
 * cutoffs (black >= 240, white <= 200) catch the narrow bars, but at speed a
 * thin gap between two wide bars only dips to a few hundred, never <= 200, so
 * the gap was lost and bars merged (scan "B903 ..." = 5 elements in one).
 *
 * Rule (fixed cutoffs kept, plus a relative one):
 *   - bar starts : raw >= BC_LVL_BLACK_MIN and raw >= lowest white so far + BC_LVL_RISE
 *   - bar ends   : raw <= BC_LVL_WHITE_MAX, or raw has fallen half way from the
 *                  bar's peak back to the white level (and >= BC_LVL_RISE below
 *                  the peak).  Narrow bar (peak ~307): ends below ~242.
 *                  Wide bar (peak ~1300): ends below ~740, so blurred gaps count.
 * The white level used for "half way" is the gap before the bar, capped at
 * BC_LVL_REF_CAP, so a bar after a blurred gap is still judged against white.
 */
#ifndef BARCODE_LEVEL_H
#define BARCODE_LEVEL_H
#include <stdbool.h>
#include <stdint.h>

#define BC_LVL_WHITE_MAX 200u   /* Ken's fixed cutoffs, still applied */
#define BC_LVL_BLACK_MIN 240u
#define BC_LVL_RISE      60u    /* min step between a gap and a bar   */
#define BC_LVL_REF_CAP   300u   /* white reference never above this   */
#ifndef BC_LVL_HOLD
#define BC_LVL_HOLD      8u     /* half-way dip must last 8 ms: track A had a 6 ms flicker
                                   inside a narrow bar; real gaps were >= 14 ms */
#endif
#ifndef BC_LVL_END_NUM          /* bar ends below ref + (peak - ref) x NUM/DEN */
#define BC_LVL_END_NUM   1u
#define BC_LVL_END_DEN   2u
#endif

typedef struct {
    uint16_t valley;   /* lowest raw in the current white run */
    uint16_t peak;     /* highest raw in the current bar      */
    uint16_t ref;      /* white level for the current bar     */
    uint16_t below;    /* samples in a row under the half-way level */
} bc_level_t;

static inline void bc_level_init(bc_level_t *s)
{
    s->valley = BC_LVL_WHITE_MAX;
    s->peak = 0u;
    s->ref = BC_LVL_WHITE_MAX;
    s->below = 0u;
}

/* is_bar: the capture's current colour (true = in a bar). Returns black. */
static inline bool bc_level_black(bc_level_t *s, bool is_bar, uint16_t raw)
{
    if (!is_bar) {
        if (raw < s->valley) s->valley = raw;
        if (raw >= BC_LVL_BLACK_MIN && raw >= (uint32_t)s->valley + BC_LVL_RISE) {
            s->ref = s->valley < BC_LVL_REF_CAP ? s->valley : BC_LVL_REF_CAP;
            s->peak = raw;
            s->below = 0u;
            return true;
        }
        return false;
    }
    if (raw > s->peak) s->peak = raw;
    {
        uint32_t half = s->peak > s->ref ? s->ref + (uint32_t)(s->peak - s->ref) * BC_LVL_END_NUM / BC_LVL_END_DEN : s->ref;
        bool dip = raw <= half && (uint32_t)raw + BC_LVL_RISE <= s->peak;
        s->below = dip ? (uint16_t)(s->below + 1u) : 0u;
        bool white = raw <= BC_LVL_WHITE_MAX || s->below >= BC_LVL_HOLD;
        if (white) {
            s->valley = raw;
            return false;
        }
    }
    return true;
}
#endif
