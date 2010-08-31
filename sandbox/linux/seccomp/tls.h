// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TLS_H__
#define TLS_H__

#include <asm/ldt.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/prctl.h>

namespace playground {

class TLS {
 private:
  class SysCalls {
   public:
    #define SYS_CPLUSPLUS
    #define SYS_ERRNO     my_errno
    #define SYS_INLINE    inline
    #define SYS_PREFIX    -1
    #undef  SYS_LINUX_SYSCALL_SUPPORT_H
    #include "linux_syscall_support.h"
    SysCalls() : my_errno(0) { }
    int my_errno;
  };

 public:
  static void *allocateTLS() {
    SysCalls sys;
    #if defined(__x86_64__)
    void *addr = sys.mmap(0, 4096, PROT_READ|PROT_WRITE,
                          MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (sys.arch_prctl(ARCH_SET_GS, addr) < 0) {
      return NULL;
    }
    #elif defined(__i386__)
    void *addr = sys.mmap2(0, 4096, PROT_READ|PROT_WRITE,
                           MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    struct user_desc u;
    u.entry_number    = (typeof u.entry_number)-1;
    u.base_addr       = (int)addr;
    u.limit           = 0xfffff;
    u.seg_32bit       = 1;
    u.contents        = 0;
    u.read_exec_only  = 0;
    u.limit_in_pages  = 1;
    u.seg_not_present = 0;
    u.useable         = 1;
    if (sys.set_thread_area(&u) < 0) {
      return NULL;
    }
    asm volatile(
        "movw %w0, %%fs"
        :
        : "q"(8*u.entry_number+3));
    #else
    #error Unsupported target platform
    #endif
    return addr;
  }

  static void freeTLS() {
    SysCalls sys;
    void *addr;
    #if defined(__x86_64__)
    sys.arch_prctl(ARCH_GET_GS, &addr);
    #elif defined(__i386__)
    struct user_desc u;
    sys.get_thread_area(&u);
    addr = (void *)u.base_addr;
    #else
    #error Unsupported target platform
    #endif
    sys.munmap(addr, 4096);
  }

  template<class T> static inline bool setTLSValue(int idx, T val) {
    #if defined(__x86_64__)
    if (idx < 0 || idx >= 4096/8) {
      return false;
    }
    asm volatile(
        "movq %0, %%gs:(%1)\n"
        :
        : "q"((void *)val), "q"(8ll * idx));
    #elif defined(__i386__)
    if (idx < 0 || idx >= 4096/8) {
      return false;
    }
    if (sizeof(T) == 8) {
      asm volatile(
          "movl %0, %%fs:(%1)\n"
          :
          : "r"((unsigned)val), "r"(8 * idx));
      asm volatile(
          "movl %0, %%fs:(%1)\n"
          :
          : "r"((unsigned)((unsigned long long)val >> 32)), "r"(8 * idx + 4));
    } else {
      asm volatile(
          "movl %0, %%fs:(%1)\n"
          :
          : "r"(val), "r"(8 * idx));
    }
    #else
    #error Unsupported target platform
    #endif
    return true;
  }

  template<class T> static inline T getTLSValue(int idx) {
    #if defined(__x86_64__)
    long long rc;
    if (idx < 0 || idx >= 4096/8) {
      return 0;
    }
    asm volatile(
        "movq %%gs:(%1), %0\n"
        : "=q"(rc)
        : "q"(8ll * idx));
    return (T)rc;
    #elif defined(__i386__)
    if (idx < 0 || idx >= 4096/8) {
      return 0;
    }
    if (sizeof(T) == 8) {
      unsigned lo, hi;
      asm volatile(
          "movl %%fs:(%1), %0\n"
          : "=r"(lo)
          : "r"(8 * idx));
      asm volatile(
          "movl %%fs:(%1), %0\n"
          : "=r"(hi)
          : "r"(8 * idx + 4));
      return (T)((unsigned long long)lo + ((unsigned long long)hi << 32));
    } else {
      long rc;
      asm volatile(
          "movl %%fs:(%1), %0\n"
          : "=r"(rc)
          : "r"(8 * idx));
      return (T)rc;
    }
    #else
    #error Unsupported target platform
    #endif
  }

};

} // namespace
#endif
