#include <stdio.h>

/*
 * NOTE: this is a global to keep a smart compiler (llvm)
 *       from partially evaluating it
 */

unsigned char *byteptr = 0;

int main(void) {
  unsigned char byte = *byteptr;
  printf("FAIL: address zero readable, contains %02x\n", byte);
  return 1;
}
