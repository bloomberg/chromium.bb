// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <map>

#include "debug.h"
#include "sandbox_impl.h"
#include "syscall_table.h"

namespace playground {

struct Thread {
  int              fdPub, fd;
  SecureMem::Args* mem;
};

SecureMem::Args* Sandbox::getSecureMem() {
  if (!secureMemPool_.empty()) {
    SecureMem::Args* rc = secureMemPool_.back();
    secureMemPool_.pop_back();
    memset(rc->scratchPage, 0, sizeof(rc->scratchPage));
    return rc;
  }
  return NULL;
}

void Sandbox::trustedProcess(int parentMapsFd, int processFdPub, int sandboxFd,
                             int cloneFd, SecureMem::Args* secureArena) {
  // The trusted process doesn't have access to TLS. Zero out the segment
  // registers so that we can later test that we are in the trusted process.
  #if defined(__x86_64__)
  asm volatile("mov %0, %%gs\n" : : "r"(0));
  #elif defined(__i386__)
  asm volatile("mov %0, %%fs\n" : : "r"(0));
  #else
  #error Unsupported target platform
  #endif

  std::map<long long, struct Thread> threads;
  SysCalls  sys;
  long long cookie               = 0;

  // The very first entry in the secure memory arena has been assigned to the
  // initial thread. The remaining entries are available for allocation.
  SecureMem::Args* startAddress  = secureArena;
  SecureMem::Args* nextThread    = startAddress;
  for (int i = 0; i < kMaxThreads-1; i++) {
    secureMemPool_.push_back(++startAddress);
  }

newThreadCreated:
  // Receive information from newly created thread
  Thread *newThread              = &threads[++cookie];
  memset(newThread, 0, sizeof(Thread));
  struct {
    SecureMem::Args* self;
    int              tid;
    int              fdPub;
  } __attribute__((packed)) data;

  size_t dataLen                 = sizeof(data);
  if (!getFd(cloneFd, &newThread->fdPub, &newThread->fd, &data, &dataLen) ||
      dataLen != sizeof(data)) {
    // We get here either because the sandbox got corrupted, or because our
    // parent process has terminated.
    if (newThread->fdPub || dataLen) {
      die("Failed to receive new thread information");
    }
    die();
  }
  if (data.self != nextThread) {
    // The only potentially security critical information received from the
    // newly created thread is "self". The "tid" is for informational purposes
    // (and for use in the new thread's TLS), and "fdPub" is uncritical as all
    // file descriptors are considered untrusted.
    // Thus, we only use "self" for a sanity check, but don't actually trust
    // it beyond that.
    die("Received corrupted thread information");
  }
  newThread->mem                 = nextThread;

  // Set up TLS area and let thread know that the data is now ready
  nextThread->cookie             = cookie;
  nextThread->threadId           = data.tid;
  nextThread->threadFdPub        = data.fdPub;
  write(sys, newThread->fd, "", 1);

  // Dispatch system calls that have been forwarded from the trusted thread(s).
  for (;;) {
    struct {
      unsigned int sysnum;
      long long    cookie;
    } __attribute__((packed)) header;

    int rc;
    if ((rc = read(sys, sandboxFd, &header, sizeof(header))) !=sizeof(header)){
      if (rc) {
        die("Failed to read system call number and thread id");
      }
      die();
    }
    std::map<long long, struct Thread>::iterator iter =
                                                   threads.find(header.cookie);
    if (iter == threads.end()) {
      die("Received request from unknown thread");
    }
    struct Thread* currentThread = &iter->second;
    if (header.sysnum > maxSyscall ||
        !syscallTable[header.sysnum].trustedProcess) {
      die("Trusted process encountered unexpected system call");
    }

    // Dispatch system call to handler function. Treat both exit() and clone()
    // specially.
    if (syscallTable[header.sysnum].trustedProcess(parentMapsFd,
                                                   sandboxFd,
                                                   currentThread->fdPub,
                                                   currentThread->fd,
                                                   currentThread->mem) &&
        header.sysnum == __NR_clone) {
      nextThread = currentThread->mem->newSecureMem;
      goto newThreadCreated;
    } else if (header.sysnum == __NR_exit) {
      NOINTR_SYS(sys.close(iter->second.fdPub));
      NOINTR_SYS(sys.close(iter->second.fd));
      SecureMem::Args* secureMem = currentThread->mem;
      threads.erase(iter);
      secureMemPool_.push_back(secureMem);
    }
  }
}

int Sandbox::initializeProtectedMap(int fd) {
  int mapsFd;
  if (!getFd(fd, &mapsFd, NULL, NULL, NULL)) {
 maps_failure:
    die("Cannot access /proc/self/maps");
  }

  // Read the memory mappings as they were before the sandbox takes effect.
  // These mappings cannot be changed by the sandboxed process.
  char line[80];
  FILE *fp = fdopen(mapsFd, "r");
  for (bool truncated = false;;) {
    if (fgets(line, sizeof(line), fp) == NULL) {
      if (feof(fp) || errno != EINTR) {
        break;
      }
      continue;
    }
    if (!truncated) {
      unsigned long start, stop;
      char *ptr = line;
      errno = 0;
      start = strtoul(ptr, &ptr, 16);
      if (errno || *ptr++ != '-') {
     parse_failure:
        die("Failed to parse /proc/self/maps");
      }
      stop = strtoul(ptr, &ptr, 16);
      if (errno || *ptr++ != ' ') {
        goto parse_failure;
      }
      protectedMap_[reinterpret_cast<void *>(start)] = stop - start;
    }
    truncated = strchr(line, '\n') == NULL;
  }

  // Prevent low address memory allocations. Some buggy kernels allow those
  if (protectedMap_[0] < (64 << 10)) {
    protectedMap_[0] = 64 << 10;
  }

  // Let the sandbox know that we are done parsing the memory map.
  SysCalls sys;
  if (write(sys, fd, &mapsFd, sizeof(mapsFd)) != sizeof(mapsFd)) {
    goto maps_failure;
  }

  return mapsFd;
}

SecureMem::Args* Sandbox::createTrustedProcess(int processFdPub, int sandboxFd,
                                               int cloneFdPub, int cloneFd) {
  // Allocate memory that will be used by an arena for storing the secure
  // memory. While we allow this memory area to be empty at times (e.g. when
  // not all threads are in use), we make sure that it never gets overwritten
  // by user-allocated memory. This happens in initializeProtectedMap() and
  // snapshotMemoryMappings().
  SecureMem::Args* secureArena = reinterpret_cast<SecureMem::Args*>(
      mmap(NULL, 8192*kMaxThreads, PROT_READ|PROT_WRITE,
           MAP_SHARED|MAP_ANONYMOUS, -1, 0));
  if (secureArena == MAP_FAILED) {
    die("Failed to allocate secure memory arena");
  }

  // Set up the mutex to be accessible from the trusted process and from
  // children of the trusted thread(s)
  if (mmap(&syscall_mutex_, 4096, PROT_READ|PROT_WRITE,
           MAP_SHARED|MAP_ANONYMOUS|MAP_FIXED, -1, 0) != &syscall_mutex_) {
    die("Failed to initialize secure mutex");
  }
  syscall_mutex_ = 0x80000000;


  // Create a trusted process that can evaluate system call parameters and
  // decide whether a system call should execute. This process runs outside of
  // the seccomp sandbox. It communicates with the sandbox'd process through
  // a socketpair() and through securely shared memory.
  pid_t pid                    = fork();
  if (pid < 0) {
    die("Failed to create trusted process");
  }
  if (!pid) {
    // Close all file handles except for sandboxFd, cloneFd, and stdio
    DIR *dir                   = opendir("/proc/self/fd");
    if (dir == 0) {
      // If we don't know the list of our open file handles, just try closing
      // all valid ones.
      for (int fd = sysconf(_SC_OPEN_MAX); --fd > 2; ) {
        if (fd != sandboxFd && fd != cloneFd) {
          close(fd);
        }
      }
    } else {
      // If available, if is much more efficient to just close the file
      // handles that show up in /proc/self/fd/
      struct dirent de, *res;
      while (!readdir_r(dir, &de, &res) && res) {
        if (res->d_name[0] < '0')
          continue;
        int fd                 = atoi(res->d_name);
        if (fd > 2 &&
            fd != sandboxFd && fd != cloneFd && fd != dirfd(dir)) {
          close(fd);
        }
      }
      closedir(dir);
    }

    // Initialize secure memory used for threads
    for (int i = 0; i < kMaxThreads; i++) {
      SecureMem::Args* args    = secureArena + i;
      args->self               = args;
      #ifndef NDEBUG
      args->allowAllSystemCalls= Debug::isEnabled();
      #endif
    }

    int parentMapsFd           = initializeProtectedMap(sandboxFd);
    trustedProcess(parentMapsFd, processFdPub, sandboxFd,
                   cloneFd, secureArena);
    die();
  }

  // We are still in the untrusted code. Deny access to restricted resources.
  mprotect(secureArena, 8192*kMaxThreads, PROT_NONE);
  mprotect(&syscall_mutex_, 4096, PROT_NONE);
  close(sandboxFd);

  return secureArena;
}

} // namespace
