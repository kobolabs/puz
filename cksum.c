/******************************************************************
 * libpuz - A .PUZ crossword library
 * Copyright(c) 2006 Josh Myer <josh@joshisanerd.com>
 * 
 * This code is released under the terms of the GNU General Public
 * License version 2 or later.  You should have receieved a copy as
 * along with this source as the file COPYING.
 ******************************************************************/

/*
 * cksum.c - Handles puzzle checksums
 */

#include <puz.h>

static unsigned short puz_cksum_cib(struct puzzle_t *puz);
static unsigned short puz_cksum(struct puzzle_t *puz, unsigned short cksum);
static unsigned short puz_cksum2(struct puzzle_t *puz, unsigned short cksum);

static void magic_gen_10(unsigned char *dest, unsigned short *sums);
static void magic_gen_14(unsigned char *dest, unsigned short *sums);
static unsigned short rtbl_gen(struct puzzle_t *puz);
static unsigned short rusr_gen(struct puzzle_t *puz);

/**
 * puz_cksum_region - Checksum a region using PUZ's rotate-and-sum
 *
 * @base: pointer to the memory region to checksum
 * @len: length to run the checksum over
 * @cksum: the initial value of the checksum
 *
 * This is used to run the PUZ checksum over chunks of memory.
 *
 * Return Value: it returns the new checksum value.
 */
unsigned short puz_cksum_region(unsigned char *base, int len,
                                unsigned short cksum) {
  int i;

  for(i = 0; i < len; i++) {
    if(cksum & 0x0001)
      cksum = (cksum >> 1) + 0x8000;
    else
      cksum = cksum >> 1;

    cksum += *(base+i);
  }

#if PRINT_CKSUM_RESULTS
  printf("\t%d %d\n", i, cksum);
#endif

  return cksum;
}

/**
 * puz_cksum_cib - Calculate the CIB Checksum for a puzzle
 *
 * @puz: the struct puzzle_t to calculate the CIB cksum for.
 *
 * This is an internal function.
 *
 * Return Value: This returns the checksum of the CIB.
 */
static unsigned short puz_cksum_cib(struct puzzle_t *puz) {
  unsigned short cksum;
  // First checksum header info
  cksum = puz_cksum_region(puz->cib, 8, 0);
  
  return cksum;
}

/**
 * puz_cksum - Checksum the puzzle in a puzzle_t
 *
 * @puz: the puzzle to calculate the checksum for
 * @cksum: initial value of the checksum
 *
 * This is an internal function.
 *
 * Return Value: new cksum.  Does not handle errors (XXX!)
 */
unsigned short puz_cksum(struct puzzle_t *puz, unsigned short cksum) {
  int i;

#define CKSUM_PIECEWISE 1
#if !(CKSUM_PIECEWISE)
  unsigned char *p;
#else 
  int puz_a = puz->header.width*puz->header.height;
#endif

  /*
    Checksumming: The file is a flat whap of text, followed by a
    delimited set of strings.  Checksumming works by doing the flat
    section, then doing each of the strings without its terminating
    NUL.

    The initial value of the flat section is 0x0000; the initial value
    for the first string is the checksum from the flat section.  Then,
    the result of string #N is used as the initial value for string
    #N+1.  The final result is stored in little-endian in the first
    two bytes of the result.
   */

#if CKSUM_PIECEWISE
  // checksum  solutions
  cksum = puz_cksum_region(puz->solution, puz_a, cksum);
  // Next checksum grid
  cksum = puz_cksum_region(puz->grid, puz_a, cksum);
  
  // title string w/NUL
  if(Sstrlen(puz->title) > 0) {
    cksum = puz_cksum_region(puz->title, Sstrlen(puz->title)+1, cksum);
  }

  // author string w/NUL
  if (Sstrlen(puz->author) > 0) {
    cksum = puz_cksum_region(puz->author, Sstrlen(puz->author)+1, cksum);
  }

  // copyright string w/NUL
  if (Sstrlen(puz->copyright) > 0) {
    cksum = puz_cksum_region(puz->copyright, Sstrlen(puz->copyright)+1, cksum);
  }
#else
  // find the beginning of the first clue
  p = puz->base + 0x2c + puz_a + puz_a;

  // skip title
  i = Sstrlen(p);
  p += i + 1;

  // skip author
  i = Sstrlen(p);
  p += i + 1;

  // skip copyright
  i = Sstrlen(p);
  p += i + 1;

  cksum = puz_cksum_region((puz->base)+0x2c+0x08, (p - puz->base) - 0x2c, cksum);
  
#endif
  // clue strings
  for(i = 0; i < puz->header.clue_count; i++)
    cksum = puz_cksum_region(puz->clues[i], Sstrlen(puz->clues[i]), cksum);
  // notes string w/NUL
  if (Sstrlen(puz->notes) > 0) {
    cksum = puz_cksum_region(puz->notes, Sstrlen(puz->notes)+1, cksum);
  }

  return cksum;
}


/**
 * puz_cksum2 - Calculate the secondary puzzle checksum
 *
 * @puz: the puzzle to calculate the secondary checksum for
 * @cksum: initial value of the checksum
 *
 * This is an internal function.
 *
 * The secondary checksum runs across the Title/Author/Copyright block
 * and then into the clues.  The Title/Author/Copyright block is done
 * blockwise (ie: including NULs) while the clues are done stringwise
 * (ie: not including their NULs).  The IV is typically 0x0000.
 *
 * Return Value: new secondary cksum.  Does not handle errors (XXX!)
 */
unsigned short puz_cksum2(struct puzzle_t *puz, unsigned short cksum) {
  int i;
  /*
    The same as the original checksum, but without including the grid
    or the solutions.  That is, start from the title and go from there.
   */

#define CKSUM_PIECEWISE 1
#if CKSUM_PIECEWISE

  // title string w/NUL
  if(Sstrlen(puz->title) > 0) {
    cksum = puz_cksum_region(puz->title, Sstrlen(puz->title)+1, cksum);
  }

  // author string w/NUL
  if (Sstrlen(puz->author) > 0) {
    cksum = puz_cksum_region(puz->author, Sstrlen(puz->author)+1, cksum);
  }

  // copyright string w/NUL
  if (Sstrlen(puz->copyright) > 0) {
    cksum = puz_cksum_region(puz->copyright, Sstrlen(puz->copyright)+1, cksum);
  }
#else
  int puz_a = puz->header.width*puz->header.height;
  char *p;

  // find the beginning of the first clue
  p = puz->base + 0x2c + puz_a + puz_a;

  // skip title
  i = Sstrlen(p);
  p += i + 1;

  // skip author
  i = Sstrlen(p);
  p += i + 1;

  // skip copyright
  i = Sstrlen(p);
  p += i + 1;

  cksum = puz_cksum_region((puz->base)+0x2c, (p - puz->base) - 0x2c, cksum);
  
#endif
  // clue strings
  for(i = 0; i < puz->header.clue_count; i++)
    cksum = puz_cksum_region(puz->clues[i], Sstrlen(puz->clues[i]), cksum);
  // notes string w/NUL
  if (Sstrlen(puz->notes) > 0) {
    cksum = puz_cksum_region(puz->notes, Sstrlen(puz->notes)+1, cksum);
  }

  return cksum;
}

/**
 * magic_gen_10 - Generate the four magic bytes at offset 0x10
 *
 * @dest: pointer to where to store these (they're stored at 0..3, not 10..13)
 * @sums: array of 4 unsigned short checksums.  See below for details.
 *
 * This is an internal function.
 *
 * This is the same as magic_gen_14(), except that the low bytes are
 * used in this function.
 *
 * The checksums to put in sums are:
 * 0. The CIB sum (from puz_cksum_cib(puz))
 * 1. The solution sum (puz_cksum_region(puz->solution, sol_len, 0x0000))
 * 2. The grid sum (puz_cksum_region(puz->grid, grid_len, 0x0000))
 * 3. The secondary puz sum (puz_cksum2(puz)) 
 *
 * The low bytes of these are then masked with the magic_10_mask from
 * puz.h and placed into dest[0]..dest[3].
 *
 * Return value: void.
 */
static void magic_gen_10(unsigned char *dest, unsigned short *sums) {
  unsigned char magic[4] = MAGIC_10_MASK;

  int i;

  for(i = 0; i < 4; i++) {
    unsigned char c = sums[i] & 0xFF;
    
    dest[i] = c ^ magic[i];

    // printf("10: %d: %x ^ %x -> %x\n", i, c, magic[i], dest[i]);
  }

  return;
}

/**
 * magic_gen_14 - Generate the four magic bytes at offset 0x10
 *
 * @dest: pointer to where to store these (they're stored at 0..3, not 14..17)
 * @sums: array of 4 unsigned short checksums.  See below for details.
 *
 * This is an internal function.
 *
 * This is the same as magic_gen_10(), except that the high bytes are
 * used in this function.
 *
 * The checksums to put in sums are:
 * 0. The CIB sum (from puz_cksum_cib(puz))
 * 1. The solution sum (puz_cksum_region(puz->solution, sol_len, 0x0000))
 * 2. The grid sum (puz_cksum_region(puz->grid, grid_len, 0x0000))
 * 3. The secondary puz sum (puz_cksum2(puz)) 
 *
 * The high bytes of these are then masked with the magic_10_mask from
 * puz.h and placed into dest[0]..dest[3].
 *
 * Return value: void.
 */
static void magic_gen_14(unsigned char *dest, unsigned short *sums) {
  unsigned char magic[4] = MAGIC_14_MASK;

  int i;

  for(i = 0; i < 4; i++) {
    dest[i] = ((sums[i] & 0xFF00) >> 8) ^ magic[i];
  }

  return;
}

/* rtbl_gen and rusr_gen calculate the checksums for sections that we
   aren't storing in their binary form */
static unsigned short rtbl_gen(struct puzzle_t *puz) {
  unsigned char *rtbl_str;
  unsigned short ck;

  rtbl_str = puz_rtblstr_get(puz);
  if (rtbl_str) {
    ck = puz_cksum_region(rtbl_str, Sstrlen(rtbl_str), 0x0000);
    free(rtbl_str);
    return ck;
  } else {
    return 0;
  }
}

static unsigned short rusr_gen(struct puzzle_t *puz) {
  unsigned char *rusr_str;
  unsigned short ck;

  rusr_str = puz_rusrstr_get(puz);
  if (rusr_str) {
    ck = puz_cksum_region(rusr_str, puz->rusr_sz, 0x0000);
    free(rusr_str);
    return ck;
  } else {
    return 0;
  }
}

/**
 * puz_cksums_calc - Calculate the checksums for a puzzle
 *
 * @puz: puzzle to calculate the checksums for
 *
 * This function is used to calculate the checksums for a given
 * puzzle.  It stores them in special calculated-value storage within
 * the puzzle_t struct.  After calling this function, you can call
 * puz_cksums_check(puz) to check the values, and
 * puz_cksums_commit(puz) to place the checksums into the headers.
 *
 * Return Value: 0.
 */
int puz_cksums_calc(struct puzzle_t *puz) {
  unsigned short soln, puz0, cib, puzcib, grid;

  w_le_8(puz->cib+0, puz->header.width);
  w_le_8(puz->cib+1, puz->header.height);
  w_le_16(puz->cib+2, puz->header.clue_count);
  w_le_16(puz->cib+4, puz->header.x_unk_30);
  w_le_16(puz->cib+6, puz->header.scrambled_tag);

  puz0 = puz_cksum2(puz, 0x0000);
  cib = puz_cksum_cib(puz);
  puzcib = puz_cksum(puz, cib);

  int bd_size = puz->header.width*puz->header.height;

  grid = puz_cksum_region(puz->grid, bd_size, 0x0000);
  soln = puz_cksum_region(puz->solution, bd_size, 0x0000);

  // printf("Cksums: %04x %04x %04x %04x\n", soln, cib, puz0, grid);

  puz->calc_cksum_puzcib = puzcib;
  
  puz->calc_cksums[0] = cib;
  puz->calc_cksums[1] = soln;
  puz->calc_cksums[2] = grid;
  puz->calc_cksums[3] = puz0;

  magic_gen_10(puz->calc_magic10, puz->calc_cksums);
  magic_gen_14(puz->calc_magic14, puz->calc_cksums);

  if (puz_has_rebus(puz)) {
    puz->calc_grbs_cksum = 
      puz_cksum_region(puz->grbs, bd_size, 0x0000);
    puz->calc_rtbl_cksum = rtbl_gen(puz);
  }

  if (puz_has_timer(puz)) {
    puz->calc_ltim_cksum = puz_cksum_region(puz->ltim, Sstrlen(puz->ltim),
                                            0x0000);
  }

  if (puz_has_extras(puz)) {
    puz->calc_gext_cksum = 
      puz_cksum_region(puz->gext, bd_size, 0x0000);
  }

  if (puz_has_rusr(puz)) {
    puz->calc_rusr_cksum = rusr_gen(puz);
  }

  return 0;
}


/**
 * puz_cksums_check - Check the checksums for a puzzle
 *
 * @puz: puzzle to check the checksums of
 *
 * This function is used to check the checksums parsed out of the file
 * with those calculated by puz_chksums_calc(puz).  It returns zero on
 * success, or the number of errors detected on error.  There is
 * currently no way to see exactly which values are incorrect through
 * the API.
 *
 * Return Value: 0 on success, positive number of errors on error(s).
 */

int puz_cksums_check(struct puzzle_t *puz) {
  int i;

  int retval = 0;

  puz_cksums_calc(puz);

  if(puz->header.cksum_cib != puz->calc_cksums[0]) {
    printf("CIBs differ: got %04x, calc %04x\n", 
	   puz->header.cksum_cib, puz->calc_cksums[0]);
    retval++;
  }
  if(puz->header.cksum_puz != puz->calc_cksum_puzcib) {
    printf("PUZ cksums differ: got %04x, calc %04x\n", 
	   puz->header.cksum_puz, puz->calc_cksum_puzcib);
    retval++;
  }

  for(i = 0; i < 4; i++) {
    if(puz->header.magic_10[i] != puz->calc_magic10[i]) {
      printf("magic 10 %d differs: got %02x, calc %02x\n", 
	     i, puz->header.magic_10[i], puz->calc_magic10[i]);
      retval++;
    }
  }

  for(i = 0; i < 4; i++) {
    if(puz->header.magic_14[i] != puz->calc_magic14[i]) {
      printf("magic 14 %d differs: got %02x, calc %02x\n", 
	     i, puz->header.magic_14[i], puz->calc_magic14[i]);
      retval++;
    }
  }

  if (puz_has_rebus(puz)) {
    if(puz->grbs_cksum != puz->calc_grbs_cksum) {
      printf("GRBS checksum differs: got %02x, calc %02x\n", 
             puz->grbs_cksum, puz->calc_grbs_cksum);
      retval++;
    }
    if(puz->rtbl_cksum != puz->calc_rtbl_cksum) {
      printf("RTBL checksum differs: got %02x, calc %02x\n", 
             puz->rtbl_cksum, puz->calc_rtbl_cksum);
      retval++;
    }
  }

  if (puz_has_timer(puz)) {
    if(puz->ltim_cksum != puz->calc_ltim_cksum) {
      printf("LTIM checksum differs: got %02x, calc %02x\n",
             puz->ltim_cksum, puz->calc_ltim_cksum);
      retval++;
    }
  }

  if (puz_has_extras(puz)) {
    if(puz->gext_cksum != puz->calc_gext_cksum) {
      printf("GEXT checksum differs: got %02x, calc %02x\n", 
             puz->gext_cksum, puz->calc_gext_cksum);
      retval++;
    }
  }

  if (puz_has_rusr(puz)) {
    if(puz->rusr_cksum != puz->calc_rusr_cksum) {
      printf("RUSR checksum differs: got %02x, calc %02x\n", 
             puz->rusr_cksum, puz->calc_rusr_cksum);
      retval++;
    }
  }

  return retval;
}

/**
 * puz_cksums_commit - commit the calculated checksums to a puzzle
 *
 * @puz: puzzle to use
 *
 * This function is used to commit the calculated checksums in a
 * puzzle.  This is used mainly to fill in the checksum values for
 * PUZs loaded from the text format.
 *
 * Return Value: 0 (no errors checked).
 */
int puz_cksums_commit(struct puzzle_t *puz) {
  puz_cksums_calc(puz);

  puz->header.cksum_puz = puz->calc_cksum_puzcib;
  puz->header.cksum_cib = puz->calc_cksums[0];

  memcpy(puz->header.magic_10, puz->calc_magic10, 4);
  memcpy(puz->header.magic_14, puz->calc_magic14, 4);

  if (puz_has_rebus(puz)) {
    puz->grbs_cksum = puz->calc_grbs_cksum;
    puz->rtbl_cksum = puz->calc_rtbl_cksum;
  }

  if (puz_has_timer(puz)) {
    puz->ltim_cksum = puz->calc_ltim_cksum;
  }

  if (puz_has_extras(puz)) {
    puz->gext_cksum = puz->calc_gext_cksum;
  }

  if (puz_has_rusr(puz)) {
    puz->rusr_cksum = puz->calc_rusr_cksum;
  }

  return 0;
}
