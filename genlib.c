#include "genlib.h"

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <limits.h>

static void usage(const char *name, int exitcode, const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  fprintf(stderr, "\nUsage: %s [-hqv] [-o|-a filename] [-g deals] [-s seed]\n"
      "\n"
      "\t-h\tPrint this message\n"
      "\t-o <file>\tSelect output file <stdout>\n"
      "\t-a <file>\tAppend output to the file\n"
      "\t-g <number>\tSet number of deals to generate <100>\n"
      "\t-s <number>\tSelect output file <current time>\n"
      "\t-q\tBe quiet\n"
      "\t-v\tBe verbose\n",
      name);
  exit(exitcode);
}

static char ucrep[14] = "23456789TJQKA";

static void parsedeal(struct tagLibdeal *d,
                      const char *north,
                      const char *east,
                      const char *south,
                      const char *west,
                      const char *result,
                      int verbosity)
{
  int p, cont;
  const char *player[4];
  const char contracts[] = "NCDHS";
  const int leader_to_player[] = {2, 1, 0, 3};
  const int contract_to_suit[] = {0, 4, 3, 2, 1};

  player[0] = north;
  player[1] = east;
  player[2] = south;
  player[3] = west;

  for (cont = 0; cont < 5; cont++) {
    for (p = 0; p < 4; p++) {
      char val[] = "1";
      long tricksew;
      int suit = contract_to_suit[cont];
      val[0] = result[cont*4 + p];
      tricksew = 13 - strtol(val, NULL, 16);
      d->tricks[suit] |= tricksew << (leader_to_player[p]*4);
      if (verbosity >= 3)
        fprintf(stderr, "contract %c makes %ld tricks for ew by %d -> %x\n",
            contracts[suit], tricksew, leader_to_player[p],
            d->tricks[suit]);
    }
  }

  for (p = 1; p < 4; p++) {
    int c;
    int suit = 0, rank;
    for (c = 0; c < 16; c++) {
      if (player[p][c] == '.') {
        suit++;
        continue;
      }

      rank = 12 - (strchr(ucrep, player[p][c]) - ucrep);
      d->suits[3 - suit] |= p << (rank * 2);
      if (verbosity >= 3)
        fprintf(stderr, "card %d in suit %d belongs to %d -> %x\n",
            rank, 3 - suit, p, d->suits[3 - suit]);
    }
  }

  for (p = 0; p < 4; p++)
    d->suits[p] = htonl(d->suits[p]);
  for (p = 0; p < 5; p++)
    d->tricks[p] = htons(d->tricks[p]);
}

int main(int argc, char * const argv[])
{
  char *output = NULL;
  char ddscmd[256];
  char buffer[1024];
  unsigned long gen = 100, seed;
  char c;
  FILE *in, *out;
  int verbosity = 1;
  char mode[] = "w";
  const char * const filenamestart = "output written to file ";
  const char * const skipdeals = "  deal=";

  char north[17], east[17], south[17], west[17], results[21];
  long deal;

  struct timespec tp;

  clock_gettime(CLOCK_REALTIME, &tp);

  seed = tp.tv_nsec + tp.tv_sec * 1000 * 1000 * 1000;
  while ((c = getopt(argc, argv, "ho:s:g:qv")) != -1) {
    switch (c) {
    default:
      usage(argv[0], 1, "ERROR: Unknown parameter %c\n", c);
      break;
    case 'h':
    case '?':
      usage(argv[0], 0, NULL);
      break;
    case 'g':
      gen = atoi(optarg);
      if (gen > INT_MAX)
        usage(argv[0], 2, "ERROR: Generate %lu is over max %d\n", gen, INT_MAX);
      break;
    case 'q':
      verbosity = 0;
      break;
    case 'v':
      verbosity++;
      break;
    case 's':
      seed = atoi(optarg);
      break;
    case 'a':
      mode[0] = 'a';
    case 'o':
      output = strdup(optarg);
      break;
    }
  }

  seed ^= seed >> 31;
  seed &= (uint32_t)~0;

  if (verbosity >= 1)
    fprintf(stderr, "Generating %ld deals with %ld seed\n", gen, seed);

  out = output ? fopen(output, mode) : stdout;

  if (!out) {
    perror("Can't open output file");
    return 10;
  }

  sprintf(ddscmd, "dds -genseed=%ld -gen=%ld -gentricks=20", seed, gen);

  if (verbosity >= 2)
    fprintf(stderr, "Running '%s'\n", ddscmd);

  in = popen(ddscmd, "r");

  if (!in) {
    perror("Can't run the dds");
    return 20;
  }

  while (fgets(buffer, sizeof buffer, in)) {
    if (strncmp(buffer, skipdeals, strlen(skipdeals)) == 0)
      continue;
    if (strncmp(buffer, filenamestart, strlen(filenamestart)) == 0) {
      const char * const outfile = buffer + strlen(filenamestart);
      strchr(outfile, '\n')[0] = '\0';
      if (verbosity >= 2)
        fprintf(stderr, "Removing '%s'\n", outfile);
      remove(outfile);
      continue;
    }
    if (sscanf(buffer, "%ld %16s %16s %16s %16s:%20s",
          &deal, west, north, east, south, results) == 6) {
      struct tagLibdeal libdeal = {{0}};
      parsedeal(&libdeal, north, east, south, west, results, verbosity);

      if (fwrite(&libdeal, 26, 1, out) <= 0) {
        fprintf(stderr, "Failed to write libdeal entry %ld\n", deal);
        return 30;
      }

      if (verbosity >= 1 && 100 * deal / gen > 100 * (deal - 1) / gen)
        fprintf(stderr, "Completed %ld/%ld deals. Progress is %ld %%\r",
            deal, gen, 100 * deal / gen);
      if (verbosity >= 1 && deal == gen)
        fprintf(stderr, "\n");
      continue;
    }
  }

  pclose(in);
  fclose(out);
  return 0;
}
