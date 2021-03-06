#include "genlib.h"

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(WIN32) || defined(__WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/select.h>
#endif
#include <limits.h>
#include <fcntl.h>

#include <assert.h>

#include <errno.h>

#include <pcg_random.hpp>

#include <random>
#include <thread>


/**
 * Print the help message for the tool
 */
static void usage(const char *name, int exitcode, const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  fprintf(stderr, "\nUsage: %s [-hqv] [-o|-a filename] [-g deals] [-s seed1,seed2,...]\n"
      "\n"
      "\t-h\t\tPrint this message\n"
      "\t-o <file>\tSelect output file <stdout>\n"
      "\t-a <file>\tAppend output to the file\n"
      "\t-g <number>\tSet number of deals to generate <100>\n"
      "\t-s <nr|nr,nr>\tSet random the seed for random number generator. <current time>\n"
      "\t\t\t%ld <= nr <= %ld or \"dev\" for std::random_device.\n"
      "\t\t\tThe useful number of numbers is 1 or 2.\n"
      "\t-c <number>\tSet number of threads to use <a thread per core>\n"
      "\t-q\t\tBe quiet\n"
      "\t-v\t\tBe verbose\n",
      name, LONG_MIN, LONG_MAX);
  exit(exitcode);
}

/**
 * State for each subprocess launched using popen
 */
struct process {
  FILE *f;
  int fd;
  char *partial;
};

static unsigned long generate = 0;
static unsigned long scheduled = 0;
static int linefeed = 0;

static char ucrep[14] = "23456789TJQKA";

/**
 * Parse a deal line that contains cards and trcik results for all contracts.
 *
 * 1 A64.8732.543.Q98 T8.96.AKT.A76432 Q2.AQJT54.Q82.JT KJ9753.K.J976.K5:6666BBBA5555A9A99999
 * <deal number> <west> <north <east> <south>:<result>
 */
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

  /* Setup ordered look up table for cards */
  player[0] = north;
  player[1] = east;
  player[2] = south;
  player[3] = west;

  /* Parse number of tricks for all contracts. No support for unknown trick numbers. */
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

  /* Parse cards for each player */
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

  /* library.dat is defined to be in network byte order. That means big
   * endian and byte swapping for little machines. */
  for (p = 0; p < 4; p++)
    d->suits[p] = htonl(d->suits[p]);
  for (p = 0; p < 5; p++)
    d->tricks[p] = htons(d->tricks[p]);
}

/* Looks if the input line has a deal or other junk that dds prints out */
static void parseLine(char *buffer, FILE *out, struct process *p, unsigned long gen, int verbosity)
{
  const char * const filenamestart = "output written to file ";
  const char * const skipdeals = "  deal=";

  char north[17], east[17], south[17], west[17], results[21];
  long deal;

  /* Check if we only have a partial line. That is very likely to happen
   * because underlying fd is set to nonblocking mode preventing stdio from
   * blocking waiting for more input.
   */
  if (!strchr(buffer, '\n')) {
    assert(p->partial == NULL);
    p->partial = strdup(buffer);
    return;
  }

  /* Previous call had partial line stored that requires us to prepend content
   * from the previous call
   */
  if (p->partial) {
    long len = strlen(p->partial);
    memmove(buffer + len, buffer, strlen(buffer));
    memcpy(buffer, p->partial, len);
    free(p->partial);
    p->partial = NULL;
  }

  /*   deal=1 leader=W 5H nodes=13,311 elapsed=0.01 */
  if (strncmp(buffer, skipdeals, strlen(skipdeals)) == 0)
    return;
  /*output written to file gen-0-1-52-20.txt*/
  if (strncmp(buffer, filenamestart, strlen(filenamestart)) == 0) {
    char * const outfile = buffer + strlen(filenamestart);
    strchr(outfile, '\n')[0] = '\0';
    if (verbosity >= 2) {
      fprintf(stderr, "%sRemoving '%s'\n", (linefeed ? "\n": ""), outfile);
      linefeed = 0;
    }
    /* Remove the giblib file created by dds */
    remove(outfile);
    return;
  }
  /* Match the giblib line from the input */
  if (sscanf(buffer, "%ld %16s %16s %16s %16s:%20s",
        &deal, west, north, east, south, results) == 6) {
    struct tagLibdeal libdeal = {{0},{0}};
    parsedeal(&libdeal, north, east, south, west, results, verbosity);

    /* Store dds result in 26 bytes. Actual libdeal struct has extra "valid"
     * field. That should be probably separated to a child struct to make it
     * clear it isn't in the file.
     */
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

/* Open subprocess and set underlying fd to nonblocking state */
static FILE *nonblockpopen(const char *cmd, const char *mode)
{
  int fd;
  int flags;
  FILE *r = popen(cmd, mode);
  if (!r)
    return r;

#if !defined(WIN32) && !defined(WIN32)
  fd = fileno(r);

  flags = fcntl(fd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(fd, F_SETFL, flags);
#endif

  return r;
}

static const char* command = "ddd";

/* Setup command line and store state information for the sub process */
static int makeprocess(struct process *p, unsigned long gen, unsigned seed, int verbosity)
{
  char ddscmd[256];
  FILE *f;

  assert(p->f == NULL);
  sprintf(ddscmd, "gen-%u-%ld-52-20.txt", seed, gen);
  remove(ddscmd);

  sprintf(ddscmd, "%s -genseed=%u -gen=%ld -gentricks=20", command, seed, gen);

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

/* Clean up state after subprocess has exited */
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
  unsigned long gen = 100;
  unsigned seed[2];
  unsigned seedpos = 0;
  char c;
  int mul = 0;
  FILE *out;
  int verbosity = 1, running = 0;
  struct timespec tp;
  char mode[] = "w";
  int cores = std::thread::hardware_concurrency();
  unsigned long blocksize;
  struct process *process;

#ifdef _WIN32
  seed[0] = time(NULL);
#else
  clock_gettime(CLOCK_REALTIME, &tp);

  seed[0] = tp.tv_nsec + tp.tv_sec * 1000 * 1000 * 1000;
#endif
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
      {
        char *startptr = optarg;
        /* Read seed from /dev/urandom */
        if (strcmp("dev", optarg) == 0) {
          std::random_device dev;
          for (seedpos = 0; seedpos < sizeof seed/sizeof seed[0]; seedpos++)
            seed[seedpos] = dev();
          break;
        }
        for (; seedpos < sizeof seed/sizeof seed[0]; seedpos++) {
          char *endptr;
          unsigned long s;
          errno = 0;
          s = strtol(startptr, &endptr, 10);
          if (endptr == startptr) {
            break;
          }
          if (errno != 0) {
            perror("sttrol");
            return errno;
          }
          startptr = endptr + 1;
          seed[seedpos] = s;
          if (*endptr == '\0') {
            seedpos++;
            break;
          }
        }
      }
      break;
    case 'a':
      mode[0] = 'a';
      /* fallthrough */
    case 'o':
      output = strdup(optarg);
      break;
    }
  }
#if _WIN32
  const char* nulldev = "nul";
#else
  const char* nulldev = "/dev/null";
#endif

  // Try ddd
  char cmd[256];
  sprintf(cmd, "%s > %s", command, nulldev);
  if (system(cmd)) {
    command = "./ddd";
    sprintf(cmd, "%s > %s", command, nulldev);
    if (system(cmd)) {
      perror("Cannot find ddd binary for popen call.");
      return -1;
    }
  }

  if (!freopen(nulldev, "r", stdin)) {
    perror("freopen null device failed");
    return -2;
  }

  process = static_cast<struct process*>(alloca(sizeof(process[0])*cores));
  memset(process, 0, sizeof(process[0])*cores);

  /* Calculate reasonable block sizes for the cpu configuration */
  blocksize = gen;
  for (mul = 20; mul > 0; mul--) {
    long high = 1 << mul;

    if (gen >= (unsigned)(cores*high)) {
      blocksize = gen / (cores * mul);
      break;
    }
  }

  if (blocksize >= (1 << 20)/20)
    blocksize = (1 << 20)/20;

  unsigned i, pos = 0;
  pcg32_k2 rng;
  if (seedpos < 2)
    rng.seed(seed[0]);
  else
    rng.seed(seed);

  pos = sprintf(buffer, "%u", seed[0]);
  for (i = 1; i < seedpos; i++)
    pos += sprintf(&buffer[pos], ",%u", seed[i]);


  if (verbosity >= 1)
    fprintf(stderr, "Generating %ld deals with %s seed. Each subprocess does %ld deals.\n",
        gen, buffer, blocksize);

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

      if ((fd = makeprocess(&process[c], b, rng(), verbosity)) < 0)
        return 20;
      running++;

      if (fd > maxfd)
        maxfd = fd;

      FD_SET(fd, &fds);
    }

    /* Loop as long as we have subprocess active */
    while (running > 0) {
      int active;
      int p = 0;
      rfds = fds;
      /* Wait for any input */
      do {
        active = select(maxfd + 1, &rfds, NULL, NULL, NULL);
      } while (active == -1 && errno == EINTR);

      if (active <= 0) {
        perror("select");
        return 60;
      }
      /* Check which subprocess provided the input */
      for (; p < cores; p++) {
        if (FD_ISSET(process[p].fd, &rfds)) {
          errno = 0;
          while (fgets(buffer, sizeof buffer, process[p].f))
            parseLine(buffer, out, &process[p], gen, verbosity);

          /* Has the process exited? */
          if (feof(process[p].f)) {
            FD_CLR(process[p].fd, &fds);
            closeprocess(&process[p]);
            running--;
            /* Is there more blocks to shedule? */
            if (scheduled < gen) {
              unsigned long b = gen - scheduled > blocksize ?
                blocksize : gen - scheduled;

              if ((fd = makeprocess(&process[p], b, rng(), verbosity)) < 0)
                return 20;

              running++;

              if (fd > maxfd)
                maxfd = fd;

              FD_SET(fd, &fds);
            }
            continue;
          }
          /* EWOULDBLOCK and EINTR can happen during normal operation */
          if (errno != EWOULDBLOCK && errno != EINTR) {
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
