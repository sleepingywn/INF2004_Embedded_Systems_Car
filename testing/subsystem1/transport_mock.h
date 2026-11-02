#ifndef TRANSPORT_MOCK_H
#define TRANSPORT_MOCK_H
#include <stdint.h>
typedef int ID;
typedef struct {unsigned mtxatr;} T_CMTX;
#define TA_INHERIT 2
#define TMO_POL 0
#define E_OK 0
int tk_cre_mtx(const T_CMTX *);
int tk_loc_mtx(ID, int);
int tk_unl_mtx(ID);
#endif
