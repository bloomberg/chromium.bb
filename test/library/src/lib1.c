#include <stdio.h>

#ifdef _WIN32
__declspec(dllexport)
#endif
void library_function(void)
{
  printf("Hello from lib1.c\n");
}
