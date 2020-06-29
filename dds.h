
#pragma once

#include "card.h"

union board;

/**
 * Do double dummy analyze returning number of tricks declarer gets if defenders
 * select the best opening lead.
 *
 * @param d Complete deal to be checked with double dummy
 * @param declarer The compass direction of declarer
 * @return Number of tricks declarer makes based on double dummy analyze
 */
extern int (*solve)(const union board *d, int declarer, int contract);
/**
 * Do double dummy analyze and store number of tricks declarer makes for each
 * card in res array.
 *
 * @param d Complete deal to be checked with double dummy
 * @param declarer The compass direction of declarer
 * @param cards Cards in the hand which has opening lead
 * @param res Array for double dummy results
 */
extern void (*solveLead)(const union board *d, int declarer, int contract, hand cards, char res[13]);
