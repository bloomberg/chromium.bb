/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <map>
#include <string.h>

#include "native_client/src/include/build_config.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/debug_stub/abi.h"
#include "native_client/src/trusted/debug_stub/mutex.h"
#include "native_client/src/trusted/debug_stub/thread.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"

namespace {

const int kX86TrapFlag = 1 << 8;

}  // namespace

namespace port {

// TODO(mseaborn): Merge Thread and IThread into a single class,
// because there is only one implementation of IThread and the methods
// do not need to be virtual.

class Thread : public IThread {
 public:
  Thread(uint32_t id, struct NaClAppThread *natp)
      : id_(id), natp_(natp), fault_signal_(0) {}
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
    if (reg->type_ == gdb_rsp::Abi::READ_ONLY_ZERO) {
      memset(dst, 0, len);
    } else {
      memcpy(dst, (char *) &context_ + reg->offset_, len);
    }
    return false;
  }

  virtual bool SetRegister(uint32_t index, void* src, uint32_t len) {
    const gdb_rsp::Abi *abi = gdb_rsp::Abi::Get();
    const gdb_rsp::Abi::RegDef *reg = abi->GetRegisterDef(index);
    if (reg->type_ == gdb_rsp::Abi::READ_ONLY ||
        reg->type_ == gdb_rsp::Abi::READ_ONLY_ZERO) {
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

  virtual void CopyRegistersFromAppThread() {
    NaClAppThreadGetSuspendedRegisters(natp_, &context_);
  }

  virtual void CopyRegistersToAppThread() {
    NaClAppThreadSetSuspendedRegisters(natp_, &context_);
  }

  virtual void SuspendThread() {
    NaClUntrustedThreadSuspend(natp_, /* save_registers= */ 1);
    CopyRegistersFromAppThread();
  }

  virtual void ResumeThread() {
    CopyRegistersToAppThread();
    NaClUntrustedThreadResume(natp_);
  }

  // HasThreadFaulted() returns whether the given thread has been
  // blocked as a result of faulting.  The thread does not need to be
  // currently suspended.
  virtual bool HasThreadFaulted() {
    return natp_->fault_signal != 0;
  }

  // UnqueueFaultedThread() takes a thread that has been blocked as a
  // result of faulting and unblocks it.  As a precondition, the
  // thread must be currently suspended.
  virtual void UnqueueFaultedThread() {
    int exception_code;
    CHECK(NaClAppThreadUnblockIfFaulted(natp_, &exception_code));
    fault_signal_ = 0;
  }

  virtual int GetFaultSignal() {
    return fault_signal_;
  }

  virtual void SetFaultSignal(int signal) {
    fault_signal_ = signal;
  }

  virtual struct NaClSignalContext *GetContext() { return &context_; }
  virtual struct NaClAppThread *GetAppThread() { return natp_; }

 private:
  uint32_t id_;
  struct NaClAppThread *natp_;
  struct NaClSignalContext context_;
  int fault_signal_;
};

IThread *IThread::Create(uint32_t id, struct NaClAppThread *natp) {
  return new Thread(id, natp);
}

}  // namespace port
