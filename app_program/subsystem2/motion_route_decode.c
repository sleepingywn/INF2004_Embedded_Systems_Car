#include "motion_route_decode.h"

const code39_symbol_t code39_table[39] = {
    {'0', "NNNWWNWNN"}, {'1', "WNNWNNNNW"}, {'2', "NNWWNNNNW"},
    {'3', "WNWWNNNNN"}, {'4', "NNNWWNNNW"}, {'5', "WNNWWNNNN"},
    {'6', "NNWWWNNNN"}, {'7', "NNNWNNWNW"}, {'8', "WNNWNNWNN"},
    {'9', "NNWWNNWNN"}, {'A', "WNNNNWNNW"}, {'B', "NNWNNWNNW"},
    {'C', "WNWNNWNNN"}, {'D', "NNNNWWNNW"}, {'E', "WNNNWWNNN"},
    {'F', "NNWNWWNNN"}, {'G', "NNNNNWWNW"}, {'H', "WNNNNWWNN"},
    {'I', "NNWNNWWNN"}, {'J', "NNNNWWWNN"}, {'K', "WNNNNNNWW"},
    {'L', "NNWNNNNWW"}, {'M', "WNWNNNNWN"}, {'N', "NNNNWNNWW"},
    {'O', "WNNNWNNWN"}, {'P', "NNWNWNNWN"}, {'Q', "NNNNNNWWW"},
    {'R', "WNNNNNWWN"}, {'S', "NNWNNNWWN"}, {'T', "NNNNWNWWN"},
    {'U', "WWNNNNNNW"}, {'V', "NWWNNNNNW"}, {'W', "WWWNNNNNN"},
    {'X', "NWNNWNNNW"}, {'Y', "WWNNWNNNN"}, {'Z', "NWWNWNNNN"},
    {'-', "NWNNNNWNW"}, {'.', "WWNNNNWNN"}, {' ', "NWWNNNWNN"},
};
