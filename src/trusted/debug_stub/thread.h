/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


// This module defines the interface for interacting with platform specific
// threads.  This API provides a mechanism to query for a thread, by using
// the acquire method with the ID of a pre-existing thread.

#ifndef NATIVE_CLIENT_PORT_THREAD_H_
#define NATIVE_CLIENT_PORT_THREAD_H_ 1

#include "native_client/src/include/portability.h"

struct NaClAppThread;
struct NaClSignalContext;

namespace port {

class IThread {
 public:
  virtual ~IThread() {}

  virtual uint32_t GetId() = 0;

  virtual bool SetStep(bool on) = 0;

  virtual bool GetRegister(uint32_t index, void *dst, uint32_t len) = 0;
  virtual bool SetRegister(uint32_t index, void *src, uint32_t len) = 0;

  virtual void CopyRegistersFromAppThread() = 0;
  virtual void CopyRegistersToAppThread() = 0;

  virtual void SuspendThread() = 0;
  virtual void ResumeThread() = 0;
  virtual bool HasThreadFaulted() = 0;
  virtual void UnqueueFaultedThread() = 0;

  virtual struct NaClSignalContext *GetContext() = 0;
  virtual struct NaClAppThread *GetAppThread() = 0;

  virtual int GetFaultSignal() = 0;
  virtual void SetFaultSignal(int signal) = 0;

  static IThread *Create(uint32_t id, struct NaClAppThread *natp);
  static int ExceptionToSignal(int exception_code);
};

}  // namespace port

#endif  // PORT_THREAD_H_
