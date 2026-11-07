#ifndef S1_DIAGNOSTIC_KEYS_H
#define S1_DIAGNOSTIC_KEYS_H
typedef enum { S1_DIAG_IGNORE, S1_DIAG_HELP, S1_DIAG_STATUS, S1_DIAG_BEGIN, S1_DIAG_FINISH } s1_diag_key_t;
static inline s1_diag_key_t s1_diagnostic_key(int c)
{
    switch(c) {
    case 'h': case '?': return S1_DIAG_HELP;
    case 'i': return S1_DIAG_STATUS;
    case 'c': return S1_DIAG_BEGIN;
    case 'f': return S1_DIAG_FINISH;
    default: return S1_DIAG_IGNORE;
    }
}
#endif
