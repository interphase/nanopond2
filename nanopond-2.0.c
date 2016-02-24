#include "nanopond-2.0.h"

/**
 * Output a line of comma-seperated statistics data
 *
 * @param clock Current clock
 */
#ifdef REPORT_FREQUENCY
static void doReport(const uint64_t clock)
{
  static uint64_t lastTotalViableReplicators = 0;

  uint64_t i;

  uint64_t totalActiveCells = 0;
  uint64_t totalEnergy = 0;
  uint64_t totalViableReplicators = 0;
  uint64_t maxGeneration = 0;

  for(i=0;i<POND_SIZE;++i) {
    struct Cell *const c = &pond[i];
    if (c->energy) {
      ++totalActiveCells;
      totalEnergy += (uint64_t)c->energy;
      if (c->generation > 2){
        ++totalViableReplicators;
      }
      if (c->generation > maxGeneration){
        maxGeneration = c->generation;
      }
    }
  }

  /* Look here to get the columns in the CSV output */

  /* The first five are here and are self-explanatory */
  printf(              \
    "%" PRIu64 "," /* clock */        \
    "%" PRIu64 "," /* totalEnergy */      \
    "%" PRIu64 "," /* totalActiveCells */      \
    "%" PRIu64 "," /* totalViableReplicators */    \
    "%" PRIu64 "," /* maxGeneration */      \
    "%" PRIu64 "," /* statCounters.viableCellsReplaced */  \
    "%" PRIu64 "," /* statCounters.viableCellsKilled */  \
    "%" PRIu64    ,/* statCounters.viableCellShares */  \
    clock,
    totalEnergy,
    totalActiveCells,
    totalViableReplicators,
    maxGeneration,
    statCounters.viableCellsReplaced,
    statCounters.viableCellsKilled,
    statCounters.viableCellShares
  );

  /* The next 16 are the average frequencies of execution for each
  * instruction per cell execution. */
  double totalMetabolism = 0.0;
  for(x=0;x<16;++x) {
    totalMetabolism += statCounters.instructionExecutions[x];
    printf(",%.4f",(statCounters.cellExecutions > 0.0) ? (statCounters.instructionExecutions[x] / statCounters.cellExecutions) : 0.0);
  }

  /* The last column is the average metabolism per cell execution */
  printf(",%.4f\n",(statCounters.cellExecutions > 0.0) ? (totalMetabolism / statCounters.cellExecutions) : 0.0);
  fflush(stdout);

  if ((lastTotalViableReplicators > 0)&&(totalViableReplicators == 0)){
    fprintf(stderr,"[EVENT] Viable replicators have gone extinct. Please reserve a moment of silence.\n");
  }
  else if ((lastTotalViableReplicators == 0)&&(totalViableReplicators > 0)){
    fprintf(stderr,"[EVENT] Viable replicators have appeared!\n");
  }

  lastTotalViableReplicators = totalViableReplicators;

  /* Reset per-report stat counters */
  for(x=0;x<sizeof(statCounters);++x){
    ((uint8_t *)&statCounters)[x] = (uint8_t)0;
  }
}
#endif //REPORT_FREQUENCY

/**
 * Dumps all viable (generation > 2) cells to a file called <clock>.dump
 *
 * @param clock Clock value
 */
#ifdef DUMP_FREQUENCY
static void doDump(const uint64_t clock)
{
  char buf[POND_DEPTH*2];
  FILE *d;
  uint64_t i,wordPtr,shiftPtr,stopCount,i;
  struct Cell *cell;
  genome_t inst;

  sprintf(buf,"%" PRIu64 ".dump.csv",clock);
  d = fopen(buf,"w");
  if (!d) {
    fprintf(stderr,"[WARNING] Could not open %s for writing.\n",buf);
    return;
  }

  fprintf(stderr,"[INFO] Dumping viable cells to %s\n",buf);

  for(i=0;i<POND_SIZE;++i) {
    cell = &pond[i];
    dumpCell(d, cell);
  }
  fclose(d);
}
#endif //DUMP_FREQUENCY

/**
 * Dumps the genome of a cell to a file.
 *
 * @param file Destination
 * @param cell Source
 */
static void dumpCell(FILE *file, struct Cell *cell)
{
  uint64_t wordPtr,shiftPtr,stopCount,i;
  genome_t inst;

  if (cell->energy&&(cell->generation > 2)) {
    wordPtr = 0;
    shiftPtr = 0;
    stopCount = 0;
    for(i=0;i<POND_DEPTH;++i) {
        inst = (cell->genome[wordPtr] >> shiftPtr) & 0xf;
        /* Four STOP instructions in a row is considered the end.
        * The probability of this being wrong is *very* small, and
        * could only occur if you had four STOPs in a row inside
        * a LOOP/REP pair that's always false. In any case, this
        * would always result in our *underestimating* the size of
        * the genome and would never result in an overestimation. */
      fprintf(file,"%" PRIx64,inst);
      if (inst == 0xf) { /* STOP */
        if (++stopCount >= 4){
          break;
        }
      } else {
        stopCount = 0;
      }
      if ((shiftPtr += 4) >= SYSWORD_BITS) {
        if (++wordPtr >= POND_DEPTH_SYSWORDS) {
          wordPtr = EXEC_START_WORD;
          shiftPtr = EXEC_START_BIT;
        } else {
          shiftPtr = 0;
        }
      }
    }
  }
  fwrite("\n",1,1,file);
}

/**
 * Get a neighbor in the pond
 *
 * @param x Starting X position
 * @param y Starting Y position
 * @param dir Direction to get neighbor from
 * @return Pointer to neighboring cell
 */
static inline struct Cell *getNeighbor(struct Cell* cell, const uint64_t dir)
{
  /* Space is toroidal; it wraps at edges */
  switch(dir) {
  case N_LEFT:
    return cell->lw;
  case N_RIGHT:
    return cell->re;
  case N_UP:
    return cell->un;
  default: // N_DOWN
    return cell->ds;
  }
}

/**
 * Determines if c1 is allowed to access c2
 *
 * @param c2 Cell being accessed
 * @param c1guess c1's "guess"
 * @param sense The "sense" of this interaction
 * @return True or false (1 or 0)
 */
static inline int accessAllowed(struct Cell *const c2,const genome_t c1guess,int sense)
{
  /* Access permission is more probable if they are more similar in sense 0,
  * and more probable if they are different in sense 1. Sense 0 is used for
  * "negative" interactions and sense 1 for "positive" ones. */
  return sense ? (((getRandom() & 0xf) >= BITS_IN_FOURBIT_WORD[(c2->genome[0] & 0xf) ^ (c1guess & 0xf)])||(!c2->parentID)) : (((getRandom() & 0xf) <= BITS_IN_FOURBIT_WORD[(c2->genome[0] & 0xf) ^ (c1guess & 0xf)])||(!c2->parentID));
}

/**
 * Gets the color that a cell should be
 *
 * @param c Cell to get color for
 * @return 8-bit color value
 */
#ifdef USE_SDL
static inline uint8_t getColor(struct Cell *c)
{
  uint64_t i,j,sum,skipnext;
  genome_t word, opcode;

  if (c->energy) {
    switch(colorScheme) {
      case KINSHIP:
        /*
         * Kinship color scheme by Christoph Groth
         *
         * For cells of generation > 1, saturation and value are set to maximum.
         * Hue is a hash-value with the property that related genomes will have
         * similar hue (but of course, as this is a hash function, totally
         * different genomes can also have a similar or even the same hue).
         * Therefore the difference in hue should to some extent reflect the grade
         * of "kinship" of two cells.
         */
        if (c->generation > 1) {
          sum = 0;
          skipnext = 0;
          for(i=0;i<POND_DEPTH_SYSWORDS&&(c->genome[i] != ~((genome_t)0));++i) {
            word = c->genome[i];
            for(j=0;j<SYSWORD_BITS/4;++j,word >>= 4) {
              /* We ignore 0xf's here, because otherwise very similar genomes
              * might get quite different hash values in the case when one of
              * the genomes is slightly longer and uses one more maschine
              * word. */
              opcode = word & 0xf;
              if (skipnext){
                skipnext = 0;
              } else {
                if (opcode != 0xf){
                  sum += opcode;
                }
                if (opcode == 0xc){ /* 0xc == XCHG */
                  skipnext = 1; /* Skip "operand" after XCHG */
                }
              }
            }
          }
          /* For the hash-value use a wrapped around sum of the sum of all
           * commands and the length of the genome. */
          return (uint8_t)((sum % 192) + 64);
        }
        return 0;
      case LINEAGE:
        /*
         * Cells with generation > 1 are color-coded by lineage.
         */
        return (c->generation > 1) ? (((uint8_t)c->lineage) | (uint8_t)1) : 0;
      case MAX_COLOR_SCHEME:
        /* ... never used... to make compiler shut up. */
        break;
    }
  }
  return 0; /* Cells with no energy are black */
}
#endif //USE_SDL

/**
 * Main method
 *
 * @param argc Number of args
 * @param argv Argument array
 */
int main(int argc,char **argv)
{
  uint64_t i = 0, x = 0, y = 0, idx = 0;

  /* Buffer used for execution output of candidate offspring */
  genome_t outputBuf[POND_DEPTH_SYSWORDS];

  /* Seed and init the random number generator */
  init_genrand();
  for(i=0;i<1024;++i){
    getRandom();
  }

  /* Reset per-report stat counters */
  for(x=0;x<sizeof(statCounters);++x){
    ((uint8_t *)&statCounters)[x] = (uint8_t)0;
  }

  /* Set up SDL if we're using it */
#ifdef USE_SDL
  SDL_Surface *screen;
  SDL_Event sdlEvent;
  if (SDL_Init(SDL_INIT_VIDEO) < 0 ) {
    fprintf(stderr,"*** Unable to init SDL: %s ***\n",SDL_GetError());
    exit(1);
  }
  atexit(SDL_Quit);
  SDL_WM_SetCaption("nanopond","nanopond");
  screen = SDL_SetVideoMode(POND_SIZE_X,POND_SIZE_Y,8,SDL_SWSURFACE);
  if (!screen) {
    fprintf(stderr, "*** Unable to create SDL window: %s ***\n", SDL_GetError());
    exit(1);
  }
  const uint64_t sdlPitch = screen->pitch;
#endif /* USE_SDL */

  /* Clear the pond and initialize all genomes to 0xffff... */
  uint64_t p;
  for(y=0;y<POND_SIZE_Y;++y) {
    for(x=0;x<POND_SIZE_X;++x) {
      p = y*(uint64_t)POND_SIZE_X+x;
      pond[p].ID = 0;
      pond[p].parentID = 0;
      pond[p].lineage = 0;
      pond[p].generation = 0;
      pond[p].energy = 0;
      for(i=0;i<POND_DEPTH_SYSWORDS;++i){
        pond[p].genome[i] = ~((genome_t)0);
      }

      /* Space is toroidal; it wraps at edges */
      pond[p].lw = (x) ? &POND(x-1, y) : &POND(POND_SIZE_X-1,y);
      pond[p].re = (x < (POND_SIZE_X-1)) ? &POND(x+1,y) : &POND(0,y);
      pond[p].un = (y) ? &POND(x,y-1) : &POND(x,POND_SIZE_Y-1);
      pond[p].ds = (y < (POND_SIZE_Y-1)) ? &POND(x,y+1) : &POND(x,0);

      //printf("%lu\t%lu\t%lu\t%lu\n", pond[p].lw, pond[p].re, pond[p].un, pond[p].ds);
    }
  }

  /* Clock is incremented on each core loop */
  uint64_t clock = 0;

  /* This is used to generate unique cell IDs */
  uint64_t cellIdCounter = 0;

  /* Miscellaneous variables used in the loop */
  uint64_t currentWord = 0,
           wordPtr = 0,
           shiftPtr = 0,
           inst = 0,
           tmp = 0;
  struct Cell *cell = 0,
              *tmcell = 0;

  /* Virtual machine memory pointer register (which
  * exists in two parts... read the code below...) */
  uint64_t ptr_wordPtr = 0;
  uint64_t ptr_shiftPtr = 0;

  /* The main "register" */
  uint64_t reg = 0;

  /* Which way is the cell facing? */
  uint64_t facing = 0;

  /* Virtual machine loop/rep stack */
  uint64_t loopStack_wordPtr[POND_DEPTH];
  uint64_t loopStack_shiftPtr[POND_DEPTH];
  uint64_t loopStackPtr = 0;

  /* Machine flags */
  uint64_t flags = 0;

  /* If this is nonzero, we're skipping to matching REP */
  /* It is incremented to track the depth of a nested set
  * of LOOP/REP pairs in false state. */
  uint64_t falseLoopDepth = 0;

  /* If this is nonzero, cell execution stops. This allows us
  * to avoid the ugly use of a goto to exit the loop. :) */
  int stop = 0;

  printf("exec_start\n");
  /* Main loop */
  for(;;) {
    /* Stop at STOP_AT if defined */
#ifdef STOP_AT
    if (clock >= STOP_AT) {
    /* Also do a final dump if dumps are enabled */
#ifdef DUMP_FREQUENCY
      doDump(clock);
#endif /* DUMP_FREQUENCY */
      fprintf(stderr,"[QUIT] STOP_AT clock value reached\n");
      break;
    }
#endif /* STOP_AT */

    /* Increment clock and run reports periodically */
    /* Clock is incremented at the start, so it starts at 1 */
    ++clock;

#ifdef REPORT_FREQUENCY
    if (!(clock % REPORT_FREQUENCY)) {
      doReport(clock);
    }
#endif

#ifdef USE_SDL
    /* Refresh the screen and check for input if SDL enabled */
    if (!(clock % SDL_REFRESH_FREQUENCY)){
      while (SDL_PollEvent(&sdlEvent)) {
        if (sdlEvent.type == SDL_QUIT) {
          fprintf(stderr,"[QUIT] Quit signal received!\n");
          exit(0);
        } else if (sdlEvent.type == SDL_MOUSEBUTTONDOWN) {
          switch (sdlEvent.button.button) {
          case SDL_BUTTON_LEFT:
            fprintf(stderr,"[INTERFACE] Genome of cell at (%d, %d):\n",sdlEvent.button.x, sdlEvent.button.y);
            dumpCell(stderr, &POND(sdlEvent.button.x,sdlEvent.button.y));
            break;
          case SDL_BUTTON_RIGHT:
            colorScheme = (colorScheme + 1) % MAX_COLOR_SCHEME;
            fprintf(stderr,"[INTERFACE] Switching to color scheme \"%s\".\n",colorSchemeName[colorScheme]);
            for (y=0;y<POND_SIZE_Y;++y) {
              for (x=0;x<POND_SIZE_X;++x)
                ((uint8_t *)screen->pixels)[x + (y * sdlPitch)] = 0;//getColor(&pond[x][y]);
            }
            break;
          }
        }
      }
      SDL_UpdateRect(screen,0,0,POND_SIZE_X,POND_SIZE_Y);
    }
#endif /* USE_SDL */

#ifdef DUMP_FREQUENCY
    /* Periodically dump the viable population if defined */
    if (!(clock % DUMP_FREQUENCY)){
      doDump(clock);
    }
#endif /* DUMP_FREQUENCY */

    if (!(clock % 10000000)) {
      printf("%"PRIu64"\n", clock);
    }

    /* Introduce a random cell somewhere with a given energy level */
    /* This is called seeding, and introduces both energy and
    * entropy into the substrate. This happens every INFLOW_FREQUENCY
    * clock ticks. */
    if (!(clock % INFLOW_FREQUENCY)) {
      idx = getRandom() % POND_SIZE;
      cell = &pond[idx];
      cell->ID = cellIdCounter;
      cell->parentID = 0;
      cell->lineage = cellIdCounter;
      cell->generation = 0;
#ifdef INFLOW_RATE_VARIATION
      cell->energy += INFLOW_RATE_BASE + (getRandom() % INFLOW_RATE_VARIATION);
  #else
      cell->energy += INFLOW_RATE_BASE;
#endif /* INFLOW_RATE_VARIATION */
      for(i=0;i<POND_DEPTH_SYSWORDS;++i) {
        cell->genome[i] = getRandom();
      }
      ++cellIdCounter;

#ifdef USE_SDL
      /* Update the random cell on SDL screen if viz is enabled */
      if (SDL_MUSTLOCK(screen)){
        SDL_LockSurface(screen);
      }
      ((uint8_t *)screen->pixels)[x + (y * sdlPitch)] = getColor(cell);
      if (SDL_MUSTLOCK(screen)){
        SDL_UnlockSurface(screen);
      }
#endif /* USE_SDL */
    }

    /* Pick a random cell to execute */

    idx = getRandom() % POND_SIZE;
    cell = &pond[idx];
    x = idx % POND_SIZE_X;
    y = idx / POND_SIZE_X;
    //printf("%lu\t%lu\t%lu\t%lu\n", x, y, y * POND_SIZE_X + x, idx);
//     break;

    /* Keep track of how many cells have been executed */
//     statCounters.cellExecutions += 1.0;

    /* Reset the state of the VM prior to execution */
    if (flags & FLAG_BUF) {
      for(i=0;i<POND_DEPTH_SYSWORDS;++i) {
        outputBuf[i] = ~((uint64_t)0); /* ~0 == 0xfffff... */
      }
    }
    ptr_wordPtr = 0;
    ptr_shiftPtr = 0;
    reg = 0;
    loopStackPtr = 0;
    wordPtr = EXEC_START_WORD;
    shiftPtr = EXEC_START_BIT;
    facing = 0;
    falseLoopDepth = 0;
    stop = 0;
    flags = 0;

    /* We use a currentWord buffer to hold the word we're
     * currently working on.  This speeds things up a bit
     * since it eliminates a pointer dereference in the
     * inner loop. We have to be careful to refresh this
     * whenever it might have changed... take a look at
     * the code. :) */
    currentWord = cell->genome[0];

    /* Core execution loop */
    while (cell->energy&&(!stop)) {
      /* Get the next instruction */
      inst = (currentWord >> shiftPtr) & 0xf;

      /* Randomly frob either the instruction or the register with a
      * probability defined by MUTATION_RATE. This introduces variation,
      * and since the variation is introduced into the state of the VM
      * it can have all manner of different effects on the end result of
      * replication: insertions, deletions, duplications of entire
      * ranges of the genome, etc. */
      if ((getRandom() & 0xffffffff) < MUTATION_RATE) {
        tmp = getRandom(); /* Call getRandom() only once for speed */
        if (tmp & 0x80){ /* Check for the 8th bit to get random boolean */
          inst = tmp & 0xf; /* Only the first four bits are used here */
        } else {
          reg = tmp & 0xf;
        }
      }

      /* Each instruction processed costs one unit of energy */
      --cell->energy;

      /* Execute the instruction */
      if (falseLoopDepth) {
        DEBUG_VM("%"PRIx64 " :\texecute:\tNOP\tloopDepth: %"PRIu64"\n", VM_GETPOS(wordPtr, shiftPtr), falseLoopDepth);
        /* Skip forward to matching REP if we're in a false loop. */
        if (inst == 0x9){ /* Increment false LOOP depth */
          ++falseLoopDepth;
        } else {
          if (inst == 0xa){ /* Decrement on REP */
            --falseLoopDepth;
          }
        }
      } else {
        /* If we're not in a false LOOP/REP, execute normally */
        DEBUG_VM("%"PRIx64 " :\texecute: %"PRIx64"\t", VM_GETPOS(wordPtr, shiftPtr), inst);
        /* Keep track of execution frequencies for each instruction */
        //statCounters.instructionExecutions[inst] += 1.0;

        switch(inst) {
          case 0x0: /* ZERO: Zero VM state registers */
            VM_ZERO(reg, ptr_wordPtr, ptr_shiftPtr, facing);
            break;
          case 0x1: /* FWD: Increment the pointer (wrap at end) */
            VM_FWD(reg, ptr_wordPtr, ptr_shiftPtr);
            break;
          case 0x2: /* BACK: Decrement the pointer (wrap at beginning) */
            VM_BACK(reg, ptr_wordPtr, ptr_shiftPtr);
            break;
          case 0x3: /* INC: Increment the register */
            VM_INC(reg, 1);
            break;
          case 0x4: /* DEC: Decrement the register */
            VM_DEC(reg, 1);
            break;
          case 0x5: /* READG: Read into the register from genome */
            VM_READG(reg, ptr_wordPtr, ptr_shiftPtr, cell->genome);
            break;
          case 0x6: /* WRITEG: Write out from the register to genome */
            VM_WRITEG(reg, ptr_wordPtr, ptr_shiftPtr, cell->genome);
            break;
          case 0x7: /* READB: Read into the register from buffer */
            VM_READB(reg, ptr_wordPtr, ptr_shiftPtr, outputBuf);
            break;
          case 0x8: /* WRITEB: Write out from the register to buffer */
            VM_WRITEB(reg, ptr_wordPtr, ptr_shiftPtr, outputBuf);
            flags |= FLAG_BUF;
            break;
          case 0x9: /* LOOP: Jump forward to matching REP if register is zero */
            VM_LOOP(reg, wordPtr, shiftPtr, loopStackPtr, loopStack_wordPtr, loopStack_shiftPtr, stop, falseLoopDepth);
            break;
          case 0xa: /* REP: Jump back to matching LOOP if register is nonzero */
            VM_REP(reg, wordPtr, shiftPtr, loopStackPtr, loopStack_wordPtr, loopStack_shiftPtr);
            break;
          case 0xb: /* TURN: Turn in the direction specified by register */
            flags &= ~(FLAG_SHARED | FLAG_KILLED);
            VM_TURN(reg, facing);
            break;
          case 0xc: /* XCHG: Skip next instruction and exchange value of register with it */
            VM_XCHG(reg, wordPtr, shiftPtr, cell->genome, tmp);
            break;
          case 0xd: /* KILL: Blow away neighboring cell if allowed with penalty on failure */
            if (!(flags & FLAG_KILLED)) {
              flags |= FLAG_KILLED;
              VM_KILL(reg, cell, tmcell, facing, tmp);
            }
            break;
          case 0xe: /* SHARE: Equalize energy between self and neighbor if allowed */
            if (!(flags & FLAG_SHARED)) {
              flags |= FLAG_SHARED;
              VM_SHARE(reg, cell, tmcell, facing, tmp);
            }
            break;
          case 0xf: /* STOP: End execution */
            VM_STOP(stop);
            break;
        }
      }

      /* Advance the shift and word pointers, and loop around
      * to the beginning at the end of the genome. */
      if ((shiftPtr += 4) >= SYSWORD_BITS) {
        if (++wordPtr >= POND_DEPTH_SYSWORDS) {
          wordPtr = EXEC_START_WORD;
          shiftPtr = EXEC_START_BIT;
        } else {
          shiftPtr = 0;
        }
        currentWord = cell->genome[wordPtr];
      }
    }

    /* Copy outputBuf into neighbor if access is permitted and there
    * is energy there to make something happen. There is no need
    * to copy to a cell with no energy, since anything copied there
    * would never be executed and then would be replaced with random
    * junk eventually. See the seeding code in the main loop above. */
    DEBUG_VM("POSTEXEC:\tcopybuf: \t");
    if ((flags & FLAG_BUF) && (outputBuf[0] & 0xff) != 0xff) {
      tmcell = getNeighbor(cell,facing);
      if ((tmcell->energy)&&accessAllowed(tmcell,reg,0)) {
        DEBUG_VM("SUCCESS\n");
        /* Log it if we're replacing a viable cell */
        if (tmcell->generation > 2) {
          ++statCounters.viableCellsReplaced;
        }

        tmcell->ID = ++cellIdCounter;
        tmcell->parentID = cell->ID;
        tmcell->lineage = cell->lineage; /* Lineage is copied in offspring */
        tmcell->generation = cell->generation + 1;
        for(i=0;i<POND_DEPTH_SYSWORDS;++i){
          tmcell->genome[i] = outputBuf[i];
        }
      } else {
        DEBUG_VM("FAILED\n");
      }
    } else {
      DEBUG_VM("NOT MODIFIED\n");
    }

  DEBUG_VM("** EXEC STOP\tiptr: %"PRIx64"\tmemptr: %"PRIx64"\n", VM_GETPOS(wordPtr, shiftPtr), VM_GETPOS(wordPtr, shiftPtr));
  DEBUG_VM("** EXEC STOP\treg: %"PRIx64"\tfacing: %"PRIu64"\tenergy: %"PRIu64"\n", reg, facing, cell->energy);

    /* Update the neighborhood on SDL screen to show any changes. */
#ifdef USE_SDL
    if (SDL_MUSTLOCK(screen)){
      SDL_LockSurface(screen);
    }
    ((uint8_t *)screen->pixels)[x + (y * sdlPitch)] = getColor(cell);
    if (x) {
      ((uint8_t *)screen->pixels)[(x-1) + (y * sdlPitch)] = getColor(&POND(x-1,y));
      if (x < (POND_SIZE_X-1)) {
        ((uint8_t *)screen->pixels)[(x+1) + (y * sdlPitch)] = getColor(&POND(x+1,y));
      } else {
        ((uint8_t *)screen->pixels)[y * sdlPitch] = getColor(&POND(0,y));
      }
    } else {
      ((uint8_t *)screen->pixels)[(POND_SIZE_X-1) + (y * sdlPitch)] = getColor(&POND(POND_SIZE_X-1,y));
      ((uint8_t *)screen->pixels)[1 + (y * sdlPitch)] = getColor(&POND(1,y));
    }
    if (y) {
      ((uint8_t *)screen->pixels)[x + ((y-1) * sdlPitch)] = getColor(&POND(x,y-1));
      if (y < (POND_SIZE_Y-1)){
        ((uint8_t *)screen->pixels)[x + ((y+1) * sdlPitch)] = getColor(&POND(x,y+1));
      } else {
        ((uint8_t *)screen->pixels)[x] = getColor(&POND(x,0));
      }
    } else {
        ((uint8_t *)screen->pixels)[x + ((POND_SIZE_Y-1) * sdlPitch)] = getColor(&POND(x,POND_SIZE_Y-1));
        ((uint8_t *)screen->pixels)[x + sdlPitch] = getColor(&POND(x,1));
    }
    if (SDL_MUSTLOCK(screen)){
      SDL_UnlockSurface(screen);
    }
#endif /* USE_SDL */
  }

  exit(0);
  return 0; /* Make compiler shut up */
}
