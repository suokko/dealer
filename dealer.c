#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

#include "bittwiddle.h"

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

#include <getopt.h>

#include "tree.h"
#include "pointcount.h"
#include "dealer.h"
#include "c4.h"
#include "pbn.h"
#include "genlib.h"

#include "card.h"

#include "Random/SFMT.h"

void yyerror (char *);

#define TWO_TO_THE_13 (1<<13)
#define DEFAULT_MODE STAT_MODE
#define RANDBITS 13
#define NRANDVALS (1<<RANDBITS)
#define NRANDMASK (NRANDVALS-1)

#ifdef MSDOS
  static const char * const crlf = "\r\n";
#else
  static const char * const crlf = "\n";
#endif /* MSDOS */

/* Global variables */

static int biastotal = 0;

static const char * const suit_name[] = {"Club", "Diamond", "Heart", "Spade"};
static hand dealtcardsmask;

static int swapindex = 0;

/* Various handshapes can be asked for. For every shape the user is
   interested in a number is generated. In every distribution that fits that
   shape the corresponding bit is set in the distrbitmaps 3-dimensional array.
   This makes looking up a shape a small constant cost.
*/

static unsigned char zero52[NRANDVALS];

/* Function definitions */
static int true_dd (const struct board *d, int l, int c); /* prototype */

  /* Special variables for exhaustive mode 
     Exhaustive mode created by Francois DELLACHERIE, 01-1999  */
  static int exh_verctordeal;
  static int exh_vect_length;
  /* a exh_verctordeal is a binary *word* of at most 26 bits: there are at most
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
     A suitable exh_verctordeal is a vector deal with hamming weight equal to 13. 
     For computing those vectors, we use a straightforward
     meet in the middle approach.  */
  static card exh_card_at_bit[26];
  /* exh_card_at_bit[i] is the card pointed by the bit #i */
  static unsigned char exh_player[2];
  /* the two players that have unknown cards */

struct globals *gp;

static inline uint16_t random16()
{
  if (gp->rngstate.idx == sizeof(gp->rngstate.random)/sizeof(gp->rngstate.random[0])) {
    gp->rngstate.idx = 0;
    sfmt_fill_array64(&gp->rngstate.sfmt, (uint64_t *)gp->rngstate.random,
        sizeof(gp->rngstate.random)/sizeof(uint64_t));
  }
  return gp->rngstate.random[gp->rngstate.idx++];
}

static inline uint32_t random32()
{
  uint32_t r1 = random16();
  uint32_t r2 = random16();
  return (r2 << 16) | r1;
}

static uint16_t uniform_random(uint16_t max) {
  uint16_t rnd;
  uint16_t val;
  do {
    val = random16();
    rnd = val % max;
  } while (max - rnd > UINT16_MAX - val);
  return rnd;
}

static uint16_t uniform_random_table() {
  uint16_t rnd;
  do {
    rnd = random16();
    rnd = zero52[rnd & NRANDMASK];
  } while (rnd == 0xFF);
  return rnd;
}

static void initevalcontract () {
  int i, j, k;
  for (i = 0; i < 2; i++)
    for (j = 0; j < 5; j++)
      for (k = 0; k < 14; k++)
        gp->results[i][j][k] = 0;
}

static int dd (const struct board *d, int l, int c) {
  /* results-cached version of dd() */
  /* the dd cache, and the gp->ngen it refers to */
  static int cached_ngen = -1;
  static char cached_tricks[4][5];

  /* invalidate cache if it's another deal */
  if (gp->ngen != cached_ngen) {
      memset (cached_tricks, -1, sizeof (cached_tricks));
      cached_ngen = gp->ngen;
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
  if (gp->loading && libdeal.valid) {
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
    sprintf (cmd, "ddd %s -trumps=%c -leader=%c -sol=1 | tail -n1 > %s;", tn1, "cdhsn"[c], "eswn"[l], tn2);
    r = system (cmd);
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
    gp->results[1][s][dd (&gp->curboard, 2, s)]++;      /* south declarer */ 
    gp->results[0][s][dd (&gp->curboard, 0, s)]++;      /* north declarer */
  }
}

static int checkshape (int nr, struct shape *s)
{
  int idx = nr / 32;
  int bit = nr % 32;
  int r = (s->bits[idx] & (1 << bit)) != 0;
  return r;
}

static card statichascard (const struct board *d, int player, card onecard) {
  return hand_has_card(d->hands[player], onecard);
}

/*
 * Calculate and cache hcp for a player
 *
 * @param d       Current board to analyze
 * @param hsbase  The base of handstat cache
 * @param compass The player to calculate stats for
 * @param suit    The suit to calculate or 4 for complete hand
 */
static int hcp (const struct board *d, struct handstat *hsbase, int compass, int suit)
{
      assert (compass >= COMPASS_NORTH && compass <= COMPASS_WEST);
      assert ((suit >= SUIT_CLUB && suit <= SUIT_SPADE) || suit == 4);

      struct handstat *hs = &hsbase[compass];

      /* Cache interleaves generation and actual value */
      if (hs->hs_points[suit*2] != gp->ngen) {
        /* No cached value so has to calculate it */
        const hand h = suit == 4 ? d->hands[compass] :
                                   d->hands[compass] & suit_masks[suit];

        hs->hs_points[suit*2]   = gp->ngen;
        hs->hs_points[suit*2+1] = getpc(idxHcp, h);
      }
      return hs->hs_points[suit*2+1];
}

static int control (const struct board *d, struct handstat *hsbase, int compass, int suit)
{
      assert (compass >= COMPASS_NORTH && compass <= COMPASS_WEST);
      assert ((suit >= SUIT_CLUB && suit <= SUIT_SPADE) || suit == 4);

      struct handstat *hs = &hsbase[compass];

      if (hs->hs_control[suit*2] != gp->ngen) {
        /* No cached value so has to calculate it */
        const hand h = suit == 4 ? d->hands[compass] :
                                   d->hands[compass] & suit_masks[suit];

        hs->hs_control[suit*2]   = gp->ngen;
        hs->hs_control[suit*2+1] = getpc(idxControls, h);
      }
      return hs->hs_control[suit*2+1];
}

/**
 * Assume that popcnt is fast enough operation not needing caching.
 */
static inline int staticsuitlength (const struct board *d, struct handstat *hsbase, int compass, int suit)
{
  assert (suit >= SUIT_CLUB && suit <= SUIT_SPADE);
  assert(compass >= COMPASS_NORTH && compass <= COMPASS_WEST);
  (void)hsbase;
  hand h = d->hands[compass] & suit_masks[suit];
  return hand_count_cards(h);
}

int suitlength (const struct board *d, struct handstat *hsbase, int compass, int suit)
{
  return staticsuitlength(d, hsbase, compass, suit);
}

/**
 * The nit position in shape bitmap for this board is calculated from length of suits.
 */
static int distrbit (const struct board *d, struct handstat *hsbase, int compass)
{
  assert(compass >= COMPASS_NORTH && compass <= COMPASS_WEST);
  struct handstat *hs = &hsbase[compass];

  if (hs->hs_bits[0] != gp->ngen) {
    hs->hs_bits[0] = gp->ngen;
    hs->hs_bits[1] = getshapenumber(staticsuitlength(d, hsbase, compass, SUIT_CLUB),
      staticsuitlength(d, hsbase, compass, SUIT_DIAMOND),
      staticsuitlength(d, hsbase, compass, SUIT_HEART));
  }
  return hs->hs_bits[1];
}
static int loser (const struct board *d, struct handstat *hsbase, int compass, int suit)
{
      assert (compass >= COMPASS_NORTH && compass <= COMPASS_WEST);
      assert ((suit >= SUIT_CLUB && suit <= SUIT_SPADE) || suit == 4);

      struct handstat *hs = &hsbase[compass];

      if (hs->hs_loser[suit*2] != gp->ngen) {
        /* No cached value so has to calculate it */
        hs->hs_loser[suit*2] = gp->ngen;
        if (suit == 4) {
          hs->hs_loser[4*2+1] = 0;
          for (suit = SUIT_CLUB; suit <= SUIT_SPADE; suit++)
            hs->hs_loser[4*2+1] += loser(d, hsbase, compass, suit);
        } else {
          const int length = staticsuitlength(d, hsbase, compass, suit);
          const hand h = d->hands[compass] & suit_masks[suit];
          int control = getpc(idxControlsInt, h);
          int winner = getpc(idxWinnersInt, h);

          switch (length) {
          case 0:
            /* A void is 0 losers */
            hs->hs_loser[suit*2+1] = 0;
            break;
          case 1:
            {
              /* Singleton A 0 losers, K or Q 1 loser */
              int losers[] = {1, 1, 0};
              assert (control < 3);
              hs->hs_loser[suit*2+1] = losers[control];
              break;
            }
          case 2:
            {
              /* Doubleton AK 0 losers, Ax or Kx 1, Qx 2 */
              int losers[] = {2, 1, 1, 0};
              assert (control <= 3);
              hs->hs_loser[suit*2+1] = losers[control];
              break;
            }
          default:
            /* Losers, first correct the number of losers */
            hs->hs_loser[suit*2+1] = 3 - winner;
            break;
          }
        }
      }
      return hs->hs_loser[suit*2+1];
}

static void setup_bias (void) {
  int p, s;
  char err[256];
  int len[4] = {0}, lenset[4] = {0};
  for (p = 0; p < 4; p++) {
    int totset = 0;
    int tot = 0;
    for (s = 0; s < 4; s++) {
      const int predealcnt = hand_count_cards(gp->predealt.hands[p] & suit_masks[s]);
      if (gp->biasdeal[p][s] >= 0) {
        if (predealcnt > gp->biasdeal[p][s]) {
          sprintf(err, "More predeal cards than bias allows for %s in %s\n", player_name[p], suit_name[s]);
          error(err);
        }
        len[s] += gp->biasdeal[p][s];
        lenset[s]++;
        tot += gp->biasdeal[p][s];
        totset++;
        biastotal += gp->biasdeal[p][s] - predealcnt;
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
  card temp = gp->curpack.c[start];
  for (i = start; i < CARDS_IN_SUIT*NSUITS; i++) {
    if (hand_has_card(mask, gp->curpack.c[i])) {
      if (pos-- == 0)
        break;
    }
  }
  assert(i < CARDS_IN_SUIT*NSUITS);
  gp->curpack.c[start]  = gp->curpack.c[i];
  gp->curpack.c[i] = temp;
  return gp->curpack.c[start];
}

static int biasfiltercounter;

static void shuffle_bias(struct board *d) {
  /* Set predealt cards */
  int p, s;
  *d = gp->predealt;
  dealtcardsmask = 0;
  for (p = 0; p < 4; p++)
    dealtcardsmask |= gp->predealt.hands[p];
  if (biastotal == 0)
    return;
  int totsuit[4] = {0}, totplayer[4] = {0};

  biasfiltercounter = 5200 * 1000;

  /* For each player and suit deal length bias cards */
  int predealcnt = hand_count_cards(dealtcardsmask);
  for (p = 0; p < 4; p++) {
    for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
      const int dealt = hand_count_cards(gp->predealt.hands[p] & suit_masks[s]);
      int bias = gp->biasdeal[p][s], b, left;
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
        gp->curboard.hands[p] |= c;
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
  return gp->biasdeal[player1][suit1] >= 0 || gp->biasdeal[player2][suit2] >= 0;
}

static void initprogram (const char *initialpack) {
  int i = 0, p, j = 0;

  struct pack temp;

  initpc();

  /* clear the handstat cache */
  memset(hs, -1, sizeof(hs));

  newpack(&temp, initialpack);

  setup_bias();

  /* Move predealtcards to begin of pack */
  hand predeal = 0;
  for (p = 0; p < 4; p++)
    predeal |= gp->predealt.hands[p];

  const int predealcnt = hand_count_cards(predeal);
  /* Move all matching cards to begin */
  for (; i < 52; i++) {
    if (hand_has_card(predeal, temp.c[i])) {
      gp->curpack.c[i-j] = temp.c[i];
    } else {
      gp->curpack.c[predealcnt + j] = temp.c[i];
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

  if (gp->loading) {
    static FILE *lib = 0;
    if (!lib) {
      lib = find_library ("library.dat", "rb");
      if (!lib) {
        fprintf (stderr, "Cannot find or open library file\n");
        exit (-1);
      }
      fseek (lib, 26 * gp->loadindex, SEEK_SET);
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
            fprintf(stderr, "Deal %d is broken\n", gp->ngen);
            fprintcompact(stderr, d, 1, 0);
            libdeal.valid = 0;
            gp->ngen++;
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
        } while (bias_filter(&gp->curpack, pos, i, postoplayer));
        card t = gp->curpack.c[pos];
        gp->curpack.c[pos] = gp->curpack.c[i];
        gp->curpack.c[i] = t;
      }

    } else {
      /* shuffle remaining cards */
      for (i = predealtcnt; i < 52; i++) {
        int pos;
        pos = uniform_random_table() + predealtcnt;
        card t = gp->curpack.c[pos];
        gp->curpack.c[pos] = gp->curpack.c[i];
        gp->curpack.c[i] = t;
      }
    }
    /* Assign remaining cards to a player */
    for (p = 0; p < 4; p++) {
      int playercnt = 13 - hand_count_cards(d->hands[p]);
      hand temp = d->hands[p];
      for (i = dealtcnt; i < playercnt + dealtcnt; i++) {
        card t = gp->curpack.c[i];
        temp = hand_add_card(temp, t);
      }
      d->hands[p] = temp;
      dealtcnt += playercnt;
    }
  }
  if (gp->swapping) {
    ++swapindex;
    if ((gp->swapping == 2 && swapindex > 1) || (gp->swapping == 3 && swapindex > 5))
      swapindex = 0;
  }
  return 1;
}

/* Specific routines for EXHAUST_MODE */

static void exh_get2players (void) {
  /* Just finds who are the 2 players for whom we make exhaustive dealing */
  int player, player_bit;
  for (player = COMPASS_NORTH, player_bit = 0; player<=COMPASS_WEST; player++) {
    if (hand_count_cards(gp->predealt.hands[player]) != 13) {
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
    predeal |= gp->predealt.hands[i];
    b->hands[i] = gp->predealt.hands[i];
  }

  /* Fill in all cards not predealt */
  for (i = 0, bit_pos = 0; i < 52; i++) {
    card c = hand_has_card(~predeal, gp->curpack.c[i]);
    if (c) {
      exh_set_bit_values (bit_pos, c);
      bit_pos++;
    }
  }

  int p1cnt = 13 - hand_count_cards(gp->predealt.hands[exh_player[1]]);

  exh_vect_length = bit_pos;
  /* Set N lower bits that will be moved to high bits */
  for (i = 1; i < p1cnt; i++) {
    exh_verctordeal |= 1 << i;
    p1 |= exh_card_at_bit[i];
  }
  for (i = 0; i < bit_pos - p1cnt; i++)
    p0 |= exh_card_at_bit[i + p1cnt];
  /* Lowest bit goes to wrong hand so first shuffle can flip it. */
  if (p1cnt > 0) {
    exh_verctordeal |= 1 << 0;
    p0 |= exh_card_at_bit[0];
  } else {
    p1 |= exh_card_at_bit[0];
  }

  assert(hand_count_cards(((p1 | b->hands[exh_player[1]])) ^ exh_card_at_bit[0]) == 13);
  assert(hand_count_cards(((p0 | b->hands[exh_player[0]])) ^ exh_card_at_bit[0]) == 13);
  b->hands[exh_player[0]] |= p0;
  b->hands[exh_player[1]] |= p1;
}

static inline void exh_print_stats (struct handstat *hs, int hs_length[4]) {
  int s;
  for (s = SUIT_CLUB; s <= SUIT_SPADE; s++) {
    printf ("  Suit %d: ", s);
    printf ("Len = %2d, Points = %2d\n", hs_length[s], hs->hs_points[s*2+1]);
  }
  printf ("  Totalpoints: %2d\n", hs->hs_points[s*2+1]);
}

static inline void exh_print_vector (struct handstat *hs) {
  int i, s, r;
  int onecard;
  struct handstat *hsp;
  int hs_length[4];

  printf ("Player %d: ", exh_player[0]);
  for (i = 0; i < exh_vect_length; i++) {
    if (!(1 & (exh_verctordeal >> i))) {
      onecard = exh_card_at_bit[i];
      s = C_SUIT (onecard);
      r = C_RANK (onecard);
      printf ("%c%d ", ucrep[r], s);
    }
  }
  printf ("\n");
  hsp = hs + exh_player[0];
  for (s = SUIT_CLUB; s <= NSUITS; s++) {
    hs_length[s] = staticsuitlength(&gp->curboard, hsp, exh_player[0], s);
    hcp(&gp->curboard, hsp, exh_player[0],  s);
    hcp(&gp->curboard, hsp, exh_player[1],  s);
  }
  exh_print_stats (hsp, hs_length);
  printf ("Player %d: ", exh_player[1]);
  for (i = 0; i < exh_vect_length; i++) {
    if ((1 & (exh_verctordeal >> i))) {
      onecard = exh_card_at_bit[i];
      s = C_SUIT (onecard);
      r = C_RANK (onecard);
      printf ("%c%d ", ucrep[r], s);
    }
  }
  printf ("\n");
  hsp = hs + exh_player[1];
  for (s = SUIT_CLUB; s <= NSUITS; s++)
    hs_length[s] = staticsuitlength(&gp->curboard, hsp, exh_player[1], s);
  exh_print_stats (hsp, hs_length);
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
  /* set all lowest zeros to one */
  int bottomones = vector | (vector - 1);
  /* Set the lowest zero bit in original */
  int nextvector = bottomones + 1;
  /* flip bits */
  int moveback = ~bottomones;
  /* select the lowest zero bit in bottomones */
  moveback &= 0 - moveback;
  /* set to ones all low bits that are ones in bottomones */
  moveback--;
  /* move back set bits the number of trailing zeros plus one in original
   * number. This removes the lowest set bit in original vector and moves
   * all ones next to it to bottom.
   */
  moveback >>= __builtin_ctz(vector) + 1;
  /* combine the result vector to get next permutation */
  return nextvector | moveback;
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
      if (gp->ngen != t->tr_int1) {
        t->tr_int2 = evaltree(t->tr_leaf1);
        t->tr_int1 = gp->ngen;
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
      return staticsuitlength(&gp->curboard, hs, t->tr_int2, t->tr_int1);
    case TRT_HCPTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return hcp(&gp->curboard, hs, t->tr_int1, 4);
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
      return getpc(idxTens + (b->tr_type - TRT_PT0TOTAL) / 2, gp->curboard.hands[t->tr_int1]);
    case TRT_HCP:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return hcp(&gp->curboard, hs, t->tr_int1, t->tr_int2);
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
      return getpc(idxTens + (b->tr_type - TRT_PT0) / 2, gp->curboard.hands[t->tr_int1] & suit_masks[t->tr_int2]);
    case TRT_SHAPE:      /* compass, shapemask */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      {
        struct treeshape *s = (struct treeshape *)b;
        return (checkshape(distrbit(&gp->curboard, hs, s->compass), &s->shape));
      }
    case TRT_HASCARD:      /* compass, card */
      {
        struct treehascard *hc = (struct treehascard *)t;
        assert (hc->compass >= COMPASS_NORTH && hc->compass <= COMPASS_WEST);
        return statichascard (&gp->curboard, hc->compass, hc->c) > 0;
      }
    case TRT_LOSERTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return loser(&gp->curboard, hs, t->tr_int1, 4);
    case TRT_LOSER:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return loser(&gp->curboard, hs, t->tr_int1, t->tr_int2);
    case TRT_CONTROLTOTAL:      /* compass */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      return control(&gp->curboard, hs, t->tr_int1, 4);
    case TRT_CONTROL:      /* compass, suit */
      assert (t->tr_int1 >= COMPASS_NORTH && t->tr_int1 <= COMPASS_WEST);
      assert (t->tr_int2 >= SUIT_CLUB && t->tr_int2 <= SUIT_SPADE);
      return control(&gp->curboard, hs, t->tr_int1, t->tr_int2);
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
      return dd (&gp->curboard, t->tr_int1, t->tr_int2);
    case TRT_SCORE:      /* vul/non_vul, contract, tricks in leaf1 */
      assert (t->tr_int1 >= NON_VUL && t->tr_int1 <= VUL);
      int cntr = t->tr_int2 & (64 - 1);
      return score (t->tr_int1, cntr % 5, cntr / 5, t->tr_int2 & 64, evaltree (t->tr_leaf1));
    case TRT_IMPS:
      return imps (evaltree (t->tr_leaf1));
    case TRT_AVG:
      return (int)(gptr->average*1000000);
    case TRT_RND:
      {
        double eval = evaltree(t->tr_leaf1);
        unsigned random = random32();
        double mul = eval * random;
        double res = mul / (UINT_MAX + 1.0);
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
#define interesting() ((int)evaltree(gp->decisiontree))

static void setup_action () {
  struct action *acp;

  /* Initialize all actions */
  for (acp = gp->actionlist; acp != 0; acp = acp->ac_next) {
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
        gp->deallist = (struct stored_board *) mycalloc (gp->maxproduce, sizeof (gp->deallist[0]));
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

  for (acp = gp->actionlist; acp != 0; acp = acp->ac_next) {
    switch (acp->ac_type) {
      default:
        assert (0); /*NOTREACHED */
      case ACT_EVALCONTRACT:
        evalcontract ();
        break;
      case ACT_PRINTCOMPACT:
        printcompact (&gp->curboard);
        if (acp->ac_expr1) {
          expr = evaltree (acp->ac_expr1);
          printf ("%d\n", expr);
        }
        break;
      case ACT_PRINTONELINE:
        printoneline (&gp->curboard);
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
        printdeal (&gp->curboard);
        break;
      case ACT_PRINTEW:
        printew (&gp->curboard);
        break;
      case ACT_PRINTPBN:
        if (! gp->quiet)
          printpbn (gp->nprod, &gp->curboard);
        break;
      case ACT_PRINT:
        board_to_stored(&gp->deallist[gp->nprod], &gp->curboard);
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

int DEFUN(deal_main) (struct globals *g) {

  assert(0x107f == bitpermutate(0x0ff0));
  assert(0xfe02 == bitpermutate(0xfe01));
  assert(0xf061 == bitpermutate(0xf058));
  assert(0x017f == bitpermutate(0x00ff));

  assert(0 == popcount(0x0000));
  assert(1 == popcount(0x0100));
  assert(2 == popcount(0x1001));
  assert(16 == popcount(0xffff));
  assert(0 == popcountl(0x0000));
  assert(1 == popcountl(0x0100));
  assert(2 == popcountl(0x1001));
  assert(16 == popcountl(0xffff));
  assert(0 == popcountll(0x0000));
  assert(1 == popcountll(0x0100));
  assert(2 == popcountll(0x1001));
  assert(16 == popcountll(0xffff));

  gp = g;
  evaltreefunc = evaltree;
  hascard = statichascard;

  /* The most suspect part of this program */
  sfmt_init_gen_rand(&g->rngstate.sfmt, g->seed);
  g->rngstate.idx = sizeof(g->rngstate.random)/sizeof(g->rngstate.random[0]);
  assert(sfmt_get_min_array_size64(&g->rngstate.sfmt) < (int)(sizeof(g->rngstate.random)/sizeof(uint64_t)));

  initprogram (g->initialpack);

  setup_action ();

  if (g->progressmeter)
    fprintf (stderr, "Calculating...  0%% complete\r");

  switch (gp->computing_mode) {
    case STAT_MODE:
      for (gp->ngen = gp->nprod = 0; gp->ngen < g->maxgenerate && gp->nprod < g->maxproduce; gp->ngen++) {
        if (!shuffle (&g->curboard))
          break;
        if (interesting ()) {
          action ();
          gp->nprod++;
          if (g->progressmeter) {
            if ((100 * gp->nprod / g->maxproduce) > 100 * (gp->nprod - 1) / g->maxproduce)
              fprintf (stderr, "Calculating... %2d%% complete\r",
               100 * gp->nprod / g->maxproduce);
          }
        }
      }
      break;
    case EXHAUST_MODE:
      {

        exh_get2players ();
        exh_map_cards (&g->curboard);
        int prevvect = exh_verctordeal ^ 1;
        for (; exh_verctordeal < (1 << exh_vect_length); exh_verctordeal = bitpermutate(exh_verctordeal)) {
          gp->ngen++;
          exh_shuffle (exh_verctordeal, prevvect, &g->curboard);
          prevvect = exh_verctordeal;
          if (interesting ()) {
            /*  exh_print_vector(hs); */
            action ();
            gp->nprod++;
          }
        }
      }
      break;
    default:
      fprintf (stderr, "Unrecognized computation mode...\n");
      exit (-1); /*NOTREACHED */
    }
  if (g->progressmeter)
    fprintf (stderr, "                                      \r");
  return 0;
}
