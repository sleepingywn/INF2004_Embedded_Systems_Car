/* Route decoder (ported unchanged from the bench barcode_bars.h, W3) for one-character Code 39 (A-Z, 0-9, - . space).
 * Every one of these characters has exactly 2 wide bars (of 5) and 1 wide
 * space (of 4), so bars and spaces are classified SEPARATELY instead of
 * ranking all nine elements together:
 *  - start/stop '*': checked on bars only. They only set the direction and
 *    confirm alignment; on the track the white spaces were the noisy part.
 *  - data character: 2 widest bars + widest space, which together identify
 *    the character. Letters sharing the same bars (A, K, U, 1) differ only in
 *    the wide space, so the widest space must clearly stand out (>= 1.25x the
 *    next) or nothing is decoded: no letter rather than a wrong letter. */
#ifndef MOTION_ROUTE_DECODE_H
#define MOTION_ROUTE_DECODE_H
#include <stdbool.h>
#include "../subsystem3/ir_types.h"
static const struct { char code; const char *p; } code39_table[] = {
    {'0',"NNNWWNWNN"},{'1',"WNNWNNNNW"},{'2',"NNWWNNNNW"},{'3',"WNWWNNNNN"},
    {'4',"NNNWWNNNW"},{'5',"WNNWWNNNN"},{'6',"NNWWWNNNN"},{'7',"NNNWNNWNW"},
    {'8',"WNNWNNWNN"},{'9',"NNWWNNWNN"},{'A',"WNNNNWNNW"},{'B',"NNWNNWNNW"},
    {'C',"WNWNNWNNN"},{'D',"NNNNWWNNW"},{'E',"WNNNWWNNN"},{'F',"NNWNWWNNN"},
    {'G',"NNNNNWWNW"},{'H',"WNNNNWWNN"},{'I',"NNWNNWWNN"},{'J',"NNNNWWWNN"},
    {'K',"WNNNNNNWW"},{'L',"NNWNNNNWW"},{'M',"WNWNNNNWN"},{'N',"NNNNWNNWW"},
    {'O',"WNNNWNNWN"},{'P',"NNWNWNNWN"},{'Q',"NNNNNNWWW"},{'R',"WNNNNNWWN"},
    {'S',"NNWNNNWWN"},{'T',"NNNNWNWWN"},{'U',"WWNNNNNNW"},{'V',"NWWNNNNNW"},
    {'W',"WWWNNNNNN"},{'X',"NWNNWNNNW"},{'Y',"WWNNWNNNN"},{'Z',"NWWNWNNNN"},
    {'-',"NWNNNNWNW"},{'.',"WWNNNNWNN"},{' ',"NWWNNNWNN"},
};
#define CODE39_STAR "NWNNWNWNN"
/* Bit 8 = first element ... bit 0 = ninth. */
static inline uint16_t code39_mask(const char *p)
{
    uint16_t m = 0;
    for (unsigned i = 0; i < 9; ++i) if (p[i] == 'W') m |= (uint16_t)(1u << (8 - i));
    return m;
}
static inline uint16_t code39_reverse(uint16_t m)
{
    uint16_t r = 0;
    for (unsigned i = 0; i < 9; ++i) if (m & (1u << i)) r |= (uint16_t)(1u << (8 - i));
    return r;
}
/* Mark the `wide` largest of n elements w[first], w[first+2], ... in mask.
 * Fails unless the smallest wide one is >= num/den x the largest narrow one. */
static inline bool code39_pick(const bc_width_t *w, unsigned first, unsigned n, unsigned wide,
                        unsigned num, unsigned den, uint16_t *mask)
{
    unsigned order[5];
    for (unsigned i = 0; i < n; ++i) {
        unsigned j = i;
        while (j && w[first + 2 * order[j - 1]] < w[first + 2 * i]) { order[j] = order[j - 1]; --j; }
        order[j] = i;
    }
    uint32_t last_wide = w[first + 2 * order[wide - 1]], first_narrow = w[first + 2 * order[wide]];
    if (first_narrow == 0u || last_wide * den < first_narrow * num) return false;
    for (unsigned i = 0; i < wide; ++i) *mask |= (uint16_t)(1u << (8 - (first + 2 * order[i]) % 10));
    return true;
}
#define BARS_RATIO_NUM 3u   /* wide bars >= 1.5x narrow bars */
#define BARS_RATIO_DEN 2u
#define SPACE_RATIO_NUM 5u  /* wide space >= 1.25x the next space */
#define SPACE_RATIO_DEN 4u
#define CODE39_BAR_BITS 0x155u /* elements 1,3,5,7,9 */
/* Returns the decoded character, or 0. Either scan direction. */
static inline char route_code39_decode(const bc_msg_t *m)
{
    if (m->hdr.count != 29u || m->hdr.overflow) return 0;
    uint16_t g1 = 0, g3 = 0, data = 0;
    if (!code39_pick(m->widths, 0, 5, 2, BARS_RATIO_NUM, BARS_RATIO_DEN, &g1) ||
        !code39_pick(m->widths, 20, 5, 2, BARS_RATIO_NUM, BARS_RATIO_DEN, &g3) ||
        !code39_pick(m->widths, 10, 5, 2, BARS_RATIO_NUM, BARS_RATIO_DEN, &data) ||
        !code39_pick(m->widths, 11, 4, 1, SPACE_RATIO_NUM, SPACE_RATIO_DEN, &data)) return 0;
    const uint16_t star = code39_mask(CODE39_STAR) & CODE39_BAR_BITS;
    bool forward = g1 == star && g3 == star;
    bool backward = g1 == code39_reverse(star) && g3 == code39_reverse(star);
    if (!forward && !backward) return 0;
    if (backward) data = code39_reverse(data);
    for (unsigned i = 0; i < sizeof code39_table / sizeof code39_table[0]; ++i)
        if (code39_mask(code39_table[i].p) == data) return code39_table[i].code;
    return 0;
}
#endif
