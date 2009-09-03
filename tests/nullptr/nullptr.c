#include <stdio.h>

int main(void) {
  unsigned char *byteptr = 0;
  unsigned char byte = *byteptr;
  printf("FAIL: address zero readable, contains %02x\n", byte);
  return 1;
}
