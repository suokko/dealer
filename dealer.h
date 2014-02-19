#ifndef DEALER_H
#define DEALER_H

#include "card.h"

extern const char * const player_name[4];

int verbose;

/* Changes for cccc and quality */
struct context {
  struct board *pd;
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

int suitlength (const struct board *d, struct handstat *hsbase, int compass, int suit);

struct handstat {
    int hs_points[(NSUITS + 1)*2];  /* 4321 HCP per suit or total */
    int hs_bits[2];                 /* Distribution bit position */
    int hs_loser[(NSUITS + 1)*2];   /* Losers in a suit */
    int hs_control[(NSUITS + 1)*2]; /* Controls in a suit or total */
} ;

struct handstat hs[4] ;

int maxgenerate;
int maxdealer;
int maxvuln;
int will_print;

#include "card.h"
extern struct board *curdeal;

#define printcompact(d) (fprintcompact(stdout, d, 0, 0))
#define printoneline(d) (fprintcompact(stdout, d, 1, 0))

card hascard (const struct board *, int, card);
 #define HAS_CARD(d,p,c) hascard(d,p,c)

#endif /* DEALER_H */
