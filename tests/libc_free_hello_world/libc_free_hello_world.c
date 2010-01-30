#include <sys/types.h>
#include <bits/nacl_syscalls.h>

int main(void) {
  typedef int (*syswrite_t)(int, char const *, size_t);
  syswrite_t syswrite = (syswrite_t) (0x10000 + NACL_sys_write * 0x20);

  (void) (*syswrite)(1, "Hello world\n", 12);
  return 0;
}
