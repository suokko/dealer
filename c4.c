/*

This code has been submitted by Danil Suits <DSuits@silknet.com>, Mar 10, 1999.

It implements the cccc() and quality() functions.  Both quality and cccc
use the algorithms described in _The Bridge World_, October 1982, with the
single exception that the values are multiplied by 100 (so that we can use
integers for them).  Thus, a minimum opening bid is about 1200, rather
than 12.00 as expressed in the text.  

In the original algorithm, everything was done with fractions.  Floating
point rounding being what it is, I've decided to implement this instead as
integer math, until the last step. 

As it happens, it is currently more convenient to use integers for the
return value from these functions as well So for the moment, Rescale is
basically a no-op. 

*/

#include <assert.h>
#include "tree.h"
#include "dealer.h"
#include "c4.h"

#define C4_TYPE int
C4_TYPE Rescale (int nValue) {
  return nValue;
}

C4_TYPE cccc (int seat) {
  return Rescale (eval_cccc (seat));
}

C4_TYPE quality (int seat, int suit) {
  return Rescale (suit_quality (seat, suit));
}
#undef C4_TYPE

int eval_cccc (int seat) {
  int eval = 0;
  int ShapePoints = 0;

  /* For each suit.... */
  int suit;

  for (suit = SUIT_CLUB; suit <= SUIT_SPADE; ++suit) {
    int Length = hs[seat].hs_length[suit];

    int HasAce   = HAS_CARD2 (suit, RK_ACE);
    int HasKing  = HAS_CARD2 (suit, RK_KING);
    int HasQueen = HAS_CARD2 (suit, RK_QUEEN);
    int HasJack  = HAS_CARD2 (suit, RK_JACK);
    int HasTen   = HAS_CARD2 (suit, RK_TEN);
    int HasNine  = HAS_CARD2 (suit, RK_NINE);

    int HigherHonors = 0;

    if (Length < 3) {
      ShapePoints += (3 - Length) * 100;
    }

    if (HasAce) {
      eval += 300;
      HigherHonors++;
    }

      if (HasKing) {
        eval += 200;
        if (Length == 1)
          eval -= 150;
        HigherHonors++;
      }

      if (HasQueen) {
        eval += 100;

        if (Length == 1)
          eval -= 75;
        if (Length == 2)
          eval -= 25;

        if (HigherHonors == 0)
          eval -= 25;

        HigherHonors++;
      }

      if (HasJack) {
        if (HigherHonors == 2)
          eval += 50;
        if (HigherHonors == 1)
          eval += 25;
        HigherHonors++;
      }

      if (HasTen) {
        if (HigherHonors == 2)
          eval += 25;
        if ((HigherHonors == 1) && HasNine)
          eval += 25;
      }

      eval += suit_quality (seat, suit);

    }                        /* end for (suit;...) */

  if (ShapePoints == 0)
    eval -= 50;
  else
    eval += ShapePoints - 100;

  assert ((eval % 5) == 0);

  return Rescale (eval);
}

int suit_quality (int seat, int suit) {
  int Quality = 0;

  int Length = hs[seat].hs_length[suit];

  int HasAce   = HAS_CARD2 (suit, RK_ACE);
  int HasKing  = HAS_CARD2 (suit, RK_KING);
  int HasQueen = HAS_CARD2 (suit, RK_QUEEN);
  int HasJack  = HAS_CARD2 (suit, RK_JACK);
  int HasTen   = HAS_CARD2 (suit, RK_TEN);
  int HasNine  = HAS_CARD2 (suit, RK_NINE);
  int HasEight = HAS_CARD2 (suit, RK_EIGHT);

  int HigherHonors = 0;

  int SuitFactor = Length * 10;

  /*ACE*/
  if (HasAce) {
      Quality += 4 * SuitFactor;
      HigherHonors++;
    }

  /*KING*/ 
  if (HasKing) {
      Quality += 3 * SuitFactor;
      HigherHonors++;
    }

  /*QUEEN*/
  if (HasQueen) {
      Quality += 2 * SuitFactor;
      HigherHonors++;
    }

  /*JACK*/
  if (HasJack) {
      Quality += 1 * SuitFactor;
      HigherHonors++;
    }

  if (Length > 6) {
      int ReplaceCount = 3;
      if (HasQueen)
      ReplaceCount -= 2;
      if (HasJack)
      ReplaceCount -= 1;

      if (ReplaceCount > (Length - 6))
      ReplaceCount = Length - 6;

      Quality += ReplaceCount * SuitFactor;
    }
  else                        /* this.Length <= 6 */
    {
      if (HasTen) {
        if ((HigherHonors > 1) || HasJack)
          Quality += SuitFactor;
        else
          Quality += SuitFactor / 2;
      }

      if (HasNine) {
        if ((HigherHonors == 2) || HasTen || HasEight)
          Quality += SuitFactor / 2;
      }
    }

  assert ((Quality % 5) == 0);

  return Quality;
}
