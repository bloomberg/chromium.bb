// Copyright (c) 2011, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// crash_generator.cc: Implement google_breakpad::CrashGenerator.
// See crash_generator.h for details.

#include "common/linux/tests/crash_generator.h"

#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

#include "common/linux/eintr_wrapper.h"
#include "common/tests/auto_tempdir.h"
#include "common/tests/file_utils.h"

namespace {

struct ThreadData {
  pthread_t thread;
  pthread_barrier_t* barrier;
  pid_t* thread_id_ptr;
};

// Core file size limit set to 1 MB, which is big enough for test purposes.
const rlim_t kCoreSizeLimit = 1024 * 1024;

void *thread_function(void *data) {
  ThreadData* thread_data = reinterpret_cast<ThreadData*>(data);
  volatile pid_t thread_id = syscall(__NR_gettid);
  *(thread_data->thread_id_ptr) = thread_id;
  int result = pthread_barrier_wait(thread_data->barrier);
  if (result != 0 && result != PTHREAD_BARRIER_SERIAL_THREAD) {
    exit(1);
  }
  while (true) {
    pthread_yield();
  }
}

}  // namespace

namespace google_breakpad {

CrashGenerator::CrashGenerator()
    : shared_memory_(NULL),
      shared_memory_size_(0) {
}

CrashGenerator::~CrashGenerator() {
  UnmapSharedMemory();
}

bool CrashGenerator::HasDefaultCorePattern() const {
  char buffer[8];
  ssize_t buffer_size = sizeof(buffer);
  return ReadFile("/proc/sys/kernel/core_pattern", buffer, &buffer_size) &&
         buffer_size == 5 && memcmp(buffer, "core", 4) == 0;
}

std::string CrashGenerator::GetCoreFilePath() const {
  return temp_dir_.path() + "/core";
}

pid_t CrashGenerator::GetThreadId(unsigned index) const {
  return reinterpret_cast<pid_t*>(shared_memory_)[index];
}

pid_t* CrashGenerator::GetThreadIdPointer(unsigned index) {
  return reinterpret_cast<pid_t*>(shared_memory_) + index;
}

bool CrashGenerator::MapSharedMemory(size_t memory_size) {
  if (!UnmapSharedMemory())
    return false;

  void* mapped_memory = mmap(0, memory_size, PROT_READ | PROT_WRITE,
                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (mapped_memory == MAP_FAILED)
    return false;

  memset(mapped_memory, 0, memory_size);
  shared_memory_ = mapped_memory;
  shared_memory_size_ = memory_size;
  return true;
}

bool CrashGenerator::UnmapSharedMemory() {
  if (!shared_memory_)
    return true;

  if (munmap(shared_memory_, shared_memory_size_) == 0) {
    shared_memory_ = NULL;
    shared_memory_size_ = 0;
    return true;
  }
  return false;
}

bool CrashGenerator::SetCoreFileSizeLimit(rlim_t limit) const {
  struct rlimit limits = { limit, limit };
  return setrlimit(RLIMIT_CORE, &limits) == 0;
}

bool CrashGenerator::CreateChildCrash(
    unsigned num_threads, unsigned crash_thread, int crash_signal) {
  if (num_threads == 0 || crash_thread >= num_threads)
    return false;

  if (!MapSharedMemory(num_threads * sizeof(pid_t)))
    return false;

  pid_t pid = fork();
  if (pid == 0) {
    if (chdir(temp_dir_.path().c_str()) == 0 &&
        SetCoreFileSizeLimit(kCoreSizeLimit)) {
      CreateThreadsInChildProcess(num_threads);
      kill(*GetThreadIdPointer(crash_thread), crash_signal);
    }
    exit(1);
  }

  int status;
  if (HANDLE_EINTR(waitpid(pid, &status, 0)) == -1 ||
      !WIFSIGNALED(status) || WTERMSIG(status) != crash_signal)
    return false;

  return true;
}

void CrashGenerator::CreateThreadsInChildProcess(unsigned num_threads) {
  *GetThreadIdPointer(0) = getpid();

  if (num_threads <= 1)
    return;

  // This method does not clean up any pthread resource, as the process
  // is expected to be killed anyway.
  ThreadData* thread_data = new ThreadData[num_threads];

  // Create detached threads so that we do not worry about pthread_join()
  // later being called or not.
  pthread_attr_t thread_attributes;
  if (pthread_attr_init(&thread_attributes) != 0 ||
      pthread_attr_setdetachstate(&thread_attributes,
                                  PTHREAD_CREATE_DETACHED) != 0) {
    exit(1);
  }

  pthread_barrier_t thread_barrier;
  if (pthread_barrier_init(&thread_barrier, NULL, num_threads) != 0) {
    exit(1);
  }

  for (unsigned i = 1; i < num_threads; ++i) {
    thread_data[i].barrier = &thread_barrier;
    thread_data[i].thread_id_ptr = GetThreadIdPointer(i);
    if (pthread_create(&thread_data[i].thread, &thread_attributes,
                       thread_function, &thread_data[i]) != 0) {
      exit(1);
    }
  }

  int result = pthread_barrier_wait(&thread_barrier);
  if (result != 0 && result != PTHREAD_BARRIER_SERIAL_THREAD) {
    exit(1);
  }

  pthread_barrier_destroy(&thread_barrier);
  pthread_attr_destroy(&thread_attributes);
  delete[] thread_data;
}

}  // namespace google_breakpad
