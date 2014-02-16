#ifndef POINTCOUNT_H
#define POINTCOUNT_H
/* Indexes into the pointcount array */
#include "card.h"

enum idxPointcount
{
	/* Internal controls and winners for loser calculation */
	idxControlsInt = 0,
	idxWinnersInt ,
	idxHcp ,
	idxBase = idxHcp ,
	idxControls ,
	idxTens ,
	idxJacks ,
	idxQueens ,
	idxKings ,
	idxAces ,
	idxTop2 ,
	idxTop3 ,
	idxWinners = idxTop3 ,
	idxTop4 ,
	idxTop5 ,
	idxC13  ,
	
	idxEnd 
} ;

struct pccheck {
	long value;
	hand mask;
};

/* the pointcount array itself */
extern int tblPointcount [idxEnd][13] ;
extern struct pccheck check[idxEnd*14];

void initpc(void);

static inline struct pccheck *checkidx(int pc, int loc)
{
	return &check[pc*14 + loc];
}

static inline int getpc(int idx, const hand h)
{
	struct pccheck *iter = checkidx(idx, 0);
	long r = 0;
	long val = iter->value;

	do {
		hand count = h & iter->mask;
		r += val * hand_count_cards(count);
		iter++;
		val = iter->value;
	} while(val);

	return r;
}

static inline int getpc(int idx, const hand h);

#endif /* POINTCOUNT_H */
