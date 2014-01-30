#include "genlib.h"

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <limits.h>
#include <fcntl.h>

#include <assert.h>

#include <sys/select.h>
#include <errno.h>

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
      "\t-c <number>\tSet number of threads to use <a thread per core>\n"
      "\t-q\tBe quiet\n"
      "\t-v\tBe verbose\n",
      name);
  exit(exitcode);
}

struct process {
  FILE *f;
  int fd;
  char *partial;
};

static unsigned long generate = 0;
static unsigned long scheduled = 0;
static int linefeed = 0;

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
      if (verbosity >= 3) {
        fprintf(stderr, "%scontract %c makes %ld tricks for ew by %d -> %x\n", (linefeed ? "\n": ""),
            contracts[suit], tricksew, leader_to_player[p],
            d->tricks[suit]);
        linefeed = 0;
      }
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
        fprintf(stderr, "%scard %d in suit %d belongs to %d -> %x\n", (linefeed ? "\n": ""),
            rank, 3 - suit, p, d->suits[3 - suit]);
    }
  }

  for (p = 0; p < 4; p++)
    d->suits[p] = htonl(d->suits[p]);
  for (p = 0; p < 5; p++)
    d->tricks[p] = htons(d->tricks[p]);
}

static void parseLine(char *buffer, FILE *out, struct process *p, unsigned long gen, int verbosity)
{
  const char * const filenamestart = "output written to file ";
  const char * const skipdeals = "  deal=";

  char north[17], east[17], south[17], west[17], results[21];
  long deal;

  if (!strchr(buffer, '\n')) {
    assert(p->partial == NULL);
    p->partial = strdup(buffer);
    return;
  }

  if (p->partial) {
    char *end = memchr(p->partial, '\0', 256);
    long len = end - p->partial;
    memmove(buffer + len, buffer, (char *)memchr(buffer, '\0', 256) - buffer);
    memcpy(buffer, p->partial, len);
    free(p->partial);
    p->partial = NULL;
  }

  if (strncmp(buffer, skipdeals, strlen(skipdeals)) == 0)
    return;
  if (strncmp(buffer, filenamestart, strlen(filenamestart)) == 0) {
    const char * const outfile = buffer + strlen(filenamestart);
    strchr(outfile, '\n')[0] = '\0';
    if (verbosity >= 2) {
      fprintf(stderr, "%sRemoving '%s'\n", (linefeed ? "\n": ""), outfile);
      linefeed = 0;
    }
    remove(outfile);
    return;
  }
  if (sscanf(buffer, "%ld %16s %16s %16s %16s:%20s",
        &deal, west, north, east, south, results) == 6) {
    struct tagLibdeal libdeal = {{0}};
    parsedeal(&libdeal, north, east, south, west, results, verbosity);

    if (fwrite(&libdeal, 26, 1, out) <= 0) {
      fprintf(stderr, "%sFailed to write libdeal entry %ld\n", (linefeed ? "\n": ""), deal);
      exit(30);
    }

    generate++;

    if (verbosity >= 1 &&
        (100 * generate / gen > 100 * (generate - 1) / gen ||
         generate / 100 > (generate - 1) / 100)) { 
      fprintf(stderr, "Completed %ld/%ld deals (%ld%%)\r",
          generate, gen, 100 * generate / gen);
      linefeed = 1;
    }
    return;
  }
}

static FILE *nonblockpopen(const char *cmd, const char *mode)
{
  int fd;
  int flags;
  FILE *r = popen(cmd, mode);
  if (!r)
    return r;

  fd = fileno(r);

  flags = fcntl(fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);

  return r;
}

static int makeprocess(struct process *p, unsigned long gen, unsigned long seed, int verbosity)
{
  char ddscmd[256];
  FILE *f;

  assert(p->f == NULL);
  sprintf(ddscmd, "gen-%ld-%ld-52-20.txt", seed, gen);
  remove(ddscmd);

  sprintf(ddscmd, "dds -genseed=%ld -gen=%ld -gentricks=20", seed, gen);

  if (verbosity >= 2) {
    fprintf(stderr, "%sRunning '%s'\n", (linefeed ? "\n": ""), ddscmd);
    linefeed = 0;
  }

  f = nonblockpopen(ddscmd, "r");

  if (!f) {
    perror("Can't run the dds");
    return - 1;
  }
  scheduled += gen;
  p->f = f;
  p->fd = fileno(p->f);
  return p->fd;
}

static void closeprocess(struct process *p)
{
  pclose(p->f);
  p->f = NULL;
  p->fd = 0;
  free(p->partial);
  p->partial = NULL;
}

int main(int argc, char * const argv[])
{
  char *output = NULL;
  char buffer[1024];
  unsigned long gen = 100, seed;
  char c;
  FILE *out;
  int verbosity = 1, running = 0;
  struct timespec tp;
  char mode[] = "w";
  int cores = sysconf (_SC_NPROCESSORS_CONF);
  unsigned long blocksize;
  struct process *process;

  clock_gettime(CLOCK_REALTIME, &tp);

  seed = tp.tv_nsec + tp.tv_sec * 1000 * 1000 * 1000;
  while ((c = getopt(argc, argv, "hc:a:o:s:g:qv")) != -1) {
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
    case 'c':
      cores = atoi(optarg);
      if (cores <= 0)
        usage(argv[0], 2, "ERROR: %d cores must be over zero", cores);
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

  freopen("/dev/null", "r", stdin);

  process = alloca(sizeof(process[0])*cores);
  memset(process, 0, sizeof(process[0])*cores);

  if (gen >= cores * 8 && cores > 1)
    blocksize = gen / (cores * 4);
  else if (gen >= cores * 2 && cores > 1)
    blocksize = gen / (cores * 2);
  else if (gen >= cores && cores > 1)
    blocksize = gen / cores;
  else
    blocksize = gen;

  if (blocksize >= 40)
    blocksize = 40;

  seed ^= seed >> 31;
  seed &= (uint32_t)~0;

  if (verbosity >= 1)
    fprintf(stderr, "Generating %ld deals with %ld seed. Each subprocess does %ld deals.\n",
        gen, seed, blocksize);

  out = output ? fopen(output, mode) : stdout;

  if (!out) {
    perror("Can't open output file");
    return 10;
  }

  {
    int fd, maxfd = -1;
    int c;
    fd_set fds;
    fd_set rfds;

    FD_ZERO(&fds);

    for (c = 0; c < cores; c++) {
      unsigned long b = gen - scheduled > blocksize ?
                blocksize : gen - scheduled;

      if (b == 0)
        break;

      if ((fd = makeprocess(&process[c], b, seed++, verbosity)) < 0)
        return 20;
      running++;

      if (fd > maxfd)
        maxfd = fd;

      FD_SET(fd, &fds);
    }

    while (running > 0) {
      int active;
      int p = 0;
      rfds = fds;
      active = select(maxfd + 1, &rfds, NULL, NULL, NULL);

      if (active <= 0) {
        perror("select");
        return 60;
      }
      for (; p < cores; p++) {
        if (FD_ISSET(process[p].fd, &rfds)) {
          errno = 0;
          while (fgets(buffer, sizeof buffer, process[p].f))
            parseLine(buffer, out, &process[p], gen, verbosity);

          if (feof(process[p].f)) {
            FD_CLR(process[p].fd, &fds);
            closeprocess(&process[p]);
            running--;
            if (scheduled < gen) {
              unsigned long b = gen - scheduled > blocksize ?
                blocksize : gen - scheduled;

              if ((fd = makeprocess(&process[p], b, seed++, verbosity)) < 0)
                return 20;

              running++;

              if (fd > maxfd)
                maxfd = fd;

              FD_SET(fd, &fds);
            }
            continue;
          }
          if (errno != EWOULDBLOCK) {
            perror("fgets");
            return 50;
          }
        }
      }
    }
  }

  fclose(out);
  return 0;
}
