// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SECURE_MEM_H__
#define SECURE_MEM_H__

#include <stdlib.h>

namespace playground {

class SecureMem {
 public:
  // Each thread is associated with two memory pages (i.e. 8192 bytes). This
  // memory is fully accessible by the trusted process, but in the trusted
  // thread and the sandboxed thread, the first page is only mapped PROT_READ,
  // and the second one is PROT_READ|PROT_WRITE.
  //
  // The first page can be modified by the trusted process and this is the
  // main mechanism how it communicates with the trusted thread. After each
  // update, it updates the "sequence" number. The trusted process must
  // check the "sequence" number has the expected value, and only then can
  // it trust the data in this page.
  typedef struct Args {
    union {
      struct {
        union {
          struct {
            struct Args* self;
            long         sequence;
            long         syscallNum;
            void*        arg1;
            void*        arg2;
            void*        arg3;
            void*        arg4;
            void*        arg5;
            void*        arg6;

            // Used by clone() to allow return from the syscall wrapper.
            void*        ret;
            #if defined(__x86_64__)
            void*        rbp;
            void*        rbx;
            void*        rcx;
            void*        rdx;
            void*        rsi;
            void*        rdi;
            void*        r8;
            void*        r9;
            void*        r10;
            void*        r11;
            void*        r12;
            void*        r13;
            void*        r14;
            void*        r15;
            #elif defined(__i386__)
            void*        ebp;
            void*        edi;
            void*        esi;
            void*        edx;
            void*        ecx;
            void*        ebx;
            #else
            #error Unsupported target platform
            #endif

            // Used by clone() to set up data for the new thread.
            struct Args* newSecureMem;
            int          processFdPub;
            int          cloneFdPub;

            // Set to non-zero, if in debugging mode
            int          allowAllSystemCalls;

            // The most recent SysV SHM identifier returned by
            // shmget(IPC_PRIVATE)
            int          shmId;

            // The following entries make up the sandboxed thread's TLS
            long long    cookie;
            long long    threadId;
            long long    threadFdPub;
          } __attribute__((packed));
          char           header[512];
        };
        // Used for calls such as open() and stat().
        char             pathname[4096 - 512];
      } __attribute__((packed));
      char               securePage[4096];
    };
    union {
      struct {
        // This scratch space is used by the trusted thread to read parameters
        // for unrestricted system calls.
        long             tmpSyscallNum;
        void*            tmpArg1;
        void*            tmpArg2;
        void*            tmpArg3;
        void*            tmpArg4;
        void*            tmpArg5;
        void*            tmpArg6;
        void*            tmpReturnValue;

        // We often have long sequences of calls to gettimeofday(). This is
        // needlessly expensive. Coalesce them into a single call.
        long             lastSyscallNum;
        int              gettimeofdayCounter;
      } __attribute__((packed));
      char               scratchPage[4096];
    };
  } __attribute__((packed)) Args;

  // Allows the trusted process to check whether the parent process still
  // exists. If it doesn't, kill the trusted process.
  static void dieIfParentDied(int parentProc);

  // The trusted process received a system call that it intends to deny.
  static void abandonSystemCall(int fd, int err);

  // Acquires the syscall_mutex_ prior to making changes to the parameters in
  // the secure memory page. Used by calls such as exit(), clone(), open(),
  // socketcall(), and stat().
  // After locking the mutex, it is no longer valid to abandon the system
  // call!
  static void lockSystemCall(int parentProc, Args* mem);

  // Sends a system call to the trusted thread. If "locked" is true, the
  // caller must first call lockSystemCall() and must also provide
  // "parentProc". In locked mode, sendSystemCall() won't return until the
  // trusted thread has completed processing.
  // Use sparingly as it serializes the operation of the trusted process.
  static void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                             int syscallNum) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum);
  }
  template<class T1> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1);
  }
  template<class T1, class T2> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1, T2 arg2) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1, (void*)arg2);
  }
  template<class T1, class T2, class T3> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1, T2 arg2, T3 arg3) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1, (void*)arg2, (void*)arg3);
  }
  template<class T1, class T2, class T3, class T4> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1, T2 arg2, T3 arg3, T4 arg4) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1, (void*)arg2, (void*)arg3, (void*)arg4);
  }
  template<class T1, class T2, class T3, class T4, class T5> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1, T2 arg2, T3 arg3, T4 arg4,
                      T5 arg5) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1, (void*)arg2, (void*)arg3, (void*)arg4,
                           (void*)arg5);
  }
  template<class T1, class T2, class T3, class T4, class T5, class T6> static
  void sendSystemCall(int fd, bool locked, int parentProc, Args* mem,
                      int syscallNum, T1 arg1, T2 arg2, T3 arg3, T4 arg4,
                      T5 arg5, T6 arg6) {
    sendSystemCallInternal(fd, locked, parentProc, mem, syscallNum,
                           (void*)arg1, (void*)arg2, (void*)arg3, (void*)arg4,
                           (void*)arg5, (void*)arg6);
  }

 private:
  static void sendSystemCallInternal(int fd, bool locked, int parentProc,
                                     Args* mem, int syscallNum, void* arg1 = 0,
                                     void* arg2 = 0, void* arg3 = 0,
                                     void* arg4 = 0, void* arg5 = 0,
                                     void* arg6 = 0);
};

} // namespace

#endif // SECURE_MEM_H__
