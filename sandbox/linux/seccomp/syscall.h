#ifndef SYSCALL_H__
#define SYSCALL_H__

#ifdef __cplusplus
extern "C" {
#endif

void syscallWrapper() asm("playground$syscallWrapper");

#ifdef __cplusplus
}
#endif

#endif // SYSCALL_H__
