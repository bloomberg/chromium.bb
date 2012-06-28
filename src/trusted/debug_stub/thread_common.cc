/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <map>
#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/gdb_rsp/abi.h"
#include "native_client/src/trusted/port/mutex.h"
#include "native_client/src/trusted/port/thread.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"

namespace {

const int kX86TrapFlag = 1 << 8;

}  // namespace

namespace port {

class Thread : public IThread {
 public:
  Thread(uint32_t id, struct NaClAppThread *natp)
      : ref_(1), id_(id), natp_(natp) {}
  ~Thread() {}

  uint32_t GetId() {
    return id_;
  }

  virtual bool SetStep(bool on) {
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
    if (on) {
      context_.flags |= kX86TrapFlag;
    } else {
      context_.flags &= ~kX86TrapFlag;
    }
    return true;
#else
    // TODO(mseaborn): Implement for ARM.
    UNREFERENCED_PARAMETER(on);
    return false;
#endif
  }

  virtual bool GetRegister(uint32_t index, void *dst, uint32_t len) {
    const gdb_rsp::Abi *abi = gdb_rsp::Abi::Get();
    const gdb_rsp::Abi::RegDef *reg = abi->GetRegisterDef(index);
    memcpy(dst, (char *) &context_ + reg->offset_, len);
    return false;
  }

  virtual bool SetRegister(uint32_t index, void* src, uint32_t len) {
    const gdb_rsp::Abi *abi = gdb_rsp::Abi::Get();
    const gdb_rsp::Abi::RegDef *reg = abi->GetRegisterDef(index);
    memcpy((char *) &context_ + reg->offset_, src, len);
    return false;
  }

  virtual struct NaClSignalContext *GetContext() { return &context_; }

 private:
  uint32_t ref_;
  uint32_t id_;
  struct NaClAppThread *natp_;
  struct NaClSignalContext context_;

  friend class IThread;
};


typedef std::map<uint32_t, Thread*> ThreadMap_t;

static ThreadMap_t *ThreadGetMap() {
  static ThreadMap_t *map_ = new ThreadMap_t;
  return map_;
}

static IMutex *ThreadGetLock() {
  static IMutex *mutex_ = IMutex::Allocate();
  return mutex_;
}

IThread *IThread::Create(uint32_t id, struct NaClAppThread *natp) {
  MutexLock lock(ThreadGetLock());
  ThreadMap_t &map = *ThreadGetMap();

  if (map.count(id)) {
    NaClLog(LOG_FATAL, "IThread::Create: thread 0x%x already exists\n", id);
  }

  Thread *thread = new Thread(id, natp);
  map[id] = thread;
  return thread;
}

IThread *IThread::Acquire(uint32_t id) {
  MutexLock lock(ThreadGetLock());
  ThreadMap_t &map = *ThreadGetMap();

  if (map.count(id) == 0) {
    NaClLog(LOG_FATAL, "IThread::Acquire: thread 0x%x does not exist\n", id);
  }

  Thread *thread = map[id];
  thread->ref_++;
  return thread;
}

void IThread::Release(IThread *ithread) {
  MutexLock lock(ThreadGetLock());
  Thread *thread = static_cast<Thread*>(ithread);
  thread->ref_--;

  if (thread->ref_ == 0) {
    ThreadGetMap()->erase(thread->id_);
    delete static_cast<IThread*>(thread);
  }
}

void IThread::SuspendAllThreadsExceptSignaled(uint32_t signaled_tid) {
  MutexLock lock(ThreadGetLock());
  ThreadMap_t &map = *ThreadGetMap();

  if (map.empty()) {
    NaClLog(LOG_FATAL,
            "IThread::SuspendAllThreadsExceptSignaled: no threads\n");
  }

  NaClUntrustedThreadsSuspendAllButOne(
      map.begin()->second->natp_->nap,
      signaled_tid == 0 ? NULL : map[signaled_tid]->natp_,
      /* save_registers= */ 1);

  for (ThreadMap_t::iterator it = map.begin(); it != map.end(); ++it) {
    Thread *thread = it->second;
    if (thread->id_ != signaled_tid) {
      if ((thread->natp_->suspend_state & NACL_APP_THREAD_UNTRUSTED) != 0) {
        thread->context_ = *thread->natp_->suspended_registers;
      } else {
        // TODO(eaeltsin): fetch context from NaClAppThread.user.
        memset(&thread->context_, 0, sizeof(thread->context_));
        NaClLog(LOG_WARNING,
                "IThread::SuspendAllThreadsExceptSignaled: thread 0x%x "
                "registers not fetched\n",
                thread->id_);
      }
    }
  }
}

void IThread::ResumeAllThreadsExceptSignaled(uint32_t signaled_tid) {
  MutexLock lock(ThreadGetLock());
  ThreadMap_t &map = *ThreadGetMap();

  if (map.empty()) {
    NaClLog(LOG_FATAL,
            "IThread::ResumeAllThreadsExceptSignaled: no threads\n");
  }

  NaClUntrustedThreadsResumeAllButOne(
      map.begin()->second->natp_->nap,
      signaled_tid == 0 ? NULL : map[signaled_tid]->natp_);
}

}  // namespace port
