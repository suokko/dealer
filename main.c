#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

#include "bittwiddle.h"
#include "initrandom.h"

char* input_file = 0;

#if defined(_MSC_VER) || defined(__WIN32) || defined(WIN32)
  /* with VC++6, winsock2 declares ntohs and struct timeval */
#ifdef _MSC_VER
  #pragma warning (disable : 4115)
#endif
  #include <winsock2.h>
  #include <fcntl.h>
#ifdef _MSC_VER
  #pragma warning (default : 4115)
#endif
#else
  /* else we assume we can get ntohs/ntohl from netinet */
  #include <netinet/in.h>
#endif /* _MSC_VER */

#if defined(WIN32) || defined(win32)
  #ifdef _MSC_VER
  struct timeval {
    long tv_sec;            /* seconds */
    long tv_usec;            /* and microseconds */
  };
  #pragma warning (disable : 4100)
  #endif /* _MSC_VER */
  void gettimeofday (struct timeval *tv, void *vp) {
    (void)vp;
    tv->tv_sec = time (0);
    tv->tv_usec = 0;
  }
  #ifdef _MSC_VER
  #pragma warning (default : 4100)
#endif
#else
  #include <unistd.h>
  #include <sys/time.h>
#endif /* WIN32 */

#include <getopt.h>

#include "dealer.h"
#include "tree.h"
#include "cpudetect/detect.h"

void yyerror (char *);

#define DEFAULT_MODE STAT_MODE

#ifdef MSDOS
  const char * const crlf = "\r\n";
#else
  const char * const crlf = "\n";
#endif /* MSDOS */

/* Global variables */


const char ucrep[14] = "23456789TJQKA";
#ifdef UTF8_SUPPORTED
const char *ucsep[4]  = {u8"♣",u8"♦",u8"♥",u8"♠"};
#else
const char *ucsep[4]  = {"C","D","H","S"};
#endif


static const int imparr[24] = { 10,   40,   80,  120,  160,  210,  260,  310,  360,
                  410,  490,  590,  740,  890, 1090, 1190, 1490, 1740,
                 1990, 2240, 2490, 2990, 3490, 3990};

const char* const suit_name[] = {"Club", "Diamond", "Heart", "Spade", "Notrump"};


const char * const player_name[] = { "North", "East", "South", "West" };

/* Various handshapes can be asked for. For every shape the user is
   interested in a number is generated. In every distribution that fits that
   shape the corresponding bit is set in the distrbitmaps 3-dimensional array.
   This makes looking up a shape a small constant cost.
*/
#define MAXDISTR 8*sizeof(int)

static struct tree defaulttree = {{TRT_NUMBER}, NIL, NIL, 1, 0, 0};
static struct action defaultaction = {(struct action *) 0, ACT_PRINTALL, NULL, NULL, 0, 0, {{0}}};
evaltreeptr evaltreefunc;
hascardptr hascard;

struct globals *gp;
const struct globals *gptr;

/* Function definitions */
void yyparse ();

int imps (int scorediff) {
  int i, j;
  j = abs (scorediff);
  for (i = 0; i < 24; i++)
    if (imparr[i] >= j) return scorediff < 0 ? -i : i;
  return scorediff < 0 ? -i : i;
}

static int scoreone (int vuln, int suit, int level, int dbl, int tricks) {
  int total = 0;

  /* going down */
  if (tricks < 6 + level) {
    if (!dbl)
      return -50 * (1 + vuln) * (6 + level - tricks);
    if (vuln) {
      return -200 - 300 * (5 + level - tricks);
    } else {
      switch (6 + level - tricks) {
        case 1:
          return -100;
        case 2:
          return -300;
        default:
          return -500 - 300 * (3 + level - tricks);
      }
    }
  }

  int trickpts = ((suit >= SUIT_HEART) ? 30 : 20);
  int contractpts = level * trickpts;
  if (suit == SUIT_NT) contractpts += 10;

  if (dbl) contractpts *= 2;
  /* Contracted trick points */
  total = contractpts;
  /* part score bonus */
  total += 50;

  if (dbl) total += 50;

  /* game bonus */
  if (contractpts >= 100) total += 250 + 200 * vuln;

  /* small slam bonus */
  if (level == 6) total += 500 + 250 * vuln;

  /* grand slam bonus */
  if (level == 7) total += 1000 + 500 * vuln;

  /* Over tricks */
  if (dbl)
    total += 100 * (1 + vuln) * (tricks - 6 - level);
  else
    total += trickpts * (tricks - 6 - level);

  return total;
}

struct value score (int vuln, int suit, int level, int dbl, struct value tricks) {
  if (tricks.type == VAL_INT) {
    tricks.intvalue = scoreone(vuln, suit, level, dbl, tricks.intvalue);
  } else {
    unsigned idx;
    for (idx = 0; idx < sizeof(*tricks.array->key)/sizeof(tricks.array->key[0])
        && tricks.array->key[idx] != 0; idx++) {
      tricks.array->value[idx] = scoreone(vuln, suit, level, dbl, tricks.array->value[idx]);
    }
  }
  return tricks;
}

void error (const char* s) {
  fprintf (stderr, "%s%s", s, crlf);
  exit (10);
}

/* implementations of regular & alternate pointcounts */

static int countindex = -1;

static void zerocount (int points[13]) {
  int i;
  for (i = 12; i >= 0; i--) points[i] = 0;
}

void clearpointcount () {
  zerocount (tblPointcount[idxHcp]);
  countindex = idxHcp;
}

void clearpointcount_alt (int cin) {
  if (cin + idxBase >= idxEnd || cin < 0)
    yyerror("Alternative point count out of range");
  zerocount (tblPointcount[cin + idxBase]);
  countindex = cin + idxBase;
}

void pointcount (int index, int value) {
  assert (index <= 12);
  if (index < 0) {
      yyerror ("too many pointcount values");
  }
  tblPointcount[countindex][index] = value;
}

void * mycalloc (unsigned nel, unsigned siz) {
  char *p;

  p = calloc (nel, siz);
  if (p) return p;
  fprintf (stderr, "Out of memory\n");
  exit (-1); /*NOTREACHED */
}

static void initdistr (struct globals *g) {
  unsigned clubs;

  int shape = 0;

  for (clubs = 0; clubs <= 13; clubs++) {
    unsigned max = 14 - clubs;
    unsigned min = 0;
    unsigned toadd = ((max+min)*(max-min+1))/2;
    g->distrbitmaps[clubs] = shape;
    shape += toadd;
  }
}

void  setshapebit (struct shape *s, int cl, int di, int ht, int sp)
{
  (void)sp;
  unsigned nr = getshapenumber(cl, di, ht);
  int idx = nr / 32;
  int bit = nr % 32;
  s->bits[idx] |= 1 << bit;
}

struct rng {
  FILE *f;
  unsigned char random[70];
  unsigned idx;
};

card make_card (char rankchar, char suitchar);

void newpack (union pack *d, const char *initialpack) {
  int suit, rank, place;

  place = 0;
  for (suit = SUIT_CLUB; suit <= SUIT_SPADE; suit++)
    for (rank = 0; rank < 13; rank++)
      d->c[place++] = MAKECARD (suit, rank);

  /* If user wants random order initial pack do a single shuffle using
   * user values or system random number generator.
   */
  if (initialpack) {
    char statetext[52*13+1];
    char *iter = statetext;

    if (strcmp(initialpack, "rng") == 0) {
      struct initrng *state = irng_open();

      for (place = 0; place < 51; place++) {
        int pos = irng_next(state, place, 51);
        assert (pos < 52);
        assert (pos >= place);
        card t = d->c[pos];
        d->c[pos] = d->c[place];
        d->c[place] = t;
        iter += sprintf(iter, "%s%c", ucsep[C_SUIT(t)], ucrep[C_RANK(t)]);
      }
      card t = d->c[place];
      iter += sprintf(iter, "%s%c", ucsep[C_SUIT(t)], ucrep[C_RANK(t)]);
      irng_close(state);
    } else {
      const char *initer = initialpack;
      const char *suitsymbols[4][4] = {
        {"C","c",u8"♣",u8"♧"},
        {"D","d",u8"♦",u8"♤"},
        {"H","h",u8"♥",u8"♡"},
        {"S","s",u8"♠",u8"♤"},
      };
      for (place = 0; place < 52; place++) {
        char suit, rank;
        int s, i;
        if (initer[0] == '\0')
          error("Need 52 card for initial pack order");
        for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
          suit = suitsymbols[s][0][0];
          for (i = 0; i < 4; i++) {
            if (strncmp(initer, suitsymbols[s][i], strlen(suitsymbols[s][i])) == 0) {
              initer += strlen(suitsymbols[s][i]);
              goto breakout;
            }
          }
        }
        error("Unknown suit symbol");
breakout:
        if (initer[0] == '\0')
          error("Need 52 card for initial pack order");
        rank = initer[0];
        initer++;

        d->c[place] = make_card(rank, suit);
        card t = d->c[place];
        iter += sprintf(iter, "%s%c", ucsep[C_SUIT(t)], ucrep[C_RANK(t)]);
      }
    }

    if (!gptr->quiet)
      printf("Initial pack order: %s\n", statetext);

    free((char *)initialpack);
  }
}

card make_card (char rankchar, char suitchar) {
  int rank, suit = 0;

  for (rank = 0; rank < 13 && ucrep[rank] != rankchar; rank++) ;
  assert (rank < 13);
  switch (suitchar) {
    case 'C':
      suit = 0;
      break;
    case 'D':
      suit = 1;
      break;
    case 'H':
      suit = 2;
      break;
    case 'S':
      suit = 3;
      break;
    default:
      assert (0);
    }
  return MAKECARD (suit, rank);
}

int make_contract (char suitchar, char trickchar, char dbl) {
  int trick, suit;
  trick = (int) trickchar - '0';
  switch (suitchar) {
    case 'C':
      suit = 0;
      break;
    case 'D':
      suit = 1;
      break;
    case 'H':
      suit = 2;
      break;
    case 'S':
      suit = 3;
      break;
    case 'N':
      suit = 4;
      break;
    default:
      suit = 0;
      printf ("%c", suitchar);
      assert (0);
  }
  return MAKECONTRACT (suit, trick) + (dbl == 'x' ? 64 : 0);
}

void fprintcompact (FILE * f, const union board *d, int ononeline, int disablecompass) {
  char pt[] = "nesw";
  int s, p, r;
  for (p = COMPASS_NORTH; p <= COMPASS_WEST; p++) {
    if (!disablecompass) fprintf (f, "%c ", pt[p]);
    for (s = SUIT_SPADE; s >= SUIT_CLUB; s--) {
      for (r = 12; r >= 0; r--)
        if (HAS_CARD (d, p, MAKECARD (s, r)))
          fprintf (f, "%c", ucrep[r]);
      if (s > 0) fprintf (f, ".");
    }
    /* OK to use \n as this is mainly intended for internal dealer use. */
    fprintf (f, ononeline && p != COMPASS_WEST ? " " : "\n");
  }
}

void printdeal (const union board *d) {
  int suit, player, rank, cards;

  printf ("%4d.\n", (gptr->nprod+1));

  for (suit = SUIT_SPADE; suit >= SUIT_CLUB; suit--) {
    cards = 10;
    for (player = COMPASS_NORTH; player <= COMPASS_WEST; player++) {
      while (cards < 10) {
        printf ("  ");
        cards++;
      }
      cards = 0;
      for (rank = 12; rank >= 0; rank--) {
        if (HAS_CARD (d, player, MAKECARD (suit, rank))) {
          printf ("%c ", ucrep[rank]);
          cards++;
        }
      }
      if (cards == 0) {
        printf ("- ");
        cards++;
      }
    }
    printf ("\n");
  }
  printf ("\n");
}

void printcard(card c) {
  assert(hand_count_cards(c) == 1);
  printf("%s%c", ucsep[C_SUIT(c)], ucrep[C_RANK(c)]);
}

void predeal (int player, card onecard) {

  int i;
  card c = 0;
  for (i = 0; i < 4; i++) {
    c = hand_has_card(gp->predealt.hands[i], onecard);
    if (c == onecard)
      yyerror ("Card predealt twice");
  }
  gp->predealt.hands[player] = hand_add_card(gp->predealt.hands[player], onecard);
  if (hand_count_cards(gp->predealt.hands[player]) > 13)
    yyerror("More than 13 cards for one player");
}

void printhands (int boardno, const union board *dealp, int player, int nhands) {
  int i, suit, rank, cards;

  for (i = 0; i < nhands; i++)
    printf ("%4d.%15c", boardno + i + 1, ' ');
  printf ("\n");
  for (suit = SUIT_SPADE; suit >= SUIT_CLUB; suit--) {
    cards = 10;
    for (i = 0; i < nhands; i++) {
      while (cards < 10) {
        printf ("  ");
        cards++;
      }
      cards = 0;
      for (rank = 12; rank >= 0; rank--) {
        if (HAS_CARD (&dealp[i], player, MAKECARD (suit, rank))) {
          printf ("%c ", ucrep[rank]);
          cards++;
        }
      }
      if (cards == 0) {
        printf ("- ");
        cards++;
      }
    }
    printf ("\n");
  }
  printf ("\n");
}

int yywrap () {
  /* Necessary if you do not have a -ll library */
  return 1;
}

void printew (const union board*d) {
  /* This function prints the east and west hands only (with west to the
     left of east), primarily intended for examples of auctions with 2
     players only.  HU.  */
  int suit, player, rank, cards;

  for (suit = SUIT_SPADE; suit >= SUIT_CLUB; suit--) {
    cards = 10;
    for (player = COMPASS_WEST; player >= COMPASS_EAST; player--) {
      if (player != COMPASS_SOUTH) {
        while (cards < 10) {
          printf ("  ");
          cards++;
        }
        cards = 0;
        for (rank = 12; rank >= 0; rank--) {
          if (HAS_CARD (d, player, MAKECARD (suit, rank))) {
            printf ("%c ", ucrep[rank]);
            cards++;
          }
        }
        if (cards == 0) {
          printf ("- ");
          cards++;
        }
      }
   }
   printf ("\n");
  }
  printf ("\n");
}

static void showevalcontract (int nh) {
  int s, l, i, v;
  for (v = 0; v < 2; v++) {
    printf ("%sVulnerable%s", v ? "" : "Not ", crlf);
    printf ("   ");
    for (l = 1; l < 8; l++) printf ("  %d       ", l);
    printf ("%s", crlf);
    for (s = 0; s < 5; s++) {
      printf ("%c: ", "cdhsn"[s]);
      for (l = 1; l < 8; l++) {
        int t = 0, tn = 0;
        for (i = 0; i < 14; i++) {
          t += gptr->results[0][s][i] * scoreone (v, s, l, 0, i);
          tn += gptr->results[1][s][i] * scoreone (v, s, l, 0, i);
        }
        printf ("%4d/%4d ", t / nh, tn / nh);
      }
      printf ("%s", crlf);
    }
    printf ("%s", crlf);
  }
}

static void cleanup_action () {
  struct action *acp;
  int player, i;

  for (acp = gptr->actionlist; acp != 0; acp = acp->ac_next) {
    switch (acp->ac_type) {
      default:
        assert (0); /*NOTREACHED */
      case ACT_PRINTALL:
      case ACT_PRINTCOMPACT:
      case ACT_PRINTPBN:
      case ACT_PRINTEW:
      case ACT_PRINTONELINE:
      case ACT_PRINTES:
        break;
      case ACT_EVALCONTRACT:
        showevalcontract (gptr->nprod);
        break;
      case ACT_PRINT:
        for (player = COMPASS_NORTH; player <= COMPASS_WEST; player++) {
          if (!(acp->ac_int1 & (1 << player)))
            continue;
          printf ("\n\n%s hands:\n\n\n\n", player_name[player]);
          for (i = 0; i < gptr->nprod; i += 4) {
            union board b[4];
            int j;
            for (j = 0; j < 4 && j < gptr->nprod - i; j++)
              board_from_stored(&b[j], &gptr->deallist[i + j]);
            printhands (i, b, player, gptr->nprod - i > 4 ? 4 : gptr->nprod - i);
          }
          printf ("\f");
        }
        break;
      case ACT_AVERAGE:
        gp->average = (double)acp->ac_int1 / gptr->nprod;
        if (acp->ac_expr2) {
          struct value r = evaltreefunc(acp->ac_expr2);
          if (r.type == VAL_INT && !r.intvalue)
            break;
        }
        if (acp->ac_str1)
          printf ("%s: ", acp->ac_str1);
        printf ("%g\n", gp->average);
        break;
      case ACT_FREQUENCY:
        printf ("Frequency %s:\n", acp->ac_str1 ? acp->ac_str1 : "");
        if (acp->ac_u.acu_f.acuf_uflow)
          printf ("Low\t%8ld\n", acp->ac_u.acu_f.acuf_uflow);
        for (i = acp->ac_u.acu_f.acuf_lowbnd; i <= acp->ac_u.acu_f.acuf_highbnd; i++) {
          if (acp->ac_u.acu_f.acuf_freqs[i - acp->ac_u.acu_f.acuf_lowbnd] > 0)
            printf ("%5d\t%8ld\n", i, acp->ac_u.acu_f.acuf_freqs[i - acp->ac_u.acu_f.acuf_lowbnd]);
        }
        if (acp->ac_u.acu_f.acuf_oflow)
          printf ("High\t%8ld\n", acp->ac_u.acu_f.acuf_oflow);
        free(acp->ac_u.acu_f.acuf_freqs);
        break;
      case ACT_FREQUENCYLEAD:
      case ACT_FREQUENCY2D: {
        int j, n = 0, low1 = 0, high1 = 0, low2 = 0, high2 = 0, sumrow,
          sumtot, sumcol, *toprintcol, *toprintrow;
        printf ("Frequency %s:%s", acp->ac_str1 ? acp->ac_str1 : "", crlf);
        high1 = acp->ac_u.acu_f2d.acuf_highbnd_expr1;
        high2 = acp->ac_u.acu_f2d.acuf_highbnd_expr2;
        low1 = acp->ac_u.acu_f2d.acuf_lowbnd_expr1;
        low2 = acp->ac_u.acu_f2d.acuf_lowbnd_expr2;
        toprintcol = (int*)mycalloc((high2 - low2 + 3), sizeof(int));
        toprintrow = (int*)mycalloc((high1 - low1 + 3), sizeof(int));
        for (i = 0; i < (high1 - low1) + 3; i++) {
          for (j = 0; j < (high2 - low2) + 3; j++) {
            toprintcol[j] |= acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * i + j];
            toprintrow[i] |= acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * i + j];
          }
        }
        if (toprintcol[0] != 0)
          printf ("        Low");
        else
          printf ("    ");
        for (j = 1; j < (high2 - low2) + 2; j++) {
          if (toprintcol[j] != 0) {
            if (acp->ac_type == ACT_FREQUENCY2D)
              printf (" %6d", j + low2 - 1);
            else {
              card * header = (card*)acp->ac_expr2;
              printf ("     ");
              printcard(header[j - 1]);
            }
          }
        }
        if (toprintcol[j] != 0)
          printf ("   High");
        printf ("    Sum%s", crlf);
        sumtot = 0;
        for (i = 0; i < (high1 - low1) + 3; i++) {
          sumrow = 0;
          if (toprintrow[i] == 0)
            continue;
          if (i == 0)
            printf ("Low ");
          else if (i == (high1 - low1 + 2))
            printf ("High");
          else
            printf ("%4d", i + low1 - 1);
          for (j = 0; j < (high2 - low2) + 3; j++) {
            n = acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * i + j];
            sumrow += n;
            if (toprintcol[j] != 0)
              printf (" %6d", n);
          }
          printf (" %6d%s", sumrow, crlf);
          sumtot += sumrow;
        }
        printf ("Sum ");
        for (j = 0; j < (high2 - low2) + 3; j++) {
          sumcol = 0;
          for (i = 0; i < (high1 - low1) + 3; i++)
            sumcol += acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * i + j];
          if (toprintcol[j] != 0)
            printf (" %6d", sumcol);
        }
        printf (" %6d%s%s", sumtot, crlf, crlf);
        if (acp->ac_type == ACT_FREQUENCYLEAD)
          free(acp->ac_expr2);
        free(acp->ac_u.acu_f2d.acuf_freqs);
        free(toprintcol);
        free(toprintrow);
      }
    }
  }
}

static void initglobals(struct globals *g)
{
  gptr = g;
  gp = g;
  memset(g->biasdeal, -1, sizeof(g->biasdeal));
  g->curboard.hands[0] = club_mask;
  g->curboard.hands[1] = diamond_mask;
  g->curboard.hands[2] = heart_mask;
  g->curboard.hands[3] = spade_mask;
  g->decisiontree = &defaulttree.base;
  g->actionlist = &defaultaction;
  g->verbose = 1;
  g->maxdealer = -1;
}

int main (int argc, char **argv) {
  int seed_provided = 0;
  extern int optind;
  extern char *optarg;
  int c;
  int errflg = 0;

  struct timeval tvstart, tvstop;

  struct globals global = {.seed = 0};

  initglobals(&global);


  gettimeofday (&tvstart, (void *) 0);

  while ((c = getopt (argc, argv, "023ehvmqp:g:s:l:Vi:")) != -1) {
    switch (c) {
      case '0':
      case '2':
      case '3':
        gp->swapping = c - '0';
        break;
      case 'e':
        gp->computing_mode = EXHAUST_MODE;
        break;
      case 'l':
        gp->loading = 1;
        gp->loadindex = atoi (optarg);
        break;
      case 'g':
        gp->maxgenerate = atoi (optarg);
        break;
      case 'm':
        gp->progressmeter ^= 1;
        break;
      case 'p':
        gp->maxproduce = atoi (optarg);
        break;
      case 'i':
        /* 96bit number translated to initial pack order or rng for secure
         * random number from system.
         */
        gp->initialpack = strdup(optarg);
        break;
      case 's':
        seed_provided = 1;
        gp->seed = atol (optarg);
        if (gp->seed == LONG_MIN || gp->seed == LONG_MAX) {
            fprintf (stderr, "Seed overflow: seed must be between %ld and %ld\n",
              LONG_MIN, LONG_MAX);
            exit (-1);
        }
        break;
      case 'v':
        gp->verbose ^= 1;
        break;
      case 'q':
        gp->quiet ^= 1;
        break;
      case 'V':
        printf ("Version info....\n");
        printf ("$Revision: 1.24 $\n");
        printf ("$Date: 2003/08/05 19:53:04 $\n");
        printf ("$Author: henk $\n");
        return 1;
      case '?':
      case 'h':
        errflg = 1;
        break;
      }
    }
  if (argc - optind > 2 || errflg) {
    fprintf (stderr, "Usage: %s [-emv] [-s seed] [-i rng|52 cards] [-p num] [-v num] [inputfile]\n", argv[0]);
    exit (-1);
  }
  if (optind < argc && freopen (input_file = argv[optind], "r", stdin) == NULL) {
    perror (argv[optind]);
    exit (-1);
  }
  initdistr (gp);
  gp->maxvuln = -1;

  yyparse ();

  /* The most suspect part of this program */
  if (!seed_provided) {
    (void) time (&gp->seed);
  }

  if (gp->maxgenerate == 0)
    gp->maxgenerate = 10000000;
  if (gp->maxproduce == 0)
    gp->maxproduce = ((gp->actionlist == &defaultaction) || gp->will_print) ? 40 : gp->maxgenerate;

  int r = deal_main(gp);

  gettimeofday (&tvstop, (void *) 0);
  cleanup_action ();
  if (gp->verbose) {
    printf ("Generated %d hands\n", gp->ngen);
    printf ("Produced %d hands\n", gp->nprod);
    printf ("Initial random seed %lu\n", gp->seed);
    printf ("Time needed %8.3f sec%s",
             (tvstop.tv_sec + tvstop.tv_usec / 1000000.0 -
             (tvstart.tv_sec + tvstart.tv_usec / 1000000.0)), crlf);
  }
  return r;
}
