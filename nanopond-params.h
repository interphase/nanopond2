/* ----------------------------------------------------------------------- */
/* Tunable parameters                                                      */
/* ----------------------------------------------------------------------- */

/* Tick length.  Simpler to think about frequencies in terms of ticks
 * rather than numbers with lots of trailing zeros.*/
#define TICK 10000ULL

/* Iteration to stop at. Comment this out to run forever. */
#define STOP_AT (10000 * TICK)

/* Frequency of comprehensive reports-- lower values will provide more
 * info while slowing down the simulation. Higher values will give less
 * frequent updates. */
//#define REPORT_FREQUENCY (10 * TICK)

/* SDL refresh frequency */
#define SDL_REFRESH_FREQUENCY (10 * TICK)

/* Frequency at which to dump all viable replicators (generation > 2)
 * to a file named <clock>.dump in the current directory.  Comment
 * out to disable. The cells are dumped in hexadecimal, which is
 * semi-human-readable if you look at the big switch() statement
 * in the main loop to see what instruction is signified by each
 * four-bit value. */
//#define DUMP_FREQUENCY (10000 * TICK)

/* Mutation rate -- range is from 0 (none) to 0xffffffff (all mutations!) */
/* To get it from a float probability from 0.0 to 1.0, multiply it by
 * 4294967295 (0xffffffff) and round. */
#define MUTATION_RATE 21475 /* p=~0.000005 */

/* How frequently should random cells / energy be introduced?
 * Making this too high makes things very chaotic. Making it too low
 * might not introduce enough energy. */
#define INFLOW_FREQUENCY 100

/* Base amount of energy to introduce per INFLOW_FREQUENCY ticks */
#define INFLOW_RATE_BASE 4000

/* A random amount of energy between 0 and this is added to
 * INFLOW_RATE_BASE when energy is introduced. Comment this out for
 * no variation in inflow rate. */
#define INFLOW_RATE_VARIATION 8000

/* Size of pond in X and Y dimensions. */
#define POND_SIZE_X 640
#define POND_SIZE_Y 480

/* Depth of pond in four-bit codons -- this is the maximum
 * genome size. This *must* be a multiple of 16! */
#define POND_DEPTH 512

/* This is the divisor that determines how much energy is taken
 * from cells when they try to KILL a viable cell neighbor and
 * fail. Higher numbers mean lower penalties. */
#define FAILED_KILL_PENALTY 2

/* Define this to use SDL. To use SDL, you must have SDL headers
 * available and you must link with the SDL library when you compile. */
/* Comment this out to compile without SDL visualization support. */
#define USE_SDL 1

/* Define this to use a fixed random number seed.  Comment out to use
 * a time-based seed. */
#define RANDOM_NUMBER_SEED 13
