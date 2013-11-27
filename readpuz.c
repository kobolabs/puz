#include <puz.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <errno.h>


#ifdef _POSIX_MAPPED_FILES
void  *mmap(void  *start,  size_t length, int prot , int flags, int fd, off_t offset);
int munmap(void *start, size_t length);
#endif
/* ************************************************************************
   Main
   **** */
int main(int argc, char *argv[]) {
  int fd;

  void *base;

  struct stat buf;
  int i, sz;

  struct puzzle_t p;

  char *destfile = NULL;

  if(argc < 2) {
    printf("Usage: %s <file.puz>\n", argv[0]);
    return 0;
  }

  if(argc == 3) {
    destfile = argv[2];
    printf("Will regurgitate into %s as binary after reading\n", argv[2]);
  }

  i = stat(argv[1], &buf);
  if(i != 0) {
    perror("stat:");
    return -1;
  }

  sz = buf.st_size;

  if(!(fd = open(argv[1], O_RDONLY))) {
    perror("open:");
    return -1;
  }

  if(!(base = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0))) {
    perror("mmap");
    return -1;
  }

  if(NULL == puz_load(&p, PUZ_FILE_UNKNOWN, base, sz)) {
    printf("There was an error loading the puzzle file.  See above for details\n");
    return -1;
  }

  puz_cksums_calc(&p);

  i = puz_cksums_check(&p);

  if(i != 00) {
    printf("*** Error: %d errors in checksums.\n", i);
	return -1;
  }

	const char* separator = "myuniquelibpuzseparator";
	printf("%s", separator);
	printf("%s", puz_title_get(&p));
	printf("%s", separator);
	printf("%s", puz_author_get(&p));
	printf("%s", separator);
	printf("%s", puz_notes_get(&p));
	printf("%s", separator);
	printf("%d", puz_width_get(&p));
	printf("%s", separator);
	printf("%d", puz_height_get(&p));
	printf("%s", separator);
	printf("%s", puz_grid_get(&p));
	printf("%s", separator);
	printf("%s", puz_solution_get(&p));
	int numClues = puz_clue_count_get(&p);
	int clueNum = 0;
	for (; clueNum < numClues; clueNum++) {
		printf("%s", separator);
		printf("%s", puz_clue_get(&p, clueNum));
	}
  return 0;
}
