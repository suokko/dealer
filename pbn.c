/*  This routine prints a hand in PBN format 
    Added by Henk Uijterwaal, Feb 1999            */

#include <stdio.h>

#include <time.h>

#include "tree.h"
#include "dealer.h"
#include "pbn.h"

extern long seed;
extern char* input_file;

int printpbn (int board, deal d) {

  /* Symbols for the cards */
  char representation[] = "23456789TJQKA";
  /* Mnemonics for vulnerability and dealer */
  char *vulner_name[] = { "None",  "NS",   "EW",    "All"  };
  char *dealer_name[] = { "N",     "E",    "S",     "W"    };
  /* Who's vulnerable on which boards */
  int board_vul[] = { 0,1,2,3, 1,2,3,0, 2,3,0,1, 3,0,1,2 };

  /* Local variables */
  time_t timet;
  size_t len;
  char timearray[12];
  int player, suit, rank;

  /* Suppress verbose output unless we really want it */
  verbose ^= 1;

  printf ("[Event \"Hand simulated by dealer with file %s, seed %lu\"]\n",
  input_file, seed);

  printf ("[Site \"-\"]\n");

  /* Today's date */
  timet = time(&timet);
  len = strftime (timearray, 12, "%Y.%m.%d", localtime(&timet));
  printf ("[Date \"%s\"]\n", timearray);

  printf ("[Board \"%d\"]\n", board+1);
 
  /* Blank tags for the players */ 
  printf ("[West \"-\"]\n");
  printf ("[North \"-\"]\n");
  printf ("[East \"-\"]\n");
  printf ("[South \"-\"]\n");
   
  /* Dealer, rotates unless set by the user */
  if ((maxdealer < 0) || (maxdealer > 3)) { 
     printf ("[Dealer \"%s\"]\n", dealer_name[board%4]);
  } else {
    printf ("[Dealer \"%s\"]\n", dealer_name[maxdealer]);
  }
  
  /* Vulnerability, rotates unless set by the user */
  if ((maxvuln < 0) || (maxvuln > 3)) {
     printf ("[Vulnerable \"%s\"]\n", vulner_name[board_vul[board%16]]);
  } else {
     printf ("[Vulnerable \"%s\"]\n", vulner_name[maxvuln]);
  }

  /* Print the cards */
  printf ("[Deal \"N:");
  for (player=COMPASS_NORTH; player<=COMPASS_WEST; player++) {
     for (suit = SUIT_SPADE; suit>= SUIT_CLUB; suit--) {
        for (rank=12; rank >= 0; rank--) {
          if (HAS_CARD(d, player, MAKECARD(suit,rank))) {
              printf ("%c", representation[rank]);
           }
        }
        if (suit > SUIT_CLUB) { printf (".");}
     }
     if (player < COMPASS_WEST) {printf (" ");}
  }
  printf ("\"]\n");

  /* Blank tags for declarer etc */
  printf ("[Declarer \"?\"]\n");
  printf ("[Contract \"?\"]\n");
  printf ("[Result \"?\"]\n");
  printf ("\n");

  return 0;
}
