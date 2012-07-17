/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <map>
#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
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
    if (reg->type_ == gdb_rsp::Abi::READ_ONLY) {
      // Do not change read-only registers.
      // TODO(eaeltsin): it is an error if new value is not equal to old value.
      // Suppress it for now as this is used in G packet that writes all
      // registers at once, and its failure might confuse GDB.
      // We can start actually reporting the error when we support P packet
      // that writes registers one at a time, as failure to write a single
      // register should be much less confusing.
    } else if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 &&
               NACL_BUILD_SUBARCH == 64 &&
               reg->type_ == gdb_rsp::Abi::X86_64_TRUSTED_PTR) {
      // Do not change high 32 bits.
      // GDB should work with untrusted addresses, thus high 32 bits of new
      // value should be 0.
      // TODO(eaeltsin): this is not fully implemented yet, and high 32 bits
      // of new value may also be equal to high 32 bits of old value.
      // Other cases are definitely bogus.
      CHECK(len == 8);
      memcpy((char *) &context_ + reg->offset_, src, 4);
    } else {
      memcpy((char *) &context_ + reg->offset_, src, len);
    }
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

  CHECK(map.count(id) == 0);

  Thread *thread = new Thread(id, natp);
  map[id] = thread;
  return thread;
}

IThread *IThread::Acquire(uint32_t id) {
  MutexLock lock(ThreadGetLock());
  ThreadMap_t &map = *ThreadGetMap();

  CHECK(map.count(id) != 0);

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

  CHECK(!map.empty());

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

  CHECK(!map.empty());

  NaClUntrustedThreadsResumeAllButOne(
      map.begin()->second->natp_->nap,
      signaled_tid == 0 ? NULL : map[signaled_tid]->natp_);
}

}  // namespace port
