#ifndef DEALER_H
#define DEALER_H

#include "card.h"
#include "Random/SFMT.h"

extern const char * const player_name[4];

int verbose;

/* Changes for cccc and quality */
struct context {
  struct board *pd;
  struct handstat *ps ; /* Pointer to stats of current deal */
} c;

#include "pointcount.h"

int suitlength (const struct board *d, struct handstat *hsbase, int compass, int suit);

struct handstat {
    int hs_points[(NSUITS + 1)*2];  /* 4321 HCP per suit or total */
    int hs_bits[2];                 /* Distribution bit position */
    int hs_loser[(NSUITS + 1)*2];   /* Losers in a suit */
    int hs_control[(NSUITS + 1)*2]; /* Controls in a suit or total */
} ;

struct rngstate {
  uint16_t random[512*4];
  unsigned idx;
  sfmt_t sfmt;
} __attribute__  ((aligned (32)));


struct globals {
    long seed;
    const char *initialpack;
    int maxgenerate;
    int maxproduce;
    int nprod;
    int ngen;
    int biastotal;
    int loadindex;
    int maxdealer;
    int maxvuln;

    char biasdeal[4][4];
    int results[2][5][14];
    struct stored_board *deallist;
    double average;

    struct pack curpack;
    struct board predealt;
    struct board curboard;
    struct rngstate rngstate;
    int **distrbitmaps[14];

    struct treebase *decisiontree;
    struct action *actionlist;

    int quiet      : 1;
    int verbose    : 1;
    int progressmeter : 1;
    int will_print : 1;
    int swapping   : 3;
    int loading    : 1;
    int use_compass: 4;
    int computing_mode : 2;
};

enum { STAT_MODE, EXHAUST_MODE };

extern const struct globals *gptr;

struct handstat hs[4] ;

int imps (int scorediff) __attribute__ ((pure));
int score (int vuln, int suit, int level, int dbl, int tricks) __attribute__ ((pure));
void error (char *s) __attribute__ ((noreturn, pure));
void clearpointcount ();
void clearpointcount_alt (int cin);
void pointcount (int index, int value);
void newpack (struct pack *d, const char *initialpack);
card make_card (char rankchar, char suitchar) __attribute__ ((pure));
int make_contract (char suitchar, char trickchar, char dbl) __attribute__ ((pure));
void fprintcompact (FILE * f, const struct board *d, int ononeline, int disablecompass);
void printdeal (const struct board *d);
void printhands (int boardno, const struct board *dealp, int player, int nhands);
void printew (const struct board *d);

typedef int (*evaltreeptr)(struct treebase *b);
extern evaltreeptr evaltreefunc;
typedef card (*hascardptr) (const struct board *d, int player, card onecard);
extern hascardptr hascard;

static inline int getshapenumber (int cl, int di, int ht, int sp)
{
  (void)sp;
  return gptr->distrbitmaps[cl][di][ht];
}

void * mycalloc (unsigned nel, unsigned siz);

extern const char ucrep[14];
extern const char *ucsep[4];

#include "card.h"

#define printcompact(d) (fprintcompact(stdout, d, 0, 0))
#define printoneline(d) (fprintcompact(stdout, d, 1, 0))

 #define HAS_CARD(d,p,c) hascard(d,p,c)

int default_deal_main (struct globals *g);
int sse2_deal_main (struct globals *g);
int popcnt_deal_main (struct globals *g);
int sse4_deal_main (struct globals *g);
int avx2_deal_main (struct globals *g);

#ifdef MVDEFAULT
#define DEFUN(x) default_##x
#elif defined(__AVX2__)
#define DEFUN(x) avx2_##x
#elif defined(__SSE4_2__)
#define DEFUN(x) sse4_##x
#elif defined(__POPCNT__)
#define DEFUN(x) popcnt_##x
#elif defined(__SSE2__)
#define DEFUN(x) sse2_##x
#else
#define DEFUN(x) default_##x
#endif

#define suitlength DEFUN(suitlength)
int suitlength (const struct board *d, struct handstat *hsbase, int compass, int suit);

#endif /* DEALER_H */
