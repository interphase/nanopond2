#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "nanopond-params.h"
#ifdef USE_SDL
#ifdef _MSC_VER
#include <SDL.h>
#else
#include <SDL/SDL.h>
#endif /* _MSC_VER */
#endif /* USE_SDL */
#include "nanopond-vminst.h"

#include "xorshift/xorshift.h"
static void init_genrand() {
#ifdef RANDOM_NUMBER_SEED
 unsigned long s = (RANDOM_NUMBER_SEED);
#else
 unsigned long s = time(NULL);
#endif
  init_xorgen(s);
}
static inline uint64_t getRandom()
{
  return genrand_uint64();
}


/* ----------------------------------------------------------------------- */

#if DEBUG_DISASM
#define DEBUG_VM(...) printf(__VA_ARGS__)
#else
#define DEBUG_VM(...)
#endif

#ifdef STATCOUNTERS
#define STATCOUNTER(cond, stat) (stat += cond);
#else
#define STATCOUNTER(...)
#endif

/* Pond depth in machine-size words.  This is calculated from
 * POND_DEPTH and the size of the machine word. (The multiplication
 * by two is due to the fact that there are two four-bit values in
 * each eight-bit byte.) */
#define POND_DEPTH_SYSWORDS (POND_DEPTH / (sizeof(uint64_t) * 2))

/* Number of bits in a machine-size word */
#define SYSWORD_BITS (sizeof(uint64_t) * 8)

/* Constants representing neighbors in the 2D grid. */
#define N_LEFT 0
#define N_RIGHT 1
#define N_UP 2
#define N_DOWN 3

/* Word and bit at which to start execution */
/* This is after the "logo" */
#define EXEC_START_WORD 0
#define EXEC_START_BIT 4

#define FLAG_SHARED 1
#define FLAG_KILLED 2
#define FLAG_BUF 4
#define FLAG_EARLY_TURN 8

/* Number of bits set in binary numbers 0000 through 1111 */
static const uint64_t BITS_IN_FOURBIT_WORD[16] = { 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4 };

typedef uint64_t genome_t;
struct Cell;
/**
 * Structure for a cell in the pond
 */
struct Cell
{
  /* Globally unique cell ID */
  uint64_t ID;

  /* ID of the cell's parent */
  uint64_t parentID;

  /* Counter for original lineages -- equal to the cell ID of
  * the first cell in the line. */
  uint64_t lineage;

  /* Generations start at 0 and are incremented from there. */
  uint64_t generation;

  /* Energy level of this cell */
  uint64_t energy;

  /* Memory space for cell genome (genome is stored as four
  * bit instructions packed into machine size words) */
  genome_t genome[POND_DEPTH_SYSWORDS];
  struct Cell *un, *ds, *re, *lw;
};

/* The pond is a 2D array of cells */
struct Cell pond[(uint64_t)POND_SIZE_X*(uint64_t)POND_SIZE_Y];
#define POND(x, y) pond[((uint64_t)y)*(uint64_t)POND_SIZE_X+((uint64_t)x)]
#define POND_SIZE ((uint64_t)POND_SIZE_X * (uint64_t)POND_SIZE_Y)

/* Currently selected color scheme */
enum { KINSHIP,LINEAGE,MAX_COLOR_SCHEME } colorScheme = KINSHIP;
const char *colorSchemeName[2] = { "KINSHIP", "LINEAGE" };

/**
 * Structure for keeping some running tally type statistics
 */
struct PerReportStatCounters
{
  /* Counts for the number of times each instruction was
  * executed since the last report. */
  double instructionExecutions[16];

  /* Number of cells executed since last report */
  double cellExecutions;

  /* Number of viable cells replaced by other cells' offspring */
  uint64_t viableCellsReplaced;

  /* Number of viable cells KILLed */
  uint64_t viableCellsKilled;

  /* Number of successful SHARE operations */
  uint64_t viableCellShares;
};

/* Global statistics counters */
struct PerReportStatCounters statCounters;
