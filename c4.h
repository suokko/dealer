#ifndef PNQ_CARD
#define PNQ_CARD

#include "dealer.h"

#define HAS_CARD2(s,r) (HAS_CARD(&gptr->curboard, seat, (card) MAKECARD(s,r)) > 0)

/* This macro is just an implementation detail - in c++ it would be
   an inline function. */


#ifdef __cplusplus

namespace DEFUN() {
int suit_quality(  int , int  ) ;
int quality (int, int);
int eval_cccc( int ) ;
int cccc (int);
}

#endif

#endif /* PNQ_CARD */

