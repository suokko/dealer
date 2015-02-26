#ifndef PNQ_CARD
#define PNQ_CARD

#include "dealer.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define HAS_CARD2(s,r) (HAS_CARD(&gptr->curboard, seat, (card) MAKECARD(s,r)) > 0)

/* This macro is just an implementation detail - in c++ it would be
   an inline function. */

#define suit_quality DEFUN(suit_quality)
#define quality DEFUN(quality)
#define eval_cccc DEFUN(eval_cccc)
#define cccc DEFUN(cccc)

int suit_quality(  int , int  ) ;
int quality (int, int);
int eval_cccc( int ) ;
int cccc (int);

#ifdef __cplusplus
} /* -- extern "C" */
#endif

#endif /* PNQ_CARD */

