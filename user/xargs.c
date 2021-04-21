#include "../kernel/types.h"
#include "../user/user.h"

#define BUFSIZE 512

int
main(int argc, char *argv[])
{
  if(argc <= 1){
    fprintf(2, "usage: xargs command [arg1 arg2 ...]\n");
    exit(1);
  }

  for (int i=0; i<argc-1; i++)
      argv[i] = argv[i+1];
  /* argv[argc-1] = 0; */

  char buf[512];
  char *p = buf;

  for (;;) {

      int n = read(0, p, 1);
      if (n <= 0)
          exit(0);
      if (*p == '\n') {
          *p = '\0'; // cut endling newline
          argv[argc-1] = buf;

          if (fork() == 0) {
              exec(argv[0], argv);
              fprintf(2, "error running %s\n", argv[0]);
          } else {
              int st;
              wait(&st);
              if (st != 0)
                  fprintf(2, "exited with %d code\n", st);
          }
          p = buf;
      } else {
          p++;
      }
  }
  exit(0);
}

