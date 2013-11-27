/******************************************************************
 * libpuz - A .PUZ crossword library
 * Copyright(c) 2006 Josh Myer <josh@joshisanerd.com>
 * 
 * This code is released under the terms of the GNU General Public
 * License version 2 or later.  You should have receieved a copy as
 * along with this source as the file COPYING.
 ******************************************************************/

/*
 * load.c -- Puzzle loading (binary and text) routines
 */


#include <puz.h>

#include <ctype.h>
#include <string.h>

#ifndef isspace
int isspace(int C);
#endif

static struct puz_head_t *read_puz_head(struct puz_head_t *h, unsigned char *base);
static struct puzzle_t *puz_load_bin(struct puzzle_t *puz, unsigned char *base, int sz);
static unsigned char *strnchr(unsigned char *buf, int n, unsigned char c);
static int delim_memcmp(unsigned char *input, unsigned char *buf);

/* A structure for linked-lists of input text */
struct line_list {
  unsigned char *line;
  struct line_list *next;
};

static struct line_list *line_append(struct line_list *list, unsigned char *line);
static int line_length(struct line_list *list);
static unsigned char *line_concat(struct line_list *list);
static int line_count(struct line_list *root);
static void line_clear(struct line_list *root);


static unsigned char *mkgrid(unsigned char *soln);

/**
 * read_puz_head - Read in puz_head_t values from a buffer
 * 
 * @h: pointer to the struct puz_head_t to fill in.  If NULL, is malloc'd for you.
 * @base: pointer to the buffer containing the header to read in (required)
 *
 * This is an internal function
 *
 * Return value: Returns a pointer to the filled-in puz_head_t.  If h
 * was NULL initially, returns the allocated pointer.
 */
static struct puz_head_t *read_puz_head(struct puz_head_t *h, unsigned char *base) {
  int i;

  if(NULL == h) {
    h = malloc(sizeof(struct puz_head_t));
    if(NULL == h) {
      perror("malloc");
      return NULL;
    }
  }

  i = 0;

  h->cksum_puz = le_16(base+i);
  i += 2;

  memcpy(h->magic, base+i, 12);
  i += 12;

  h->cksum_cib = le_16(base+i);
  i += 2;

  memcpy(h->magic_10, base+i, 4);
  i += 4;

  memcpy(h->magic_14, base+i, 4);
  i += 4;

  memcpy(h->magic_18, base+i, 4);
  i += 4;

  h->noise_1c = le_16(base+i);
  i += 2;

  h->scrambled_cksum = le_16(base+i);
  i += 2;

  h->noise_20 = le_16(base+i);
  i += 2;
  h->noise_22 = le_16(base+i);
  i += 2;
  h->noise_24 = le_16(base+i);
  i += 2;
  h->noise_26 = le_16(base+i);
  i += 2;
  h->noise_28 = le_16(base+i);
  i += 2;
  h->noise_2a = le_16(base+i);
  i += 2;

  h->width = le_8(base+i);
  i++;
  h->height = le_8(base+i);
  i++;
  h->clue_count = le_16(base+i);
  i += 2;
  
  h->x_unk_30 = le_16(base+i);
  i += 2;
  h->scrambled_tag = le_16(base+i);
  i += 2;

  return h;
}

// Each of these special section readers return the actual number
// of bytes they advance.  A 0 signals an error

/**
 * load_grbs_bin - Reads the GRBS and RTBL sections
 *
 * @puz: pointer to the struct puzzle_t to fill in.  Must not be NULL
 * @base: pointer to the buffer containing the puzzle to load from
 * @sz: expected size of the GRBS section, not counting the RTBL.
 * 
 * This is an internal function
 *
 * Return value: The number of bytes read, or 0 on an error.
 */
unsigned int load_grbs_bin(struct puzzle_t *puz, unsigned char *base,
                           unsigned short sz) {
  if(NULL == puz) {
    return 0;
  }

  int i = 0;
  int bd_sz = puz->header.width*puz->header.height;

  puz->grbs_cksum = le_16(base+i);
  i += 2;

  // rebus grid
  puz->grbs = (unsigned char*) malloc(bd_sz * sizeof(unsigned char));
  if(NULL == puz->grbs) {
    return 0;
  }

  memcpy(puz->grbs, base+i, bd_sz);
  i += bd_sz;
  i += 1; // NULL terminator

  // check if there actually are any rebuses
  int rbssum = 0;
  int rbsj = 0;
  for (; rbsj < bd_sz; rbsj++) {
    rbssum += puz->grbs[rbsj];
  }
  // if rbssum is 0, the rebus grid is empty and should be ignored
  if (rbssum == 0) {
    free(puz -> grbs);
    puz -> grbs = NULL;
  }        
    
  if (0 != Sstrncmp(base+i,"RTBL",4)) {
    if (rbssum != 0) {
      printf("Rebus grid is missing a rebus table: sz: %d, i: %d\n", sz, i);
      free(puz->grbs);
      return 0;
    }
  } else {
    i += 4; // skip RTBL

    // parse size of the RTBL itself...this is NOT the number of entries
    // we don't _technically_ need it, but we keep it as a sanity check
    unsigned int rtbl_strsz = le_16(base+i);
    i += 2;

    if (rbssum != 0) {
      puz->rtbl_cksum = le_16(base+i);
    }
    i += 2;


    // load the rebus table from its string representation
    if (rbssum != 0) {
      puz_rtblstr_set(puz, base + i);
    }
    i += rtbl_strsz;

    i += 1; // NULL terminator
  }

  return i;
}

/**
 * load_ltim_bin - Reads the LTIM section
 *
 * @puz: pointer to the struct puzzle_t to fill in.  Must not be NULL
 * @base: pointer to the buffer containing the LTIM section
 * @ltim_sz: expected size of the LTIM section
 * 
 * This is an internal function
 *
 * Return value: The number of bytes read, or 0 on an error.
 */
unsigned int load_ltim_bin(struct puzzle_t *puz, unsigned char *base,
                           unsigned short ltim_sz) {
  if(NULL == puz) {
    return 0;
  }

  int i = 0;

  puz->ltim_cksum = le_16(base+i);
  i += 2;

  puz->ltim = calloc(sizeof(unsigned char), ltim_sz+1);
char *dest = (char*)puz->ltim;
char *src = (char*)base+i;
strncpy(dest, src, ltim_sz);
//  Sstrncpy(puz->ltim, base+i, ltim_sz);
  
  i += ltim_sz + 1;

  return i;
}

/**
 * load_gext_bin - Reads the GEXT section
 *
 * @puz: pointer to the struct puzzle_t to fill in.  Must not be NULL
 * @base: pointer to the buffer containing the gext section
 * @gext_sz: expected size of the GEXT section
 * 
 * This is an internal function
 *
 * Return value: The number of bytes read, or 0 on an error.
 */
unsigned int load_gext_bin(struct puzzle_t *puz, unsigned char *base) {

  if(NULL == puz) {
    return 0;
  }

  int i = 0;
  int bd_sz = puz->header.width*puz->header.height;

  puz->gext_cksum = le_16(base+i);
  i += 2;

  // extras grid
  puz->gext = (unsigned char *) malloc(bd_sz * sizeof(unsigned char));
  memcpy(puz->gext, base+i, bd_sz);
  i += bd_sz;

  i += 1; // NULL terminator

  return i;
}

/**
 * load_rusr_bin - Reads the RUSR section
 *
 * @puz: pointer to the struct puzzle_t to fill in.  Must not be NULL
 * @base: pointer to the buffer containing the gext section
 * @rusr_sz: expected size of the RUSR section
 * 
 * This is an internal function
 *
 * Return value: The number of bytes read, or 0 on an error.
 */
unsigned int load_rusr_bin(struct puzzle_t *puz, unsigned char *base) {
  if(NULL == puz) {
    return 0;
  }

  int i = 0;
  int bd_sz = puz->header.width*puz->header.height;

  puz->rusr_cksum = le_16(base+i);
  i += 2;

  // rusr grid
  puz->rusr = (unsigned char **) malloc(bd_sz * sizeof(unsigned char *));
  int j;
  for(j = 0; j < bd_sz; j++) {
    if(base[i]) {
      // these strings are required to be null terminated...
      // but of course we could be given an ill-formed file, so
      // we use a max size (100) for fun.
      unsigned char* usr_rebus = 
        Sstrndup(base+i, MAX_REBUS_SIZE * (sizeof(unsigned char)));
      puz->rusr[j] = usr_rebus;
      i += Sstrlen(usr_rebus) + 1;
    } else {
      puz->rusr[j] = NULL;
      i++;
    }
  }
  
  puz->rusr_sz = i - 2;
  i += 1; // NULL terminator

  return i;
}



/**
 * puz_load_bin - Load a puzzle in binary format
 *
 * @puz: pointer to the struct puzzle_t to fill in.  If NULL, will be allocated for you.
 * @base: pointer to the buffer containing the puzzle to load from (required)
 * @sz: size of the puzzle file in the buffer (required)
 * 
 * This is an internal function
 *
 * Return value: NULL on error, else a pointer to the filled-in struct
 * puzzle_t.  If puz was NULL, this pointer is the newly-allocated
 * puzzle_t.
 */
static struct puzzle_t *puz_load_bin(struct puzzle_t *puz, unsigned char *base, int sz) {
  int i, j;
  int didmalloc = 0;

  if( (NULL == base) || (sz < 0x34)) {
    printf("NULL region (%p) or size (%d) too small!\n", base, sz);
    return NULL;
  }

  if(NULL == puz) {
    puz = (struct puzzle_t *)malloc(sizeof(struct puzzle_t));
    if(NULL == puz) {
      perror("malloc");
      return NULL;
    }
    didmalloc = 1;
  }

  memset(puz, 0, sizeof(struct puzzle_t));

  puz->base = base;
  puz->sz = sz;
	
  if(NULL == read_puz_head(&(puz->header), base)) {
    printf("Error reading header!\n");

    if(didmalloc) {
      free(puz);
      puz = NULL;
    }

    return NULL;
  }

  memcpy(puz->cib, base+0x2c, 8);

  i = 0x34;
  int bd_sz = puz->header.width*puz->header.height;
  
  puz->solution = calloc(sizeof(unsigned char), 1+bd_sz);
char*dest = (char*)puz->solution;
char*src = (char*)base+i;
strncpy(dest,src,bd_sz);
//  Sstrncpy(puz->solution, base+i, bd_sz);
  puz->solution[bd_sz] = 0;

  i += bd_sz;

  puz->grid = calloc(sizeof(unsigned char), 1+bd_sz);
dest = (char*)puz->grid;
src = (char*)base+i;
strncpy(dest,src,bd_sz);
//  Sstrncpy(puz->grid, base+i, bd_sz);
  i += bd_sz;
  
  puz->title = Sstrdup(base+i);
  i += Sstrlen(puz->title) + 1;

  puz->author = Sstrdup(base+i);
  i += Sstrlen(puz->author) + 1;

  puz->copyright = Sstrdup(base+i);
  i += Sstrlen(puz->copyright) + 1;

  puz->clues = (unsigned char **)malloc(puz->header.clue_count * sizeof(unsigned char *));
  if(NULL == puz->clues) {
    perror("malloc");
    return NULL; /* XXX cleanup */
  }

  for(j = 0; i < sz && j < puz->header.clue_count; j++) {
    puz->clues[j] = Sstrdup(base+i);

    if(NULL == puz->clues[j]) {
      perror("Sstrdup");
      return NULL; /* XXX cleanup */
    }

    i += Sstrlen(puz->clues[j]) + 1;
  }
 
  if(j != puz->header.clue_count) {
    printf("Appear to have run out of clues: sz: %d, i: %d, clues: %d, j: %d\n",
	   sz, i, puz->header.clue_count, j);

    if(didmalloc) {
      free(puz);
      puz = NULL;
    }

    return NULL; /* XXX cleanup */
  }

  puz->notes = NULL;
  if(i < sz) {
    puz->notes = Sstrdup(base+i);
    if(NULL == puz->notes) {
      perror("strdup");
    }
    puz->notes_sz = Sstrlen(puz->notes);
    i += puz->notes_sz + 1;
  }

  /* At this point, any number of additional sections can appear.
   * The supported ones are:
   * 
   * GRBS/RTBL (rebus stuff)
   * GEXT      (circled squares, incorrect flags)
   * LTIM      (time)
   * RUSR      (user entered rebus solutions)
   * 
   * We support these section in any order, except that RTBL must
   * immediately follow GRBS
   */

  puz->grbs = NULL;
  puz->rtbl = NULL;
  puz->rtbl_sz = 0;

  puz->ltim=NULL;

  puz->gext=NULL;

  puz->rusr=NULL;
  puz->rusr_sz=0;

  unsigned short section_sz;
  unsigned int advance;

  while (i+5 < sz) {
    // get the size of the section to come
    section_sz = le_16(base+i+4);
    if (0 == Sstrncmp(base+i,"GRBS",4)) {
      advance = load_grbs_bin(puz,base+i+6,section_sz);
    } else if (0 == Sstrncmp(base+i,"LTIM",4)) {
      advance = load_ltim_bin(puz,base+i+6,section_sz);
    } else if (0 == Sstrncmp(base+i,"GEXT",4)) {
      advance = load_gext_bin(puz,base+i+6);
    } else if (0 == Sstrncmp(base+i,"RUSR",4)) {
      advance = load_rusr_bin(puz,base+i+6);
    } else {
      printf("Warning: unknown board section %.4s", base+i);
      i += 6 + section_sz + 1;
      continue;
    }
    if (advance == 0) {
      printf("Error reading %.4s section", base+i);
      if(didmalloc) {
        free(puz);
        puz = NULL;
        break;
      }
    }
    i += 6 + advance;
  }

  return puz;
}


/**
 * strnchr - find a unsigned character within the first n bytes of a buffer
 *
 * @buf: buffer to search through
 * @n: maximum search length (look in buf[0]..buf[n-1]
 * @c: unsigned character value to look for.
 *
 * Returns a pointer to the unsigned character if found, else points to buf[n]
 * (which might not actually be in buf!)
 */

static unsigned char *strnchr(unsigned char *buf, int n, unsigned char c) {
  int i;
  for(i = 0; i < n; i++) {
    if(*(buf+i) == c)
      return (buf+i);
  }
  return(buf+n);
}

/**
 * get_one_line - get one [\r\n]{1,2}-delimited line
 *
 * This is an internal function
 *
 * This crazy little function finds the end of the current line, no
 * matter how it's been delimited.  It looks for all three of the
 * major file formats, and should cleanly handle excruciatingly
 * malformed input files.
 *
 * The contents of **buf are updated (ie: the cursor is advanced), and
 * it returns a fresh copy of the next line.  This result needs to be
 * freed by the caller.  We have to do it this way because our input
 * may be an mmap'd file which is read-only.
 */
unsigned char *get_one_line(unsigned char **buf, int *n) {
  unsigned char *b, *c, *d, *end;
  int extra = 0;
  b = *buf;

  while(isspace(*b) && *b != '\r' && *b != '\n' )
    b++;

  c = strnchr(b, *n, '\r');
  d = strnchr(b, *n, '\n');

  if(c == NULL) // if no \r found, use the \n (unix)
    end = d;
  else if(d == NULL) // if no \n found, use the \r (macos)
    end = c;
  else {
    if(d == (c+1)) {// if find \r\n, use \r, skip \n (DOS)
      end = c;
      extra = 1;
    } else if(c == (d+1)) { // if \n\r, use \n, skip \r (??!)
      end = c;
      extra = 1;
    } else {
      if(d == c) { // we must be at end of the buffer
	extra = -1; // since we're not consuming a \r or \n
	end = d;
      } else
	end = (d > c ? d : c); // if find both, but not together, use
      // earliest (??!!!)
    }
  }

  d = end;
  while(d > b && isspace(*d))
    d--;
  /* d now points to the last unsigned character of the string */

  /* Diagram of what our pointers look like:
     0123456
     __XYZ_\n
     __b d
     
     So, d = 4, b = 2, but our string is 3 bytes long (it includes d).
     We thus have (d+1)-b bytes of string, so we need d+1-b+1 bytes to
     hold the string along with its terminating NUL */
  c = (unsigned char *)malloc(d+1-b+1);
  memset(c, 'X', d+1-b+1);
  memcpy(c, b, d+1-b);
  c[d+1-b] = '\0';

  /* update the remaining bytes count, and advance the cursor */
  *n = (*n - (end+1+extra - *buf));
  *buf = (end + 1 + extra);

  return c;
}

/**
 * delim_memcmp -- compare a string with a magic number
 * 
 * @input: input string to look through
 * @buf: buffer containing magic number
 * 
 * This is an internal function
 *
 * This is used to compare an input string with a magic number buffer,
 * to see if they match.  
 *
 * Returns 0 if they match, -1 if they don't.
 */
static int delim_memcmp(unsigned char *input, unsigned char *buf) {
  int i;

  for(i = 0; input[i] != 0 && buf[i] != 0; i++) {
    if(input[i] != buf[i])
      return -1;
  }
  return 0;
}


static struct line_list *line_append(struct line_list *list, unsigned char *line) {
  struct line_list *cur;

  for(cur = list; cur->next != NULL; cur = cur->next) 
    {}; // Do nothing

  cur->line = line;

  cur->next = (struct line_list *)malloc(sizeof(struct line_list));
  cur = cur->next;

  memset(cur, 0, sizeof(struct line_list));

  cur->next = NULL;
  return cur->next;
}

static int line_length(struct line_list *list) {
  struct line_list *cur;
  int i = 0;

  for(cur = list; cur != NULL && cur->line != NULL; cur = cur->next) {
    i += Sstrlen(cur->line);
  }
  return i;
}

static unsigned char *line_concat(struct line_list *list) {
  struct line_list *cur;
  unsigned char *retval;
  int i, len;

  printf("   ** LC: %p\n ", list);

  len = line_length(list);

  retval = (unsigned char *)malloc(len);
  i = 0;
  for(cur = list; cur != NULL && cur->line != NULL; cur = cur->next) {
    memcpy(retval+i, cur->line, Sstrlen(cur->line));
    i += Sstrlen(cur->line);
  }
  retval[len] = 0;

  return retval;  
}

static int line_count(struct line_list *root) {
  struct line_list *cur;
  int i;

  printf("   ** Ln: %p\n ", root);

  for(i = 0, cur = root; cur->line != NULL; cur = cur->next)
    i++;

  return i;
}

static void line_clear(struct line_list *root) {
  /* doesn't free root! */
  struct line_list *cur = NULL, *next = NULL;

  printf("   ** LW: %p\n ", root);

  if(NULL == root)
    return;

  /* Since we don't want to free root, we have to manually handle
     cleaning it up.
   */

  free(root->line);
  root->line = NULL;

  if(NULL == root->next) {
    return;
  }

  cur = root->next;
  next = cur->next;

  for(; cur != NULL;) {
    free(cur->line);
    free(cur);
    cur = next;
    if(cur != NULL)
      next = cur->next;
  } 

  root->next = NULL;
  return;
}

/**
 * mkgrid -- make a grid out of a solution
 *
 * @soln: pointer to the NUL-delimited string containing the solution
 *
 * This function creates a copy of the solution, then simply replaces
 * any unsigned character not already a '.' with a '-', forcing it into grid
 * format.
 *
 * Return Value: NULL on error, pointer to new grid string on success.
 */
static unsigned char *mkgrid(unsigned char *soln) {
  unsigned int i;
  unsigned char *grid = Sstrdup(soln);

  if(NULL == grid)
    return NULL;

  for(i = 0; i < Sstrlen(grid); i++) {
    if(grid[i] != '.')
      grid[i] = '-';
  }
  return grid;
}

/**
 * puz_load_text - Load a puzzle in text format
 *
 * @puz: pointer to the struct puzzle_t to fill in.  If NULL, will be allocated for you.
 * @base: pointer to the buffer containing the puzzle to load from (required)
 * @sz: size of the puzzle file in the buffer (required)
 * 
 * This is an internal function
 *
 * This is implemented as a state machine which keeps track of which
 * section of the file it's currently in.  It reads in values
 * linewise, then checks if they are magic lines.  If a line isn't
 * magic, it gets appended to a list of read lines.  If it a magic
 * line, the machine notes that it's about to exit the current state.
 *
 * Then, depending on the current state, we process the list of lines
 * and store the parsed results into the puzzle_t.
 *
 * After all this processing, the line_list is cleared (unless the
 * state handler set do_clear to 0, which is used in grabbing clues),
 * and the state advanced.
 *
 * This proceeds until the final state is reached, then the puzzle is
 * checksummed and returned.
 *
 * Return value: NULL on error, else a pointer to the filled-in struct
 * puzzle_t.  If puz was NULL, this pointer is the newly-allocated
 * puzzle_t.
 */
static struct puzzle_t *puz_load_text(struct puzzle_t *puz, unsigned char *base, int sz) {
  /* And this, boys and girls, is why Josh Hates Delimited Formats */

  unsigned char *cursor;
  unsigned char *line;

  int remaining = sz;

  unsigned char magics[9][17] = { {}, /* no initial magic */
                         TEXT_FILE_MAGIC,
			 TEXT_FILE_TITLE_MAGIC,
			 TEXT_FILE_AUTHOR_MAGIC,
			 TEXT_FILE_COPYRIGHT_MAGIC,
			 TEXT_FILE_SIZE_MAGIC,
			 TEXT_FILE_GRID_MAGIC,
			 TEXT_FILE_CLUE0_MAGIC,
			 TEXT_FILE_CLUE1_MAGIC };

  enum states { STATE_INIT = 0, 
		STATE_FILE = 1, 
		STATE_TITLE = 2, 
		STATE_AUTHOR = 3,
		STATE_COPYRIGHT = 4,
		STATE_SIZE = 5,
		STATE_GRID = 6,
		STATE_CLUE0 = 7,
		STATE_CLUE1 = 8,
		STATE_FINAL = 9};

  int state = STATE_INIT; /* current state */
  int state_d = 0; /* state has changed */
  int do_clear = 0; /* should we clear the linelist after this loop? */

  struct line_list linelist;

  /* A quick sanity check */
  if(*(base) != TEXT_SUBMAGIC)
    return NULL;

  /* initialize our structure */
  puz = puz_init(puz);

  /* Set up our state machine */
  memset(&linelist, 0, sizeof(struct line_list));
  cursor = base;

  /* Run the state machine */
  while(state != STATE_FINAL) {
    line = get_one_line(&cursor, &remaining);

    printf("Got Line: %s\n", line);

    state_d = 0;

    if(remaining <= 0) {
      line_append(&linelist, line);
      state_d = 1;
    }

    if(line[0] == TEXT_SUBMAGIC) {
      if(0 != delim_memcmp(line, magics[state+1])) {
	printf("Didn't get the right magic line at state %d (%s not %s)!\n", 
	       state, line, magics[state+1]);
	return NULL;
      }
      state_d = 1;
    }

    if(0 == state_d) {
      line_append(&linelist, line);
    } else {
      do_clear = 1;

      printf("Exiting state %d\n", state);
      printf("Values: %s\n\n", line_concat(&linelist));

      switch(state) {
      case STATE_INIT:
	break;
      case STATE_FILE:
	break;
      case STATE_TITLE:
	puz_title_set(puz, line_concat(&linelist));
	break;
      case STATE_AUTHOR:
	puz_author_set(puz, line_concat(&linelist));
	break;
      case STATE_COPYRIGHT:
	puz_copyright_set(puz, line_concat(&linelist));
	break;
      case STATE_SIZE: {
	unsigned char *buf, *a, *b, *c;
	int w,h;
	buf = line_concat(&linelist);

	printf("size line: %s\n", buf);
	fflush(stdout);
	a = buf;
	b = Sstrchr(buf, 'x');
	c = b+1;

	if(NULL == a || NULL == b /* || NULL == c is meaningless */) {
	  printf("Got bad size values or something: '%s'\n", buf);
	}

	*b = 0x00;
	w = Satoi(a);
	h = Satoi(c);
	*b = 'x';

	puz_width_set(puz, w);
	puz_height_set(puz, h);

	break;
      }
      case STATE_GRID: {
	unsigned char *soln = line_concat(&linelist);
	unsigned char *grid = mkgrid(soln);
	puz_solution_set(puz, soln);
	puz_grid_set(puz, grid);
	break;
      }
      case STATE_CLUE0: do_clear = 0;
      case STATE_CLUE1: {
	int j, n_clues = line_count(&linelist);
	struct line_list *cur;
	puz_clear_clues(puz);
	puz_clue_count_set(puz, n_clues);
	
	for(j = 0, cur = &linelist;  j < n_clues && cur != NULL; j++, cur = cur->next) {
	  puz_clue_set(puz, j, cur->line);
	  printf("Clue %d -> %s\n", j, cur->line);
	}
	break;
      }
      default:
	printf("Reached Unknown state: %d\n", state);
	return NULL;
      }

      if(do_clear) {
	line_clear(&linelist);
      }
    }

    if(state_d) state++;
  }

  puz_cksums_calc(puz);
  puz_cksums_commit(puz);

  return puz;
}

/**
 * puz_load - Load a puzzle
 *
 * @puz: pointer to the struct puzzle_t to fill in.  If NULL, one will be allocated for you.
 * @type: type of the file to load as (PUZ_FILE_BINARY, PUZ_FILE_TEXT, or PUZ_FILE_UNKNOWN)
 * @base: pointer to the buffer containing the puz file to load (required)
 * @sz: size of the puz file in the buffer
 *
 * This is used to load a puzzle file from a loaded PUZ file.  Pass in
 * a pointer to a puzzle, a pointer to a buffer containing the puzzle,
 * and the size of the puzzle, and you'll get back a pointer to the
 * filled-in puzzle_t.
 * 
 * Returns NULL on error, the filled-in struct puzzle_t on success.
 */
struct puzzle_t *puz_load(struct puzzle_t *puz, int type, unsigned char *base, int sz) {
  int typeguess;

  if(base[0] != TEXT_SUBMAGIC || base[0xd] == 0x00) 
    typeguess = PUZ_FILE_BINARY;
  else
    typeguess = PUZ_FILE_TEXT;

  if(type != PUZ_FILE_UNKNOWN && type != typeguess){
    printf("Explicit file type requested, but given input appears to be the other format.\n");
    return NULL;
  }

  switch(typeguess) {
  case PUZ_FILE_BINARY:
    puz = puz_load_bin(puz, base, sz);
    break;
  case PUZ_FILE_TEXT:
    puz = puz_load_text(puz, base, sz);
    break;
  }
  
  return puz;
}
