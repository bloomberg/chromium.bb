/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// This module defines the interface for interacting with platform specific
// threads  .  This API provides a mechanism to query for a thread, by using
// the acquire method with the ID of a pre-existing thread.   The register
// accessors are expected to return false if the thread is not in a state
// where the registers can be accessed, such RUNNING or SYSCALL. This API
// will throw:
//   std::exception - if a unexpected OS Error is encountered.
//   std::out_of_range - if the register index is out of range.

#ifndef NATIVE_CLIENT_PORT_THREAD_H_
#define NATIVE_CLIENT_PORT_THREAD_H_ 1

#include "native_client/src/trusted/port/std_types.h"

struct NaClAppThread;
struct NaClSignalContext;

namespace port {

class IThread {
 public:
  typedef void (*CatchFunc_t)(uint32_t id, int8_t sig, void *cookie);

  virtual uint32_t GetId() = 0;

  virtual bool SetStep(bool on) = 0;

  virtual bool GetRegister(uint32_t index, void *dst, uint32_t len) = 0;
  virtual bool SetRegister(uint32_t index, void *src, uint32_t len) = 0;

  virtual struct NaClSignalContext *GetContext() = 0;

  static IThread *Create(uint32_t id, struct NaClAppThread *natp);
  static IThread *Acquire(uint32_t id);
  static void Release(IThread *thread);
  static void SuspendAllThreadsExceptSignaled(uint32_t signaled_tid);
  static void ResumeAllThreadsExceptSignaled(uint32_t signaled_tid);
  static void SetExceptionCatch(CatchFunc_t func, void *cookie);

 protected:
  virtual ~IThread() {}  // Prevent delete of base pointer
};

}  // namespace port

#endif  // PORT_THREAD_H_
