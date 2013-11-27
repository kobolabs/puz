/******************************************************************
 * libpuz - A .PUZ crossword library
 * Copyright(c) 2006 Josh Myer <josh@joshisanerd.com>
 * 
 * This code is released under the terms of the GNU General Public
 * License version 2 or later.  You should have receieved a copy as
 * along with this source as the file COPYING.
 ******************************************************************/

/*
 * puz.h -- General header-y stuff
 */

#ifndef __LIBPUZ_H__
#define __LIBPUZ_H__

#include <stdio.h>
#include <sys/types.h>

#include <unistd.h>
#include <stdlib.h>

#include <string.h>

// little-endian access routines

#define le_32(x) ( ((*(x+3)) << 24) + ((*(x+2)) << 16) + \
		   ((*(x+1)) <<  8) + ((*(x+0))) )
#define le_16(x) ( ((*(x+1)) << 8) + ((*(x+0)) << 0) )
#define le_8(x)  ( *(x) ) 


#define w_le_8(a, x) ( *(a) = (x))
#define w_le_16(a, x) *(a) = ((x) & 0xFF); *((a)+1) = (((x) & 0xFF00) >> 8)

// A puzzle header
struct puz_head_t {
  unsigned short cksum_puz;   // IV: cksum_cib

  unsigned char magic[12];

  unsigned short cksum_cib;  // IV: 0x0000

  unsigned char magic_10[4];
  unsigned char magic_14[4];

  unsigned char magic_18[4]; // looks to be ASCII "1.2\0"

  // 1c,1d unwritten memory noise
  unsigned short noise_1c;

  unsigned short scrambled_cksum;

  // 20-2b unwritten memory noise
  unsigned short noise_20;
  unsigned short noise_22;
  unsigned short noise_24;
  unsigned short noise_26;
  unsigned short noise_28;
  unsigned short noise_2a;

  /* These 8 bytes, with shorts in LE,  are the CIB */
  unsigned char width;
  unsigned char height;
  unsigned short clue_count;
  unsigned short x_unk_30;  // a bitmask of some sort
  unsigned short scrambled_tag;
};

// A whole, parsed puzzle file
struct puzzle_t {
  int sz;

  struct puz_head_t header;

  unsigned short calc_cksum_puzcib;
  unsigned short calc_cksums[4];
  unsigned char calc_magic10[4];
  unsigned char calc_magic14[4];

  unsigned char cib[8];

  unsigned char *base;

  unsigned char *solution;
  unsigned char *grid;

  unsigned char *title;
  unsigned char *author;
  unsigned char *copyright;

  unsigned char **clues;
  unsigned char *notes;
  int notes_sz;

  // The extra sections
  unsigned short grbs_cksum;
  unsigned short calc_grbs_cksum;
  unsigned char *grbs;  /* rebus squares */
  int rtbl_sz; // this is the # of entries, not the length of all table entries
  unsigned short rtbl_cksum;
  unsigned short calc_rtbl_cksum;
  unsigned char **rtbl; /* rebus table, indexed by entry, not a raw string! */

  unsigned short ltim_cksum;
  unsigned short calc_ltim_cksum;
  unsigned char *ltim;
  
  unsigned short gext_cksum;
  unsigned short calc_gext_cksum;
  unsigned char *gext;  /* circled squares */

  unsigned short rusr_cksum;
  unsigned short calc_rusr_cksum;
  unsigned char **rusr; /* rusr table - one (possibly null) entry per square */ 
  unsigned int   rusr_sz;
  /* The rusr board is an array of strings.  The squares without rusr
     entries will be NULL, rather than a pointer to an empty string.
     We cache the size of its representation in the binary because
     we have to calculate it several times.  This does not include
     the size of the null terminator for the whole rusr data section */

};

#define PUZ_FILE_BINARY 1
#define PUZ_FILE_TEXT   2
#define PUZ_FILE_UNKNOWN 4

/* Magic Numbers.  These are the numbers required for interoperability
   in various places within the files.  They are arrays of 8-bit
   values required for interoperability, and should not be construed
   as anything more than numbers. */

#define FILE_MAGIC { 65, 67, 82, 79, 83, 83, 38, 68, 79, 87, 78, 0 }
#define VER_MAGIC { 49, 46, 50, 0 }

#define MAGIC_10_MASK { 73, 67, 72, 69 }
#define MAGIC_14_MASK { 65, 84, 69, 68 }


#define TEXT_SUBMAGIC 60
#define TEXT_FILE_MAGIC { 60, 65, 67, 82, 79, 83, 83, 32, 80, 85, 90, 90, 76, 69, 62, 0 }
#define TEXT_FILE_TITLE_MAGIC { 60, 84, 73, 84, 76, 69, 62, 0 }
#define TEXT_FILE_AUTHOR_MAGIC { 60, 65, 85, 84, 72, 79, 82, 62, 0 }
#define TEXT_FILE_COPYRIGHT_MAGIC { 60, 67, 79, 80, 89, 82, 73, 71, 72, 84, 62, 0 }
#define TEXT_FILE_SIZE_MAGIC { 60, 83, 73, 90, 69, 62, 0 }
#define TEXT_FILE_GRID_MAGIC { 60, 71, 82, 73, 68, 62, 0 }
#define TEXT_FILE_CLUE0_MAGIC { 60, 65, 67, 82, 79, 83, 83, 62, 0 }
#define TEXT_FILE_CLUE1_MAGIC  { 60, 68, 79, 87, 78, 62, 0 }

#define GEXT_NORMAL  0
#define GEXT_CIRCLED 128

#define MAX_REBUS_SIZE 100
  /* the max size of a rebus string, in unsigned chars */

/* Sign-ified str ops to silence GCC4 signedness warnings */
#define Sstrlen(x) strlen((char *)(x))
#define Sstrdup(x) (unsigned char *)strdup((char *)(x))
#define Sstrndup(x,n) (unsigned char *)strndup((char *)(x),(n))
#define Sstrncpy(dest,src,n) ((unsigned char*)strncpy((char *)(dest),(char *)(src),(n)))
#define Sstrcpy(dest,src) ((unsigned char*)strcpy((char *)(dest),(char *)(src)))
#define Sstrchr(x,c) (unsigned char *)strchr((char *)(x), (c))
#define Satoi(x) atoi((char *)(x))
#define Sstrncmp(s1,s2,n) (int)strncmp((char *)(s1),(char *)(s2),n)

struct puzzle_t *puz_init(struct puzzle_t *puz);

struct puzzle_t *puz_load(struct puzzle_t *retval, int type, unsigned char *base, int sz);

void puz_deep_free(struct puzzle_t *puz);

unsigned short puz_cksum_region(unsigned char *base, int len, 
                                unsigned short cksum);
int puz_cksums_calc(struct puzzle_t *puz);
int puz_cksums_check(struct puzzle_t *puz);

int puz_size(struct puzzle_t *puz);

int puz_cksums_commit(struct puzzle_t *puz);
int puz_save(struct puzzle_t *puz, int type, unsigned char *base, int sz);

int puz_width_set(struct puzzle_t *puz, unsigned char val);
int puz_width_get(struct puzzle_t *puz);

int puz_height_set(struct puzzle_t *puz, unsigned char val);
int puz_height_get(struct puzzle_t *puz);

unsigned char * puz_solution_get(struct puzzle_t *puz);
unsigned char * puz_solution_set(struct puzzle_t *puz, unsigned char * val);

unsigned char * puz_grid_get(struct puzzle_t *puz);
unsigned char * puz_grid_set(struct puzzle_t *puz, unsigned char * val);

unsigned char * puz_title_get(struct puzzle_t *puz);
unsigned char * puz_title_set(struct puzzle_t *puz, unsigned char * val);

unsigned char * puz_author_get(struct puzzle_t *puz);
unsigned char * puz_author_set(struct puzzle_t *puz, unsigned char * val);

unsigned char * puz_copyright_get(struct puzzle_t *puz);
unsigned char * puz_copyright_set(struct puzzle_t *puz, unsigned char * val);

int puz_clear_clues(struct puzzle_t *puz);

int puz_clue_count_set(struct puzzle_t *puz, int val);
int puz_clue_count_get(struct puzzle_t *puz);

unsigned char * puz_clue_get(struct puzzle_t *puz, int n);
unsigned char * puz_clue_set(struct puzzle_t *puz, int n, unsigned char * val);

unsigned char * puz_notes_get(struct puzzle_t *puz);
unsigned char * puz_notes_set(struct puzzle_t *puz, unsigned char * val);

int puz_has_rebus(struct puzzle_t *puz);

unsigned char * puz_rebus_get(struct puzzle_t *puz);
unsigned char * puz_rebus_set(struct puzzle_t *puz, unsigned char * val);

int puz_rebus_count_set(struct puzzle_t *puz, int val);
int puz_rebus_count_get(struct puzzle_t *puz);

/* we provide two methods of interfacing with the rtable - a nice one
 * that deals with its logical structure (rtbl_get and set), and one
 * that deals with the raw binary representation (rtblstr_get and
 * set).  We actually store things in the logical way, so the former
 * are more efficient for the user.
 */
unsigned char * puz_rtbl_get(struct puzzle_t *puz, int n);
unsigned char * puz_rtbl_set(struct puzzle_t *puz, int n, unsigned char * val);

unsigned char * puz_rtblstr_get(struct puzzle_t *puz);
unsigned char ** puz_rtblstr_set(struct puzzle_t *puz, unsigned char * val);

int puz_clear_rtbl(struct puzzle_t *puz);

int puz_has_timer(struct puzzle_t *puz);
int puz_timer_elapsed_get(struct puzzle_t *puz);
int puz_timer_stopped_get(struct puzzle_t *puz);
unsigned char * puz_timer_set(struct puzzle_t *puz, int elapsed, int stopped);

int puz_has_extras(struct puzzle_t *puz);
unsigned char * puz_extras_get(struct puzzle_t *puz);
unsigned char * puz_extras_set(struct puzzle_t *puz, unsigned char * val);

int puz_has_rusr(struct puzzle_t *puz);
unsigned char ** puz_rusr_get (struct puzzle_t *puz);
unsigned char ** puz_rusr_set (struct puzzle_t *puz, unsigned char ** val);

// gets the binary form of the rusr structure
unsigned char * puz_rusrstr_get (struct puzzle_t *puz);

int puz_clear_rusr(struct puzzle_t *puz);

int puz_is_locked_get(struct puzzle_t *puz);
unsigned short puz_locked_cksum_get(struct puzzle_t *puz);
unsigned short puz_lock_set(struct puzzle_t *puz, unsigned short cksum);

int puz_unlock_solution(struct puzzle_t* puz, unsigned short code);
int puz_brute_force_unlock(struct puzzle_t* puz);

#endif /* ndef __LIBPUZ_H__ */
