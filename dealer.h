#ifndef DEALER_H
#define DEALER_H

#include "card.h"
#include "pointcount.h"

#include "cpudetect/entry.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char* const player_name[4];
extern const char* const suit_name[5];

/* Changes for cccc and quality */
struct context {
  union board *pd;
  struct handstat *ps ; /* Pointer to stats of current deal */
};

struct handstat {
    int hs_points[(NSUITS + 1)*2];  /* 4321 HCP per suit or total */
    int hs_bits[2];                 /* Distribution bit position */
    int hs_loser[(NSUITS + 1)*2];   /* Losers in a suit */
    int hs_control[(NSUITS + 1)*2]; /* Controls in a suit or total */
} ;

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

    union board predealt;
    union board curboard;
    int16_t distrbitmaps[14];
    uint16_t libtricks[5];

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

int imps (int scorediff) __attribute__ ((pure));
int scoreone (int vuln, int suit, int level, int dbl, int tricks);
void error (const char* s) __attribute__ ((noreturn));
void clearpointcount ();
void clearpointcount_alt (int cin);
void pointcount (int index, int value);
void newpack (union pack *d, const char *initialpack);
card make_card (char rankchar, char suitchar) __attribute__ ((pure));
int make_contract (char suitchar, char trickchar, char dbl) __attribute__ ((pure));
void fprintcompact (FILE * f, const union board *d, int ononeline, int disablecompass);
void printdeal (const union board *d);
void printhands (int boardno, const union board *dealp, int player, int nhands);
void printew (const union board *d);
void showevalcontract (int nh);

struct value_array {
  int key[13];
  int value[13];
};

static inline int getshapenumber (unsigned cl, unsigned di, unsigned ht)
{
  unsigned max = 14-cl;
  unsigned min = max-di+1;
  di = ((max+min)*(max-min+1))/2;
  max = gptr->distrbitmaps[cl] + di + ht;
  return max;
}

void * mycalloc (unsigned nel, unsigned siz);

extern const char ucrep[14];
extern const char **ucsep;
extern const char *const crlf;

#include "card.h"

#define printcompact(d) (fprintcompact(stdout, d, 0, 0))
#define printoneline(d) (fprintcompact(stdout, d, 1, 0))

void printcard(card c);

#define HAS_CARD(d,p,c) hand_has_card((d)->hands[p], c)


extern int (*deal_main)(struct globals *g);

#ifdef MVDEFAULT
#define CONCAT2(a,b) a ##_## b
#define CONCAT(a,b) CONCAT2(a,b)
#define DEFUN(x) CONCAT(MVDEFAULT, x)
#else
#define DEFUN(x) default_##x
#endif

#ifdef __cplusplus
}

extern entry<decltype(deal_main), &deal_main> deal_main_entry;

namespace DEFUN() {
int suitlength (const union board* d, int compass,int suit);
}

#endif

#endif /* DEALER_H */
