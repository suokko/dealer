#ifndef PNQ_CARD
#define PNQ_CARD

#include "entry.h"

#define HAS_CARD2(s,r) (HAS_CARD(&gptr->curboard, seat, (card) MAKECARD(s,r)) > 0)

/* This macro is just an implementation detail - in c++ it would be
   an inline function. */


namespace DEFUN() {
int suit_quality(  int , int  ) ;
int quality (int, int);
int eval_cccc( int ) ;
int cccc (int);
}

#endif /* PNQ_CARD */

