#ifndef SYSCALL_H__
#define SYSCALL_H__

#ifdef __cplusplus
extern "C" {
#endif

void syscallWrapper() asm("playground$syscallWrapper")
#if defined(__x86_64__)
                      __attribute__((visibility("internal")))
#endif
;

#ifdef __cplusplus
}
#endif

#endif // SYSCALL_H__
