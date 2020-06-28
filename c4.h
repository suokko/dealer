#ifndef PNQ_CARD
#define PNQ_CARD

#include "entry.h"

// Namespace for different compiler optimizations
namespace DEFUN() {
/**
 * Calculate suit quality using algorithm published in The Bridge World,
 * October 1982.
 *
 * @param seat The compass direction of player
 * @param suit The suit to calculate quality
 * @return The quality multiplied by 100
 */
int quality(int seat, int suit) ;
/**
 * Calculate suit cccc using algorithm published in The Bridge World,
 * October 1982.
 *
 * @param seat The compass direction of player
 * @param suit The suit to calculate quality
 * @return The cccc multiplied by 100
 */
int cccc(int seat);
}

#endif /* PNQ_CARD */

