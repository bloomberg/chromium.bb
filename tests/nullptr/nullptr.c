#include <stdio.h>

int main(void)
{
  char  *byteptr;
  char  byte;

  byteptr = (char *) NULL;
  byte = *byteptr;
  printf("FAIL: address zero readable, contains %02x\n", byte & 0xff);
  return 1;
}
