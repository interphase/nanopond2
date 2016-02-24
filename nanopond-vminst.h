#define VM_GETPOS(wp, sp) \
  (wp * SYSWORD_BITS + sp) / 4

#define VM_GETINST(wp, sp, genome) \
  (genome[wp] >> sp) & 0xf

/* ZERO: Zero VM state registers */
#define VM_ZERO(reg, mwp, msp, facing) \
  DEBUG_VM("ZERO:\treg: %"PRIx64" facing: %"PRIu64" -> ", reg, facing); \
  reg = 0; \
  mwp = 0; \
  msp = 0; \
  facing = 0; \
  DEBUG_VM("reg: %"PRIx64" facing: %"PRIu64"\n", reg, facing);

/* FWD: Increment the pointer (wrap at end) */
#define VM_FWD(reg, mwp, msp) \
  DEBUG_VM("FWD:\tmemptr: %"PRIx64" -> ", VM_GETPOS(mwp, msp)); \
  if ((msp += 4) >= SYSWORD_BITS) { \
    if (++mwp >= POND_DEPTH_SYSWORDS){ \
      mwp = 0; \
    } \
    msp = 0; \
  } \
  DEBUG_VM("%"PRIx64 "\n", VM_GETPOS(mwp, msp));

/* BACK: Decrement the pointer (wrap at beginning) */
#define VM_BACK(reg, mwp, msp) \
  DEBUG_VM("BACK:\tmemptr: %"PRIx64" -> ", VM_GETPOS(mwp, msp)); \
  if (msp){ \
    msp -= 4; \
  } else { \
    if (mwp){ \
      --mwp; \
    } else { \
      mwp = POND_DEPTH_SYSWORDS - 1; \
    } \
    msp = SYSWORD_BITS - 4; \
  } \
  DEBUG_VM("%"PRIx64 "\n", VM_GETPOS(mwp, msp));

/* INC: Increment the register */
#define VM_INC(reg, amt) \
  DEBUG_VM("INC\treg: %"PRIx64" -> %"PRIx64"\n", reg, (reg + amt) & 0xf); \
  reg = (reg + amt) & 0xf;

/* DEC: Decrement the register */
#define VM_DEC(reg, amt) \
  DEBUG_VM("DEC\treg: %"PRIx64" -> %"PRIx64"\n", reg, (reg - amt) & 0xf); \
  reg = (reg - amt) & 0xf;

/* READG: Read into the register from genome */
#define VM_READG(reg, mwp, msp, genome) \
  DEBUG_VM("READG:\tdna: %"PRIx64" reg: %"PRIx64" -> ", VM_GETINST(mwp, msp, genome), reg); \
  reg = (genome[mwp] >> msp) & 0xf;\
  DEBUG_VM("%"PRIx64 "\n", reg);

/* WRITEG: Write out from the register to genome */
#define VM_WRITEG(reg, mwp, msp, genome) \
  DEBUG_VM("WRITEG:\treg: %"PRIx64" dna: %"PRIx64" -> ", reg, VM_GETINST(mwp, msp, genome)); \
  genome[mwp] &= ~(((genome_t)0xf) << msp); \
  genome[mwp] |= reg << msp; \
  currentWord = genome[wordPtr]; \
  DEBUG_VM("%"PRIx64 "\n", VM_GETINST(mwp, msp, genome));

/* READB: Read into the register from buffer */
#define VM_READB(reg, mwp, msp, outputBuf) \
  DEBUG_VM("READB:\tbuf: %"PRIx64" reg: %"PRIx64" -> ", VM_GETINST(mwp, msp, outputBuf), reg); \
  reg = (outputBuf[mwp] >> msp) & 0xf; \
  DEBUG_VM("%"PRIx64 "\n", reg);

/* WRITEB: Write out from the register to buffer */
#define VM_WRITEB(reg, mwp, msp, outputBuf) \
  DEBUG_VM("WRITEB:\treg: %"PRIx64" buf: %"PRIx64" -> ", reg, VM_GETINST(mwp, msp, outputBuf)); \
  outputBuf[mwp] &= ~(((genome_t)0xf) << msp); \
  outputBuf[mwp] |= reg << msp; \
  DEBUG_VM("%"PRIx64 "\n", VM_GETINST(mwp, msp, outputBuf)); \

/* LOOP: Jump forward to matching REP if register is zero */
#define VM_LOOP(reg, wp, sp, lsp, ls_wp, ls_sp, stop, falseLoopDepth) \
  DEBUG_VM("LOOP:\tiptr: %"PRIx64" ", VM_GETPOS(wp, sp)); \
  DEBUG_VM("(reg: %"PRIx64" jump: ", reg); \
  if (reg) { \
    DEBUG_VM("%u ", 1); \
    if (lsp >= POND_DEPTH){ \
      stop = 1; \
    } else { \
      ls_wp[lsp] = wp; \
      ls_sp[lsp] = sp; \
      ++lsp; \
    } \
  } else { \
    DEBUG_VM("%u ", 0); \
    falseLoopDepth = 1; \
  } \
  DEBUG_VM("falseLoopDepth: %"PRIx64", loopstack %"PRIx64") -> iptr: %"PRIx64"\n", falseLoopDepth, lsp, VM_GETPOS(wp, sp));

/* REP: Jump back to matching LOOP if register is nonzero */
#define VM_REP(reg, wp, sp, lsp, ls_wp, ls_sp) \
  DEBUG_VM("REP:\tiptr: %"PRIx64" ", VM_GETPOS(wp, sp)); \
  DEBUG_VM("[(reg: %"PRIx64", loopstack: %"PRIx64") jump: ", reg, lsp); \
  if (lsp) { \
    --lsp; \
    if (reg) { \
      wp = ls_wp[lsp]; \
      sp = ls_sp[lsp]; \
      currentWord = cell->genome[wp]; \
      /* This ensures that the LOOP is rerun */ \
      DEBUG_VM("%u] -> iptr: %"PRIx64"\n", 1, VM_GETPOS(wp, sp)); \
      continue; \
    } \
  } \
  DEBUG_VM("%u] -> iptr: %"PRIx64"\n", 0, VM_GETPOS(wp, sp));

/* TURN: Turn in the direction specified by register */ \
#define VM_TURN(reg, facing) \
  DEBUG_VM("TURN:\treg: %"PRIu64" facing: %"PRIu64" -> ", reg, facing); \
  facing = reg & 3; \
  DEBUG_VM("%"PRIu64 "\n", facing);

/* XCHG: Skip next instruction and exchange value of register with it */
#define VM_XCHG(reg, wp, sp, genome, tmp) \
  DEBUG_VM("XCHG:\tiptr: %"PRIx64" ", VM_GETPOS(wp, sp)); \
  if ((sp += 4) >= SYSWORD_BITS) { \
    if (++wp >= POND_DEPTH_SYSWORDS) { \
      wp = EXEC_START_WORD; \
      sp = EXEC_START_BIT; \
    } else { \
      sp = 0; \
    } \
  } \
  tmp = reg; \
  reg = (genome[wp] >> sp) & 0xf; \
  DEBUG_VM("(reg: %"PRIx64" -> %"PRIx64" dna: %"PRIx64" -> ", tmp, reg, VM_GETINST(wp, sp, genome)); \
  genome[wp] &= ~(((genome_t)0xf) << sp); \
  genome[wp] |= tmp << sp; \
  currentWord = genome[wp]; \
  DEBUG_VM("%"PRIx64 ") -> iptr: %"PRIx64"\n", VM_GETINST(wp, sp, genome), VM_GETPOS(wp, sp));

/* KILL: Blow away neighboring cell if allowed with penalty on failure */
#define VM_KILL(reg, cell, tmcell, facing, tmp) \
  DEBUG_VM("KILL\t"); \
  tmcell = getNeighbor(cell,facing); \
  DEBUG_VM("target: %"PRIu64"\t parentid: %"PRIu64"\t", tmcell->ID, tmcell->parentID); \
  if (accessAllowed(tmcell,reg,0)) { \
    DEBUG_VM("SUCCESS\n"); \
    if (tmcell->generation > 2){ \
      ++statCounters.viableCellsKilled; \
    } \
    /* Filling first two words with 0xfffff... is enough */ \
    for (int j = 0; j < POND_DEPTH_SYSWORDS; j++) { \
      tmcell->genome[j] = ~((genome_t)0); \
    } \
    tmcell->ID = cellIdCounter; \
    tmcell->parentID = 0; \
    tmcell->lineage = cellIdCounter; \
    tmcell->generation = 0; \
    ++cellIdCounter; \
  } else if (tmcell->generation > 2) { \
    DEBUG_VM("FAILURE\tenergy: %"PRIu64" -> ", cell->energy); \
    tmp = cell->energy / FAILED_KILL_PENALTY; \
    if (cell->energy > tmp){ \
      cell->energy -= tmp; \
    } else { \
      cell->energy = 0; \
    } \
    DEBUG_VM("%"PRIu64"\n", cell->energy); \
  } else { \
    DEBUG_VM("FAILURE\n"); \
  }

/* SHARE: Equalize energy between self and neighbor if allowed */
#define VM_SHARE(reg, cell, tmcell, facing, tmp) \
  DEBUG_VM("SHARE\t"); \
  tmcell = getNeighbor(cell,facing); \
  if (accessAllowed(tmcell,reg,1)) { \
    DEBUG_VM("SUCCESS\tenergy: %"PRIu64" -> ", cell->energy); \
    if (tmcell->generation > 2) { \
      ++statCounters.viableCellShares; \
    } \
    tmp = cell->energy + tmcell->energy; \
    tmcell->energy = tmp / 2; \
    cell->energy = tmp - tmcell->energy; \
    DEBUG_VM("%"PRIu64"\n", cell->energy); \
  } else { \
    DEBUG_VM("FAILURE\n"); \
  }

/* STOP: End execution */
#define VM_STOP(stop) \
  DEBUG_VM("STOP\n"); \
  stop = 1;
