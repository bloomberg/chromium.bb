// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <asm/unistd.h>
#include "sandbox_impl.h"
#include "syscall_table.h"

#if defined(__x86_64__)
#ifndef __NR_set_robust_list
#define __NR_set_robust_list 273
#endif
#ifndef __NR_accept4
#define __NR_accept4         288
#endif
#elif defined(__i386__)
#ifndef __NR_set_robust_list
#define __NR_set_robust_list 311
#endif
#else
#error Unsupported target platform
#endif

// TODO(markus): This is an incredibly dirty hack to make the syscallTable
//               live in r/o memory.
//               Unfortunately, gcc doesn't give us a clean option to do
//               this. Ultimately, we should probably write some code that
//               parses /usr/include/asm/unistd*.h and generates a *.S file.
//               But we then need to figure out how to integrate this code
//               with our build system.

const struct SyscallTable syscallTable[] __attribute__((
    section(".rodata, \"a\", @progbits\n#"))) ={

  #if defined(__NR_accept)
  [ __NR_accept          ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_accept4         ] = { UNRESTRICTED_SYSCALL,     0                   },
  #endif
  [ __NR_access          ] = { (void*)&sandbox_access,   process_access      },
  [ __NR_brk             ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_clock_gettime   ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_clone           ] = { (void*)&sandbox_clone,    process_clone       },
  [ __NR_close           ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_dup             ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_dup2            ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_epoll_create    ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_epoll_ctl       ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_epoll_wait      ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_exit            ] = { (void*)&sandbox_exit,     process_exit        },
  [ __NR_exit_group      ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_fcntl           ] = { UNRESTRICTED_SYSCALL,     0                   },
  #if defined(__NR_fcntl64)
  [ __NR_fcntl64         ] = { UNRESTRICTED_SYSCALL,     0                   },
  #endif
  [ __NR_fstat           ] = { UNRESTRICTED_SYSCALL,     0                   },
  #if defined(__NR_fstat64)
  [ __NR_fstat64         ] = { UNRESTRICTED_SYSCALL,     0                   },
  #endif
  [ __NR_futex           ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_getdents        ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_getdents64      ] = { UNRESTRICTED_SYSCALL,     0                   },
  #if defined(__NR_getpeername)
  [ __NR_getpeername     ] = { UNRESTRICTED_SYSCALL,     0                   },
  #endif
  [ __NR_getpid          ] = { (void*)&sandbox_getpid,   0                   },
  #if defined(__NR_getsockname)
  [ __NR_getsockname     ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_getsockopt      ] = { (void*)&sandbox_getsockopt,process_getsockopt },
  #endif
  [ __NR_gettid          ] = { (void*)&sandbox_gettid,   0                   },
  [ __NR_gettimeofday    ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_ioctl           ] = { (void*)&sandbox_ioctl,    process_ioctl       },
  #if defined(__NR_ipc)
  [ __NR_ipc             ] = { (void*)&sandbox_ipc,      process_ipc         },
  #endif
  #if defined(__NR__llseek)
  [ __NR__llseek         ] = { UNRESTRICTED_SYSCALL,     0                   },
  #endif
  [ __NR_lseek           ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_lstat           ] = { (void*)&sandbox_lstat,    process_stat        },
  #if defined(__NR_lstat64)
  [ __NR_lstat64         ] = { (void*)&sandbox_lstat64,  process_stat        },
  #endif
  [ __NR_madvise         ] = { (void*)&sandbox_madvise,  process_madvise     },
  #if defined(__NR_mmap2)
  [ __NR_mmap2           ] =
  #else
  [ __NR_mmap            ] =
  #endif
                             { (void*)&sandbox_mmap,     process_mmap        },
  [ __NR_mprotect        ] = { (void*)&sandbox_mprotect, process_mprotect    },
  [ __NR_munmap          ] = { (void*)&sandbox_munmap,   process_munmap      },
  [ __NR_open            ] = { (void*)&sandbox_open,     process_open        },
  [ __NR_pipe            ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_poll            ] = { UNRESTRICTED_SYSCALL,     0                   },
  #if defined(__NR_recvfrom)
  [ __NR_recvfrom        ] = { (void*)&sandbox_recvfrom, process_recvfrom    },
  [ __NR_recvmsg         ] = { (void*)&sandbox_recvmsg,  process_recvmsg     },
  [ __NR_sendmsg         ] = { (void*)&sandbox_sendmsg,  process_sendmsg     },
  [ __NR_sendto          ] = { (void*)&sandbox_sendto,   process_sendto      },
  #endif
  [ __NR_set_robust_list ] = { UNRESTRICTED_SYSCALL,     0                   },
  #if defined(__NR_setsockopt)
  [ __NR_setsockopt      ] = { (void*)&sandbox_setsockopt,process_setsockopt },
  #if defined(__NR_shmat)
  [ __NR_shmat           ] = { (void*)&sandbox_shmat,    process_shmat       },
  [ __NR_shmctl          ] = { (void*)&sandbox_shmctl,   process_shmctl      },
  [ __NR_shmdt           ] = { (void*)&sandbox_shmdt,    process_shmdt       },
  [ __NR_shmget          ] = { (void*)&sandbox_shmget,   process_shmget      },
  #endif
  [ __NR_shutdown        ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_socketpair      ] = { UNRESTRICTED_SYSCALL,     0                   },
  #endif
  #if defined(__NR_socketcall)
  [ __NR_socketcall      ] = { (void*)&sandbox_socketcall,process_socketcall },
  #endif
  [ __NR_stat            ] = { (void*)&sandbox_stat,     process_stat        },
  #if defined(__NR_stat64)
  [ __NR_stat64          ] = { (void*)&sandbox_stat64,   process_stat        },
  #endif
  [ __NR_time            ] = { UNRESTRICTED_SYSCALL,     0                   },
  [ __NR_uname           ] = { UNRESTRICTED_SYSCALL,     0                   },
};
const unsigned maxSyscall __attribute__((section(".rodata"))) =
    sizeof(syscallTable)/sizeof(struct SyscallTable);

const int syscall_mutex_[4096/sizeof(int)] asm("playground$syscall_mutex")
    __attribute__((section(".rodata"),aligned(4096)
#if defined(__x86_64__)
                   ,visibility("internal")
#endif
                   )) = { 0x80000000 };
