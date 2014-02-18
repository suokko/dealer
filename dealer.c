#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

long seed = 0;
static int quiet = 0;
char* input_file = 0;

#if defined(_MSC_VER) || defined(__WIN32) || defined(WIN32)
  /* with VC++6, winsock2 declares ntohs and struct timeval */
  #pragma warning (disable : 4115)
  #include <winsock2.h>
  #include <fcntl.h>
  #pragma warning (default : 4115)
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
  #endif /* _MSC_VER */
  #pragma warning (disable : 4100)
  void gettimeofday (struct timeval *tv, void *pv) {
    tv->tv_sec = time (0);
    tv->tv_usec = 0;
  }
  #pragma warning (default : 4100)
#else
  #include <unistd.h>
  #include <sys/time.h>
#endif /* WIN32 */

#include <getopt.h>

#include "tree.h"
#include "pointcount.h"
#include "dealer.h"
#include "c4.h"
#include "pbn.h"
#include "genlib.h"

#include "card.h"

void yyerror (char *);

#define TWO_TO_THE_13 (1<<13)
#define DEFAULT_MODE STAT_MODE
#define RANDBITS 16
#define NRANDVALS (1<<RANDBITS)
#define NRANDMASK (NRANDVALS-1)

#ifdef MSDOS
  char *crlf = "\r\n";
#else
  char *crlf = "\n";
#endif /* MSDOS */

/* Global variables */

enum { STAT_MODE, EXHAUST_MODE };
static int computing_mode = DEFAULT_MODE;

static const char ucrep[14] = "23456789TJQKA";

static int biastotal = 0;
int biasdeal[4][4] = { {-1, -1, -1, -1}, {-1, -1, -1, -1},
                       {-1, -1, -1, -1}, {-1, -1, -1, -1}};
static struct board predealt = {{0}};

static const int imparr[24] = { 10,   40,   80,  120,  160,  210,  260,  310,  360,
                  410,  490,  590,  740,  890, 1090, 1190, 1490, 1740,
                 1990, 2240, 2490, 2990, 3490, 3990};

static const char * const suit_name[] = {"Club", "Diamond", "Heart", "Spade"};
const char * const player_name[] = { "North", "East", "South", "West" };
static struct pack curpack = {{0}};
static struct board curboard = {{club_mask, diamond_mask, heart_mask, spade_mask}};
struct board *curdeal = &curboard;
static hand dealtcardsmask;
typedef const struct board * deal;

static int swapping = 0;
static int swapindex = 0;
static int loading = 0;
static int loadindex = 0;
static double average = 0.0;

/* Various handshapes can be asked for. For every shape the user is
   interested in a number is generated. In every distribution that fits that
   shape the corresponding bit is set in the distrbitmaps 3-dimensional array.
   This makes looking up a shape a small constant cost.
*/
#define MAXDISTR 8*sizeof(int)
static int **distrbitmaps[14];

static int results[2][5][14];
int use_compass[NSUITS];
int use_vulnerable[NSUITS];
static int nprod;
int maxproduce;
static int ngen;

static struct tree defaulttree = {{TRT_NUMBER}, NIL, NIL, 1, 0, 0};
struct treebase *decisiontree = &defaulttree.base;
static struct action defaultaction = {(struct action *) 0, ACT_PRINTALL, NULL, NULL, 0, 0, {{0}}};
struct action *actionlist = &defaultaction;
static unsigned char zero52[NRANDVALS];
static struct stored_board *deallist;

/* Function definitions */
static void fprintcompact (FILE *, deal, int, int);
static void error (char *);
static void printew (deal d);
void yyparse ();
static int true_dd (deal d, int l, int c); /* prototype */

  /* Special variables for exhaustive mode 
     Exhaustive mode created by Francois DELLACHERIE, 01-1999  */
  int vectordeal;
  int exh_vect_length;
  /* a vectordeal is a binary *word* of at most 26 bits: there are at most
     26 cards to deal between two players.  Each card to deal will be
     affected to a given position in the vector.  Example : we need to deal
     the 26 minor cards between north and south (what a double fit !!!)
     Then those cards will be represented in a binary vector as shown below:
          Card    : DA DK ... D2 CA CK ... C4 C3 C2
          Bit-Pos : 25 24     13 12 11      2  1  0  
     The two players will be respectively represented by the bit values 0 and
     1. Let's say north gets the 0, and south the 1. Then the vector
     11001100000001111111110000 represents the following hands :
          North: D QJ8765432 C 5432
          South: D AKT9      C AKQJT9876
     A suitable vectordeal is a vector deal with hamming weight equal to 13. 
     For computing those vectors, we use a straightforward
     meet in the middle approach.  */
  int exh_predealt_vector = 0;
  card exh_card_at_bit[26];
  /* exh_card_at_bit[i] is the card pointed by the bit #i */
  unsigned char exh_player[2];
  /* the two players that have unknown cards */

static int uniform_random(int max) {
  unsigned rnd;
  unsigned val;
#if STD_RAND
  const int rand_max = RAND_MAX;
#else
  const unsigned rand_max = RAND_MAX << 1;
#endif
  do {
    val = RANDOM ();
    rnd = val % max;
  } while (max - rnd > rand_max - val);
  return (int)rnd;
}

static int uniform_random_table() {
  unsigned rnd;
  do {
    rnd = RANDOM ();
#ifdef STD_RAND
    rnd = rnd & (NRANDMASK);
#else
    rnd = rnd >> (31 - RANDBITS);
#endif
    rnd = zero52[rnd & NRANDMASK];
  } while (rnd == 0xFF);
  return rnd;
}

static void initevalcontract () {
  int i, j, k;
  for (i = 0; i < 2; i++)
    for (j = 0; j < 5; j++)
      for (k = 0; k < 14; k++)
        results[i][j][k] = 0;
}

static int imps (int scorediff) {
  int i, j;
  j = abs (scorediff);
  for (i = 0; i < 24; i++)
    if (imparr[i] >= j) return scorediff < 0 ? -i : i;
  return scorediff < 0 ? -i : i;
}

static int score (int vuln, int suit, int level, int tricks) {
  int total = 0;

  /* going down */
  if (tricks < 6 + level) return -50 * (1 + vuln) * (6 + level - tricks);

  /* Tricks */
  total = total + ((suit >= SUIT_HEART) ? 30 : 20) * (tricks - 6);

  /* NT bonus */
  if (suit == SUIT_NT) total += 10;

  /* part score bonus */
  total += 50;

  /* game bonus for NT */
  if ((suit == SUIT_NT) && level >= 3) total += 250 + 200 * vuln;

  /* game bonus for major-partscore bns */
  if ((suit == SUIT_HEART || suit == SUIT_SPADE) && level >= 4) total += 250 + 200 * vuln;

  /* game bonus for minor-partscore bns */
  if ((suit == SUIT_CLUB || suit == SUIT_DIAMOND) && level >= 5) total += 250 + 200 * vuln;

  /* small slam bonus */
  if (level == 6) total += 500 + 250 * vuln;

  /* grand slam bonus */
  if (level == 7) total += 1000 + 500 * vuln;

  return total;
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
          t += results[0][s][i] * score (v, s, l, i);
          tn += results[1][s][i] * score (v, s, l, i);
        }
        printf ("%4d/%4d ", t / nh, tn / nh);
      }
      printf ("%s", crlf);
    }
    printf ("%s", crlf);
  }
}

static int dd (const struct board *d, int l, int c) {
  /* results-cached version of dd() */
  /* the dd cache, and the ngen it refers to */
  static int cached_ngen = -1;
  static char cached_tricks[4][5];

  /* invalidate cache if it's another deal */
  if (ngen != cached_ngen) {
      memset (cached_tricks, -1, sizeof (cached_tricks));
      cached_ngen = ngen;
  }
  if (cached_tricks[l][c] == -1) {
      /* cache the costly computation's result */
      cached_tricks[l][c] = true_dd (d, l, c);
  }

  /* return the cached value */
  return cached_tricks[l][c];
}

struct tagLibdeal libdeal;

static int get_tricks (int pn, int dn) {
  int tk = libdeal.tricks[dn];
  int resu;
  resu = (pn ? (tk >> (4 * pn)) : tk) & 0x0F;
  return resu;
}

#if defined(WIN32) || defined(__WIN32)
static int mkstemp(char *temp)
{
  mktemp(temp);
  return open(temp, O_RDWR | O_CREAT);
}
#endif

static int true_dd (const struct board *d, int l, int c) {
  if (loading && libdeal.valid) {
    int resu = get_tricks ((l + 1) % 4, (c + 1) % 5);
    /* This will get the number of tricks EW can get.  If the user wanted NS, 
       we have to subtract 13 from that number. */
    return ((l == 0) || (l == 2)) ? 13 - resu : resu;
  } else {
#ifdef MSDOS
    /* Ugly fix for MSDOS. Requires a file called in.txt and will create the
       files tst.pbn and out.txt. Note that we need not user crlf here, as it's
       only dealer that will read the files anyway, Micke Hovmöller 990310 */
    FILE *f;
    char tn1[] = "tst.pbn";
    char tn2[] = "out.txt";
    char res;

    f = fopen (tn1, "w+");
    if (f == 0) error ("Can't open temporary file");
    fprintcompact (f, d, 0);
    /* Write the player to lead and strain. Note that since the player to lead
       sits _behind_ declarer, the array is "eswn" instead of "nesw".
       /Micke Hovmöller 990312 */
    fprintf (f, "%c %c\n", "eswn"[l], "cdhsn"[c]);
    fclose (f);

    fflush (stdout);
    system ("bridge.exe < in.txt > out.txt");
    fflush (stdout);
    f = fopen (tn2, "r");
    if (f == 0) error ("Can't read output of analysis");

    fscanf (f, "%*[^\n]\nEnter argument line: %c", &res);
    fclose (f);
    /* This will get the number of tricks EW can get.  If the user wanted NW, 
       we have to subtract 13 from that number. */
    return ((l == 1) || (l == 3)) ? 13 - res : res;
#else
    FILE *f;
    int r;
    char cmd[1024];
    char tn1[] = "input.XXXXXX",  tn2[] = "output.XXXXXX";
    int res;

    int fd = mkstemp (tn1);
    if (fd < 0 ) error ("Can't create temporary file");
    f = fdopen (fd, "w+");
    if (f == 0 ) error ("Can't open temporary file");
    fprintcompact (f, d, 1, 1);
    fclose (f);
    fd = mkstemp (tn2);
    if (fd < 0 ) error ("Can't create temporary file");
    sprintf (cmd, "dds %s -trumps=%c -leader=%c -sol=1 | tail -n1 > %s;", tn1, "cdhsn"[c], "eswn"[l], tn2);
    system (cmd);
    f = fdopen (fd, "r");
    if (f == 0) error ("Can't read output of analysis");
    r = fscanf (f, " %d: ", &res);
    if (r != 1) fprintf(stderr, "Error reading number of triks from dds\n");
    fclose (f);
    remove (tn1);
    remove (tn2);
    return res;
#endif /* MSDOS */
  }
}

static void evalcontract () {
  int s;
  for (s = 0; s < 5; s++) {
    results[1][s][dd (curdeal, 2, s)]++;      /* south declarer */ 
    results[0][s][dd (curdeal, 0, s)]++;      /* north declarer */
  }
}

static void error (char *s) {
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

char * mycalloc (unsigned nel, unsigned siz) {
  char *p;

  p = calloc (nel, siz);
  if (p) return p;
  fprintf (stderr, "Out of memory\n");
  exit (-1); /*NOTREACHED */
}

static void initdistr () {
  int **p4, *p3;
  int clubs, diamonds, hearts;

  int shape = 0;

  /* Allocate the four dimensional pointer array */

  for (clubs = 0; clubs <= 13; clubs++) {
    p4 = (int **) mycalloc ((unsigned) 14 - clubs, sizeof (*p4));
    distrbitmaps[clubs] = p4;
    for (diamonds = 0; diamonds <= 13 - clubs; diamonds++) {
      p3 = (int *) mycalloc ((unsigned) 14 - clubs - diamonds, sizeof (*p3));
      p4[diamonds] = p3;
      for (hearts = 0; hearts <= 13 - clubs - diamonds; hearts++) {
        p3[hearts] = ++shape;
      }
    }
  }
}

static int getshapenumber (int cl, int di, int ht, int sp)
{
  (void)sp;
  return distrbitmaps[cl][di][ht];
}

void  setshapebit (struct shape *s, int cl, int di, int ht, int sp)
{
  int nr = getshapenumber(cl, di, ht, sp);
  int idx = nr / 32;
  int bit = nr % 32;
  s->bits[idx] |= 1 << bit;
}

static int checkshape (int nr, struct shape *s)
{
  int idx = nr / 32;
  int bit = nr % 32;
  int r = (s->bits[idx] & (1 << bit)) != 0;
  return r;
}

static void newpack (struct pack *d) {
  int suit, rank, place;

  place = 0;
  for (suit = SUIT_CLUB; suit <= SUIT_SPADE; suit++)
    for (rank = 0; rank < 13; rank++)
      d->c[place++] = MAKECARD (suit, rank);
}

card hascard (const struct board *d, int player, card onecard) {
  return hand_has_card(d->hands[player], onecard);
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

int make_contract (char suitchar, char trickchar) {
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
  return MAKECONTRACT (suit, trick);
}

static void analyze (struct board *d, struct handstat *hsbase) {

  /* Analyze a hand.  Modified by HU to count controls and losers. */
  /* Further mod by AM to count several alternate pointcounts too  */

  int player, s;
  struct handstat *hs;

  /* for each player */
  for (player = COMPASS_NORTH; player <= COMPASS_WEST; ++player) {
    /* If the expressions in the input never mention a player
       we do not calculate his hand statistics. */
    if (use_compass[player] == 0) {

#ifdef _DEBUG
      /* In debug mode, blast the unused handstat, so that we can recognize it
         as garbage should if we accidently read from it */
      hs = hsbase + player;
      memset (hs, 0xDF, sizeof (struct handstat));
#endif /* _DEBUG_ */

      continue;
    }
    /* where are the handstats for this player? */
    hs = hsbase + player;

#ifdef _DEBUG
    /* To debug, blast it with garbage.... */
    memset (hs, 0xDF, sizeof (struct handstat));

    /* then overwrite those specific counters which need to be incremented */
    for (t = idxHcp; t < idxEnd; ++t) {
      /* clear the points for each suit */
      for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
        hs->hs_counts[t][s] = 0;
      }
      /* and the total points as well */
      hs->hs_totalcounts[t] = 0;
    }

    /* clear the length for each suit */
    for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
      hs->hs_length[s] = 0;
    }
    /* clear the total losers */
    hs->hs_totalloser = 0;
#endif /* _DEBUG_ */

    /* Initialize values summed */
    hs->hs_totalloser = 0;
    hs->hs_totalpoints = 0;
    hs->hs_totalcontrol = 0;

    /* start from the first card for this player, and walk through them all -
       use the player offset to jump to the first card.  Can't just increment
       through the deck, because we skip those players who are not part of the
       analysis */
    hand h = d->hands[player];
    for (s = SUIT_CLUB; s <= SUIT_SPADE; s++)
      hs->hs_length[s] = hand_count_cards(h & suit_masks[s]);

    for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
      assert (hs->hs_length[s] < 14);
      assert (hs->hs_length[s] >= 0);
      /* Now, using the values calculated already, load those pointcount
         values which are common enough to warrant a non array lookup */
      hs->hs_points[s] = getpc(idxHcp, h & suit_masks[s]);
      int ctrl = getpc(idxControlsInt, h & suit_masks[s]);
      hs->hs_control[s] = getpc(idxControls, h & suit_masks[s]);

      switch (hs->hs_length[s]) {
        case 0: {
          /* A void is 0 losers */
          hs->hs_loser[s] = 0;
          break;
        }
        case 1: {
          /* Singleton A 0 losers, K or Q 1 loser */
          int losers[] = {1, 1, 0};
          assert (ctrl < 3);
          hs->hs_loser[s] = losers[ctrl];
          break;
        }
        case 2: {
          /* Doubleton AK 0 losers, Ax or Kx 1, Qx 2 */
          int losers[] = {2, 1, 1, 0};
          assert (ctrl <= 3);
          hs->hs_loser[s] = losers[ctrl];
          break;
        }
        default: {
          /* Losers, first correct the number of losers */
          hs->hs_loser[s] = 3 - getpc(idxWinnersInt, h & suit_masks[s]);
          break;
        }
      }

      /* Now add the losers to the total. */
      hs->hs_totalloser += hs->hs_loser[s];
      hs->hs_totalpoints += hs->hs_points[s];
      hs->hs_totalcontrol += hs->hs_control[s];

    } /* end for each suit */

    hs->hs_bits = distrbitmaps[hs->hs_length[SUIT_CLUB]]
      [hs->hs_length[SUIT_DIAMOND]]
      [hs->hs_length[SUIT_HEART]];
  } /* end for each player */
}

static void fprintcompact (FILE * f, deal d, int ononeline, int disablecompass) {
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

static void printdeal (deal d) {
  int suit, player, rank, cards;

  printf ("%4d.\n", (nprod+1));
  
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

static void setup_bias (void) {
  int p, s;
  char err[256];
  int len[4] = {0}, lenset[4] = {0};
  for (p = 0; p < 4; p++) {
    int totset = 0;
    int tot = 0;
    for (s = 0; s < 4; s++) {
      const int predealcnt = hand_count_cards(predealt.hands[p] & suit_masks[s]);
      if (biasdeal[p][s] >= 0) {
        if (predealcnt > biasdeal[p][s]) {
          sprintf(err, "More predeal cards than bias allows for %s in %s\n", player_name[p], suit_name[s]);
          error(err);
        }
        len[s] += biasdeal[p][s];
        lenset[s]++;
        tot += biasdeal[p][s];
        totset++;
        biastotal += biasdeal[p][s] - predealcnt;
        /* Remove predealt cards from bias length */
      } else {
        len[s] += predealcnt;
        tot += predealcnt;
      }
    }
    if (tot > 13 || (totset == 4 && tot < 13)) {
      sprintf(err, "%d cards predealt for %s in %d suits\n", tot, player_name[p], totset);
      error(err);
    }
  }
  for (s = 0; s < 4; s++) {
    if (len[s] > 13 || (lenset[s] == 4 && len[s] < 13)) {
      sprintf(err, "%d cards predealt to %s in %d hands\n", len[s], suit_name[s], lenset[s]);
      error(err);
    }
  }
}

static card bias_pickcard(int start, int pos, hand mask) {
  int i;
  card temp = curpack.c[start];
  for (i = start; i < CARDS_IN_SUIT*NSUITS; i++) {
    if (hand_has_card(mask, curpack.c[i])) {
      if (pos-- == 0)
        break;
    }
  }
  assert(i < CARDS_IN_SUIT*NSUITS);
  curpack.c[start]  = curpack.c[i];
  curpack.c[i] = temp;
  return curpack.c[start];
}

static int biasfiltercounter;

static void shuffle_bias(struct board *d) {
  /* Set predealt cards */
  int p, s;
  *d = predealt;
  dealtcardsmask = 0;
  for (p = 0; p < 4; p++)
    dealtcardsmask |= predealt.hands[p];
  if (biastotal == 0)
    return;
  int totsuit[4] = {0}, totplayer[4] = {0};

  biasfiltercounter = 5200 * 1000;

  /* For each player and suit deal length bias cards */
  int predealcnt = hand_count_cards(dealtcardsmask);
  for (p = 0; p < 4; p++) {
    for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
      const int dealt = hand_count_cards(predealt.hands[p] & suit_masks[s]);
      int bias = biasdeal[p][s], b, left;
      bias -= dealt;
      if (bias <= 0)
        continue;

      totsuit[s] += bias;
      totplayer[p] += bias;

      hand suit = suit_masks[s] & ~dealtcardsmask;
      left = hand_count_cards(suit);
      for (b = 0; b < bias; b++) {
        int pos = uniform_random(left--);
        card c = bias_pickcard(predealcnt++, pos, suit);
        dealtcardsmask |= c;
        curdeal->hands[p] |= c;
      }
    }
  }
}

static int bias_filter(const struct pack *d, int pos, int idx, const int postoplayer[52]) {
  if (biastotal == 0)
    return 0;
  if (biasfiltercounter-- == 0)
    error("Bias filter called too many time for a single dealer. Likely impossible bias compination.\n");
  const int player1 = postoplayer[pos];
  const int suit1 = C_SUIT(d->c[idx]);
  const int player2 = postoplayer[idx];
  const int suit2 = C_SUIT(d->c[pos]);
  return biasdeal[player1][suit1] >= 0 || biasdeal[player2][suit2] >= 0;
}


static void setup_deal () {
}


void predeal (int player, card onecard) {

  int i;
  card c = 0;
  for (i = 0; i < 4; i++) {
    c = hand_has_card(predealt.hands[i], onecard);
    if (c == onecard)
      yyerror ("Card predealt twice");
  }
  predealt.hands[player] = hand_add_card(predealt.hands[player], onecard);
  if (hand_count_cards(predealt.hands[player]) > 13)
    yyerror("More than 13 cards for one player");
}

static void initprogram () {
  int i = 0, p, j = 0;

  struct pack temp;

  initpc();

  newpack(&temp);

  setup_bias();

  /* Move predealtcards to begin of pack */
  hand predeal = 0;
  for (p = 0; p < 4; p++)
    predeal |= predealt.hands[p];

  const int predealcnt = hand_count_cards(predeal);
  /* Move all matching cards to begin */
  for (; i < 52; i++) {
    if (hand_has_card(predeal, temp.c[i])) {
      curpack.c[i-j] = temp.c[i];
    } else {
      curpack.c[predealcnt + j] = temp.c[i];
      j++;
    }
  }

  /* Now initialize array zero52 with numbers 0..51 repeatedly. This whole
     charade is just to prevent having to do divisions. */
  j = 52 - biastotal - predealcnt;
  /* Are all cards are bias randomized? */
  if (j == 0)
    return;
  for (i = 0; i < j; i++)
    zero52[i] = i;

  for (; i + i < NRANDVALS; i += i)
    memcpy(&zero52[i], &zero52[0], i);

  /* Fill last chunk up to last full block */
  int end = NRANDVALS - (NRANDVALS % j);
  memcpy(&zero52[i], &zero52[0], end - i);
  /* Fill the last part of the array with 0xFF, just to prevent
     that 0 occurs more than 51. This is probably just for hack value */
  memset(&zero52[end], 0xFF, NRANDVALS - end);
}

static void swap2 (struct board *d, int p1, int p2) {
  /* functions to assist "simulated" shuffling with player
     swapping or loading from Ginsberg's library.dat -- AM990423 */
  hand h;
  h = d->hands[p1];
  d->hands[p1] = d->hands[p2];
  d->hands[p2] = h;
}

static FILE * find_library (const char *basename, const char *openopt) {
  static const char *prefixes[] = { "", "./", "../", "../../", "c:/", "c:/data/",
    "d:/myprojects/dealer/", "d:/arch/games/gib/", 0 };
  int i;
  char buf[256];
  FILE *result = 0;
  for (i = 0; prefixes[i]; ++i) {
    strcpy (buf, prefixes[i]);
    strcat (buf, basename);
    result = fopen (buf, openopt);
    if (result) break;
  }
  return result;
}

static int shuffle (struct board *d) {

  if (loading) {
    static FILE *lib = 0;
    if (!lib) {
      lib = find_library ("library.dat", "rb");
      if (!lib) {
        fprintf (stderr, "Cannot find or open library file\n");
        exit (-1);
      }
      fseek (lib, 26 * loadindex, SEEK_SET);
    }
retry_read:
    if (fread (&libdeal, 26, 1, lib)) {
      int i, suit, rank, pn;
      unsigned long su;
      libdeal.valid = 1;
      memset(d->hands,0,sizeof(d->hands));
      for (i = 0; i <= 4; ++i) {
        libdeal.tricks[i] = ntohs (libdeal.tricks[i]);
      }
      for (suit = 0; suit < 4; ++suit) {
        su = libdeal.suits[suit];
        su = ntohl (su);
        for (rank = 0; rank < 13; ++rank) {
          pn = su & 0x03;
          su >>= 2;
          d->hands[pn] = hand_add_card(d->hands[pn], MAKECARD(suit, 12 - rank));
          if (hand_count_cards(d->hands[pn]) > 13) {
            fprintf(stderr, "Deal %d is broken\n", ngen);
            fprintcompact(stderr, d, 1, 0);
            libdeal.valid = 0;
            ngen++;
            goto retry_read;
          }
        }
      }
      return 1;
    } else {
      libdeal.valid = 0;
      return 0;
    }
  }

  if (swapindex) {
    switch (swapindex) {
      case 1:
        swap2 (d, 1, 3);
        break;
      case 2:
        swap2 (d, 2, 3);
        break;
      case 3:
        swap2 (d, 1, 2);
        break;
      case 4:
        swap2 (d, 1, 3);
        break;
      case 5:
        swap2 (d, 2, 3);
        break;
    }
  } else {
    /* Algorithm according to Knuth. For each card exchange with a random
       other card. This is supposed to be the perfect shuffle algorithm. 
       It only depends on a valid random number generator.  */
    shuffle_bias(d);
    int dealtcnt = hand_count_cards(dealtcardsmask);
    const int predealtcnt = dealtcnt;
    int p, i = predealtcnt;
    if (biastotal > 0) {
      int postoplayer[52];
      for (p = 0; p < 4; p++) {
        int playercnt = 13 - hand_count_cards(d->hands[p]) + i;
        for (;i < playercnt; i++) {
          postoplayer[i] = p;
        }
      }
      assert(i == 52);

      for (i = predealtcnt; i < 52; i++) {
        int pos;
        do {
          pos = uniform_random_table() + predealtcnt;
        } while (bias_filter(&curpack, pos, i, postoplayer));
        card t = curpack.c[pos];
        curpack.c[pos] = curpack.c[i];
        curpack.c[i] = t;
      }

    } else {
      /* shuffle remaining cards */
      for (i = predealtcnt; i < 52; i++) {
        int pos;
        pos = uniform_random_table() + predealtcnt;
        card t = curpack.c[pos];
        curpack.c[pos] = curpack.c[i];
        curpack.c[i] = t;
      }
    }
    /* Assign remaining cards to a player */
    for (p = 0; p < 4; p++) {
      int playercnt = 13 - hand_count_cards(d->hands[p]);
      hand temp = d->hands[p];
      for (i = dealtcnt; i < playercnt + dealtcnt; i++) {
        card t = curpack.c[i];
        temp = hand_add_card(temp, t);
      }
      d->hands[p] = temp;
      dealtcnt += playercnt;
    }
  }
  if (swapping) {
    ++swapindex;
    if ((swapping == 2 && swapindex > 1) || (swapping == 3 && swapindex > 5))
      swapindex = 0;
  }
  return 1;
}

/* Specific routines for EXHAUST_MODE */

static void exh_get2players (void) {
  /* Just finds who are the 2 players for whom we make exhaustive dealing */
  int player, player_bit;
  for (player = COMPASS_NORTH, player_bit = 0; player<=COMPASS_WEST; player++) {
    if (hand_count_cards(predealt.hands[player]) != 13) {
      if (player_bit == 2) {
        /* Exhaust mode only if *exactly* 2 hands have unknown cards */
        fprintf (stderr,
         "Exhaust-mode error: more than 2 unknown hands...%s",crlf);
        exit (-1); /*NOTREACHED */
      }
      exh_player[player_bit++] = player;
    }
  }
  if (player_bit < 2) {
    /* Exhaust mode only if *exactly* 2 hands have unknown cards */
    fprintf (stderr, "Exhaust-mode error: less than 2 unknown hands...%s",crlf);
    exit (-1); /*NOTREACHED */
  }
}

static void exh_set_bit_values (int bit_pos, card onecard) {
  exh_card_at_bit[bit_pos] = onecard;
}

static void exh_map_cards (struct board *b) {
  int i;
  int bit_pos;
  hand predeal = 0;
  hand p1 = 0;
  hand p0 = 0;

  for (i = 0; i < 4; i++) {
    predeal |= predealt.hands[i];
    b->hands[i] = predealt.hands[i];
  }

  /* Fill in all cards not predealt */
  for (i = 0, bit_pos = 0; i < 52; i++) {
    card c = hand_has_card(~predeal, curpack.c[i]);
    if (c) {
      exh_set_bit_values (bit_pos, c);
      bit_pos++;
    }
  }

  int p1cnt = 13 - hand_count_cards(predealt.hands[exh_player[1]]);

  exh_vect_length = bit_pos;
  /* Set N upper bits that will be moved to low bits */
  for (i = 0; i < p1cnt; i++) {
    vectordeal |= 1 << (bit_pos - i - 1);
    p1 |= exh_card_at_bit[bit_pos - i - 1];
  }
  for (i = 1; i < bit_pos - p1cnt; i++)
    p0 |= exh_card_at_bit[i];
  /* Lowest bit goes to wrong hand so first shuffle can flip it. */
  if (bit_pos - p1cnt > 0)
    p1 |= exh_card_at_bit[0];
  else
    p0 |= exh_card_at_bit[0];

  assert(hand_count_cards((p1 | b->hands[exh_player[1]]) ^ exh_card_at_bit[0]) == 13);
  assert(hand_count_cards((p0 | b->hands[exh_player[0]]) ^ exh_card_at_bit[0]) == 13);
  b->hands[exh_player[0]] |= p0;
  b->hands[exh_player[1]] |= p1;
}

static inline void exh_print_stats (struct handstat *hs) {
  int s;
  for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
    printf ("  Suit %d: ", s);
    printf ("Len = %2d, Points = %2d\n", hs->hs_length[s], hs->hs_points[s]);
  }
  printf ("  Totalpoints: %2d\n", hs->hs_totalpoints);
}

static inline void exh_print_vector (struct handstat *hs) {
  int i, s, r;
  int onecard;
  struct handstat *hsp;

  printf ("Player %d: ", exh_player[0]);
  for (i = 0; i < exh_vect_length; i++) {
    if (!(1 & (vectordeal >> i))) {
      onecard = exh_card_at_bit[i];
      s = C_SUIT (onecard);
      r = C_RANK (onecard);
      printf ("%c%d ", ucrep[r], s);
    }
  }
  printf ("\n");
  hsp = hs + exh_player[0];
  exh_print_stats (hsp);
  printf ("Player %d: ", exh_player[1]);
  for (i = 0; i < exh_vect_length; i++) {
    if ((1 & (vectordeal >> i))) {
      onecard = exh_card_at_bit[i];
      s = C_SUIT (onecard);
      r = C_RANK (onecard);
      printf ("%c%d ", ucrep[r], s);
    }
  }
  printf ("\n");
  hsp = hs + exh_player[1];
  exh_print_stats (hsp);
}

static void exh_shuffle (int vector, int prevvect, struct board *b) {
  int p;
  hand bitstoflip = 0;

  int changed = vector ^ prevvect;

  do {
    int last = __builtin_ctz(changed);
    bitstoflip |= exh_card_at_bit[last];
    changed &= changed - 1;
  } while (changed);

  b->hands[exh_player[0]] ^= bitstoflip;
  b->hands[exh_player[1]] ^= bitstoflip;

  for (p = 0; p < 4; p++)
    assert(hand_count_cards(b->hands[p]) == 13);
}

static int bitpermutate(int vector)
{
  int nextvector, shadow;
  int bits_to_move_back = 0, move_back_count = 0;
  /* Find the lowest set bit that have zeros lower to it */
  if ((vector & 1) == 0) {
    shadow = vector - 1;
    nextvector = vector & shadow;
    shadow = vector & ~shadow;
    return nextvector | (shadow >> 1);
  }
  /* Find all set bits in lower most bits */
  shadow = vector + 1;
  nextvector = vector & shadow;
  if (nextvector == 0)
    return 0;
  bits_to_move_back = vector & ~shadow;
  /* find the lowest set bit from reminding bits */
  shadow = nextvector - 1;
  vector = nextvector;
  nextvector = vector & shadow;
  shadow = vector & ~shadow;
  /* Figure out position where to but bits back in the vector */
  move_back_count = __builtin_ctz(~bits_to_move_back);
  /* Figure out how much to shift the move back bits */
  int positiontomove = __builtin_ctz(shadow >> 1) - move_back_count;
  nextvector |= (shadow >> 1) | (bits_to_move_back << positiontomove);
  return nextvector;
}

/* End of Specific routines for EXHAUST_MODE */

static int evaltree (struct treebase *b) {
  struct tree *t = (struct tree*)b;
  switch (b->tr_type) {
    default:
      assert (0);
    case TRT_NUMBER:
      return t->tr_int1;
    case TRT_VAR:
      if (ngen != t->tr_int1) {
        t->tr_int2 = evaltree(t->tr_leaf1);
        t->tr_int1 = ngen;
      }
      return t->tr_int2;
    case TRT_AND2:
      return evaltree (t->tr_leaf1) && evaltree (t->tr_leaf2);
    case TRT_OR2:
      return evaltree (t->tr_leaf1) || evaltree (t->tr_leaf2);
    case TRT_ARPLUS:
      return evaltree (t->tr_leaf1) + evaltree (t->tr_leaf2);
    case TRT_ARMINUS:
      return evaltree (t->tr_leaf1) - evaltree (t->tr_leaf2);
    case TRT_ARTIMES:
      return evaltree (t->tr_leaf1) * evaltree (t->tr_leaf2);
    case TRT_ARDIVIDE:
      return evaltree (t->tr_leaf1) / evaltree (t->tr_leaf2);
    case TRT_ARMOD:
      return evaltree (t->tr_leaf1) % evaltree (t->tr_leaf2);
    case TRT_CMPEQ:
      return evaltree (t->tr_leaf1) == evaltree (t->tr_leaf2);
    case TRT_CMPNE:
      return evaltree (t->tr_leaf1) != evaltree (t->tr_leaf2);
    case TRT_CMPLT:
      return evaltree (t->tr_leaf1) < evaltree (t->tr_leaf2);
    case TRT_CMPLE:
      return evaltree (t->tr_leaf1) <= evaltree (t->tr_leaf2);
    case TRT_CMPGT:
      return evaltree (t->tr_leaf1) > evaltree (t->tr_leaf2);
    case TRT_CMPGE:
      return evaltree (t->tr_leaf1) >= evaltree (t->tr_leaf2);
    case TRT_NOT:
      return !evaltree (t->tr_leaf1);
    case TRT_LENGTH:      /* suit, compass */
      assert (t->tr_int1 >= SUIT_CLUB && t->tr_int1 <= SUIT_SPADE);
      assert (t->tr_int2 >= COMPASS_NORTH && t->tr_int2 <= COMPASS_WEST);
      return hs[t->tr_int2].hs_length[t->tr_int1];
    case TRT_HCPTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalpoints;
    case TRT_PT0TOTAL:      /* compass */
    case TRT_PT1TOTAL:      /* compass */
    case TRT_PT2TOTAL:      /* compass */
    case TRT_PT3TOTAL:      /* compass */
    case TRT_PT4TOTAL:      /* compass */
    case TRT_PT5TOTAL:      /* compass */
    case TRT_PT6TOTAL:      /* compass */
    case TRT_PT7TOTAL:      /* compass */
    case TRT_PT8TOTAL:      /* compass */
    case TRT_PT9TOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return getpc(idxTens + (b->tr_type - TRT_PT0TOTAL) / 2, curdeal->hands[t->tr_int1]);
    case TRT_HCP:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_points[t->tr_int2];
    case TRT_PT0:      /* compass, suit */
    case TRT_PT1:      /* compass, suit */
    case TRT_PT2:      /* compass, suit */
    case TRT_PT3:      /* compass, suit */
    case TRT_PT4:      /* compass, suit */
    case TRT_PT5:      /* compass, suit */
    case TRT_PT6:      /* compass, suit */
    case TRT_PT7:      /* compass, suit */
    case TRT_PT8:      /* compass, suit */
    case TRT_PT9:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return getpc(idxTens + (b->tr_type - TRT_PT0) / 2, curdeal->hands[t->tr_int1] & suit_masks[t->tr_int2]);
    case TRT_SHAPE:      /* compass, shapemask */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      /* assert (t->tr_int2 >= 0 && t->tr_int2 < MAXDISTR); */
      {
        struct treeshape *s = (struct treeshape *)b;
        return (checkshape(hs[s->compass].hs_bits, &s->shape));
      }
    case TRT_HASCARD:      /* compass, card */
      {
        struct treehascard *hc = (struct treehascard *)t;
        assert (hc->compass >= COMPASS_NORTH && hc->compass <= COMPASS_WEST);
        return hascard (curdeal, hc->compass, hc->c) > 0;
      }
    case TRT_LOSERTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalloser;
    case TRT_LOSER:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_loser[t->tr_int2];
    case TRT_CONTROLTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hs[t->tr_int1].hs_totalcontrol;
    case TRT_CONTROL:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hs[t->tr_int1].hs_control[t->tr_int2];
    case TRT_CCCC:
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return cccc (t->tr_int1);
    case TRT_QUALITY:
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return quality (t->tr_int1, t->tr_int2);
    case TRT_IF:
      assert (t->tr_leaf2->tr_type == TRT_THENELSE);
      {
        struct tree *leaf2 = (struct tree *)t->tr_leaf2;
        return (evaltree (t->tr_leaf1) ? evaltree (leaf2->tr_leaf1) :
              evaltree (leaf2->tr_leaf2));
      }
    case TRT_TRICKS:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= 1 + SUIT_SPADE);
      return dd (curdeal, t->tr_int1, t->tr_int2);
    case TRT_SCORE:      /* vul/non_vul, contract, tricks in leaf1 */
      assert (t->tr_int1 >= NON_VUL && t->tr_int1 <= VUL);
      return score (t->tr_int1, t->tr_int2 % 5, t->tr_int2 / 5, evaltree (t->tr_leaf1));
    case TRT_IMPS:
      return imps (evaltree (t->tr_leaf1));
    case TRT_AVG:
      return (int)(average*1000000);
    case TRT_RND:
      {
        double eval = evaltree(t->tr_leaf1);
        int random = RANDOM() & ((RAND_MAX - 1) | RAND_MAX);
        double mul = eval * random;
        double res = mul / (RAND_MAX + 1.0);
        int rv = (int)(res);
        return rv;
      }
  }
}

/* This is a macro to replace the original code :
static int interesting () {
  return evaltree (decisiontree);
}
*/
#define interesting() ((int)evaltree(decisiontree))

static void setup_action () {
  struct action *acp;

  /* Initialize all actions */
  for (acp = actionlist; acp != 0; acp = acp->ac_next) {
    switch (acp->ac_type) {
      default:
        assert (0); /*NOTREACHED */
      case ACT_EVALCONTRACT:
        initevalcontract ();
        break;
      case ACT_PRINTCOMPACT:
      case ACT_PRINTPBN:
      case ACT_PRINTEW:
      case ACT_PRINTALL:
      case ACT_PRINTONELINE:
      case ACT_PRINTES:
        break;
      case ACT_PRINT:
        deallist = (struct stored_board *) mycalloc (maxproduce, sizeof (deallist[0]));
        break;
      case ACT_AVERAGE:
        break;
      case ACT_FREQUENCY:
        acp->ac_u.acu_f.acuf_freqs = (long *) mycalloc (
           acp->ac_u.acu_f.acuf_highbnd - acp->ac_u.acu_f.acuf_lowbnd + 1,
         sizeof (long));
        break;
      case ACT_FREQUENCY2D:
        acp->ac_u.acu_f2d.acuf_freqs = (long *) mycalloc (
           (acp->ac_u.acu_f2d.acuf_highbnd_expr1 - acp->ac_u.acu_f2d.acuf_lowbnd_expr1 + 3) *
           (acp->ac_u.acu_f2d.acuf_highbnd_expr2 - acp->ac_u.acu_f2d.acuf_lowbnd_expr2 + 3),
           sizeof (long));
        break;

      }
    }
}

static void action () {
  struct action *acp;
  int expr, expr2, val1, val2, high1 = 0, high2 = 0, low1 = 0, low2 = 0;

  for (acp = actionlist; acp != 0; acp = acp->ac_next) {
    switch (acp->ac_type) {
      default:
        assert (0); /*NOTREACHED */
      case ACT_EVALCONTRACT:
        evalcontract ();
        break;
      case ACT_PRINTCOMPACT:
        printcompact (curdeal);
        if (acp->ac_expr1) {
          expr = evaltree (acp->ac_expr1);
          printf ("%d\n", expr);
        }
        break;
      case ACT_PRINTONELINE:
        printoneline (curdeal);
        if (acp->ac_expr1) {
          expr = evaltree (acp->ac_expr1);
          printf ("%d\n", expr);
        }
        break;

      case ACT_PRINTES: 
        { struct expr *pex = (struct expr *) acp->ac_expr1;
          while (pex) {
            if (pex->ex_tr) {
              expr = evaltree (pex->ex_tr);
              printf ("%d", expr);
            }
            if (pex->ex_ch) {
              printf ("%s", pex->ex_ch);
            }
            pex = pex->next;
          }
        }
        break;
      case ACT_PRINTALL:
        printdeal (curdeal);
        break;
      case ACT_PRINTEW:
        printew (curdeal);
        break;
      case ACT_PRINTPBN:
        if (! quiet)
          printpbn (nprod, curdeal);
        break;
      case ACT_PRINT:
        board_to_stored(&deallist[nprod], curdeal);
        break;
      case ACT_AVERAGE:
        acp->ac_int1 += evaltree (acp->ac_expr1);
        break;
      case ACT_FREQUENCY:
        expr = evaltree (acp->ac_expr1);
        if (expr < acp->ac_u.acu_f.acuf_lowbnd)
          acp->ac_u.acu_f.acuf_uflow++;
        else if (expr > acp->ac_u.acu_f.acuf_highbnd)
          acp->ac_u.acu_f.acuf_oflow++;
        else
          acp->ac_u.acu_f.acuf_freqs[expr - acp->ac_u.acu_f.acuf_lowbnd]++;
        break;
      case ACT_FREQUENCY2D:
        expr = evaltree (acp->ac_expr1);
        expr2 = evaltree (acp->ac_expr2);

        high1 = acp->ac_u.acu_f2d.acuf_highbnd_expr1;
        high2 = acp->ac_u.acu_f2d.acuf_highbnd_expr2;
        low1 = acp->ac_u.acu_f2d.acuf_lowbnd_expr1;
        low2 = acp->ac_u.acu_f2d.acuf_lowbnd_expr2;
        if (expr > high1)
          val1 = high1 - low1 + 2;
        else {
          val1 = expr - low1 + 1;
          if (val1 < 0) val1 = 0;
        }
        if (expr2 > high2)
          val2 = high2 - low2 + 2;
        else {
          val2 = expr2 - low2 + 1;
          if (val2 < 0) val2 = 0;
        }
        acp->ac_u.acu_f2d.acuf_freqs[(high2 - low2 + 3) * val1 + val2]++;
        break;
      }
    }
}

static void printhands (int boardno, deal dealp, int player, int nhands) {
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

static void cleanup_action () {
  struct action *acp;
  int player, i;

  for (acp = actionlist; acp != 0; acp = acp->ac_next) {
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
        showevalcontract (nprod);
        break;
      case ACT_PRINT:
        for (player = COMPASS_NORTH; player <= COMPASS_WEST; player++) {
          if (!(acp->ac_int1 & (1 << player)))
            continue;
          printf ("\n\n%s hands:\n\n\n\n", player_name[player]);
          for (i = 0; i < nprod; i += 4) {
            struct board b[4];
            int j;
            for (j = 0; j < 4 && j < nprod - i; j++)
              board_from_stored(&b[j], &deallist[i + j]);
            printhands (i, b, player, nprod - i > 4 ? 4 : nprod - i);
          }
          printf ("\f");
        }
        break;
      case ACT_AVERAGE:
        average = (double)acp->ac_int1 / nprod;
        if (acp->ac_expr2 && !evaltree(acp->ac_expr2))
          break;
        if (acp->ac_str1)
          printf ("%s: ", acp->ac_str1);
        printf ("%g\n", average);
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
        break;
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
        for (j = 1; j < (high2 - low2) + 2; j++) {
          if (toprintcol[j] != 0)
            printf (" %6d", j + low2 - 1);
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
        free(toprintcol);
        free(toprintrow);
      }
    }
  }
}

int yywrap () {
  /* Necessary if you do not have a -ll library */
  return 1;
}

static void printew (deal d) {
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

int main (int argc, char **argv) {
  int seed_provided = 0;
  extern int optind;
  extern char *optarg;
  int c;
  int errflg = 0;
  int progressmeter = 0;

  struct timeval tvstart, tvstop;

  assert(0xfe80 == bitpermutate(0xff00));
  assert(0xfd80 == bitpermutate(0xfe01));
  assert(0xfb80 == bitpermutate(0xfc03));
  assert(0x0000 == bitpermutate(0x00ff));

  verbose = 1;

  gettimeofday (&tvstart, (void *) 0);

  while ((c = getopt (argc, argv, "023ehvmqp:g:s:l:V")) != -1) {
    switch (c) {
      case '0':
      case '2':
      case '3':
        swapping = c - '0';
        break;
      case 'e':
        computing_mode = EXHAUST_MODE;
        break;
      case 'l':
        loading = 1;
        loadindex = atoi (optarg);
        break;
      case 'g':
        maxgenerate = atoi (optarg);
        break;
      case 'm':
        progressmeter ^= 1;
        break;
      case 'p':
        maxproduce = atoi (optarg);
        break;
      case 's':
        seed_provided = 1;
        seed = atol (optarg);
        if (seed == LONG_MIN || seed == LONG_MAX) {
            fprintf (stderr, "Seed overflow: seed must be between %ld and %ld\n", 
              LONG_MIN, LONG_MAX);
            exit (-1);
        }
        break;
      case 'v':
        verbose ^= 1;
        break;
      case 'q':
        quiet ^= 1;
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
    fprintf (stderr, "Usage: %s [-emv] [-s seed] [-p num] [-v num] [inputfile]\n", argv[0]);
    exit (-1);
  }
  if (optind < argc && freopen (input_file = argv[optind], "r", stdin) == NULL) {
    perror (argv[optind]);
    exit (-1);
  }
  initdistr ();
  maxdealer = -1;
  maxvuln = -1;

  yyparse ();

  /* The most suspect part of this program */
  if (!seed_provided) {
    (void) time (&seed);
  }
  SRANDOM (seed);

  initprogram ();
  if (maxgenerate == 0)
    maxgenerate = 10000000;
  if (maxproduce == 0)
    maxproduce = ((actionlist == &defaultaction) || will_print) ? 40 : maxgenerate;

  setup_action ();

  if (progressmeter)
    fprintf (stderr, "Calculating...  0%% complete\r");

  switch (computing_mode) {
    case STAT_MODE:
      setup_deal ();
      for (ngen = nprod = 0; ngen < maxgenerate && nprod < maxproduce; ngen++) {
        shuffle (curdeal);
        analyze (curdeal, hs);
        if (interesting ()) {
          action ();
          nprod++;
          if (progressmeter) {
            if ((100 * nprod / maxproduce) > 100 * (nprod - 1) / maxproduce)
              fprintf (stderr, "Calculating... %2d%% complete\r",
               100 * nprod / maxproduce);
          }
        }
      }
      break;
    case EXHAUST_MODE:
      {

        exh_get2players ();
        exh_map_cards (curdeal);
        int prevvect = vectordeal ^ 1;
        for (; vectordeal; vectordeal = bitpermutate(vectordeal)) {
          ngen++;
          exh_shuffle (vectordeal, prevvect, curdeal);
          prevvect = vectordeal;
          analyze(curdeal, hs);
          if (interesting ()) {
            /*  exh_print_vector(hs); */
            action ();
            nprod++;
          }
        }
      }
      break;
    default:
      fprintf (stderr, "Unrecognized computation mode...\n");
      exit (-1); /*NOTREACHED */
    }
  if (progressmeter)
    fprintf (stderr, "                                      \r");
  gettimeofday (&tvstop, (void *) 0);
  cleanup_action ();
  if (verbose) {
    printf ("Generated %d hands\n", ngen);
    printf ("Produced %d hands\n", nprod);
    printf ("Initial random seed %lu\n", seed);
    printf ("Time needed %8.3f sec%s", 
             (tvstop.tv_sec + tvstop.tv_usec / 1000000.0 -
             (tvstart.tv_sec + tvstart.tv_usec / 1000000.0)), crlf);
  }
  return 0;
}
