#include <stdio.h>

extern int SyscallReturnIsSandboxed(void);

int main(void) {
  if (SyscallReturnIsSandboxed()) {
    printf("PASSED\n");
    return 0;
  } else {
    printf("FAILED\n");
    return 1;
  }
}
