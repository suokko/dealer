#include "pointcount.h"
#include "card.h"

#include <malloc.h>

/**
 * Preset pointcount tables which script can modify
 */
int tblPointcount [idxEnd][13] =
{
	/* 2  3  4  5  6  7  8  9  T  J  Q  K  A */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2}, /* controls */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1}, /* winners */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4}, /* hcp */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2}, /* controls */
	{  0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}, /* tens */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}, /* jacks */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}, /* queens */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0}, /* kings */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, /* aces */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1}, /* top2 */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1}, /* top3 */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1}, /* top4 */
	{  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1}, /* top5 */
	{  0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 4, 6}  /* c13 */
} ;

struct pccheck check[idxEnd*14] = {{0}};

void initpc()
{
	int i, pc;
	card curc = 1ULL << club_shift |
		1ULL << diamond_shift |
		1ULL << heart_shift |
		1ULL << spade_shift;
	for (i = 0; i < 13; i++) {
		for (pc = 0; pc < idxEnd; pc++) {
			int c = 0;
			if (tblPointcount[pc][i] == 0)
				continue;

			while (checkidx(pc, c)->value != 0 &&
				checkidx(pc, c)->value != tblPointcount[pc][i])
				c++;

			checkidx(pc, c)->value = tblPointcount[pc][i];
			checkidx(pc, c)->mask |= curc;
		}
		curc <<= 1;
	}
}

