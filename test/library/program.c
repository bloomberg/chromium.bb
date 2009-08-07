#include <stdio.h>

extern void library_function(void);

int main(int argc, char *argv[])
{
  printf("Hello from program.c\n");
  library_function();
  return 0;
}
