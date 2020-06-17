#ifndef DEALER_H
#define DEALER_H

typedef unsigned char card;

typedef card deal[52];

static char *player_name[] = { "North", "East", "South", "West" };

int verbose;

/* Changes for cccc and quality */
struct context {
  deal *pd ; /* pointer to current deal */
  struct handstat *ps ; /* Pointer to stats of current deal */
} c;

#ifdef STD_RAND
 #define RANDOM rand
 #define SRANDOM(seed) srand(seed) ;
#else
 #define RANDOM gnurand
 int gnurand ();
 #define SRANDOM(seed) gnusrand(seed)
 int gnusrand (int);
#endif /* STD_RAND */

#include "pointcount.h"

struct handstat {
    int hs_length[NSUITS];      /* distribution */
    int hs_points[NSUITS];      /* 4321 HCP per suit */
    int hs_fl[NSUITS];          /* FL per suit */
    int hs_fv[NSUITS];          /* FV per suit */
    int hs_together[NSUITS];    /* Length together with partner */
    int hs_totalpoints;         /* Sum of above four */
    int hs_totalfl;             /* Sum of above four */
    int hs_totalfv;             /* Sum of above four */
    int hs_bits;                /* Bitmap to check distribution */
    int hs_loser[NSUITS];       /* Losers in a suit */
    int hs_totalloser;          /* Losers in the hand */
    int hs_control[NSUITS];     /* Controls in a suit */
    int hs_totalcontrol;        /* Controls in the hand */
    int hs_counts[idxEnd][NSUITS];  /* other auxiliary counts */
    int hs_totalcounts[idxEnd];         /* totals of the above */
} ;

struct handstat hs[4] ;

deal curdeal;

int maxgenerate;
int maxdealer;
int maxvuln;
int will_print;

#define printcompact(d) (fprintcompact(stdout, d, 0))
#define printoneline(d) (fprintcompact(stdout, d, 1))
#define printlin(d) (fprintlin(stdout, d, 1))

#ifdef FRANCOIS
  int hascard (deal, int, card, int);
  #define HAS_CARD(d,p,c) hascard(d,p,c,0)
#else
  int hascard (deal, int, card);
  #define HAS_CARD(d,p,c) hascard(d,p,c)
#endif

#endif /* DEALER_H */
