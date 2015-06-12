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
#include "native_client/src/trusted/service_runtime/thread_suspension.h"

namespace {

const int kX86TrapFlag = 1 << 8;

}  // namespace

namespace port {

Thread::Thread(uint32_t id, struct NaClAppThread *natp)
    : id_(id), natp_(natp), fault_signal_(0) {}

Thread::~Thread() {}

uint32_t Thread::GetId() {
  return id_;
}

bool Thread::SetStep(bool on) {
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

bool Thread::GetRegisters(uint8_t *dst) {
  const gdb_rsp::Abi *abi = gdb_rsp::Abi::Get();
  for (uint32_t a = 0; a < abi->GetRegisterCount(); a++) {
    const gdb_rsp::Abi::RegDef *reg = abi->GetRegisterDef(a);
    if (reg->type_ == gdb_rsp::Abi::READ_ONLY_ZERO) {
      memset(dst + reg->offset_, 0, reg->bytes_);
    } else {
      memcpy(dst + reg->offset_,
             (char *) &context_ + reg->offset_,
             reg->bytes_);
    }
  }
  return false;
}

bool Thread::SetRegisters(uint8_t *src) {
  const gdb_rsp::Abi *abi = gdb_rsp::Abi::Get();
  for (uint32_t a = 0; a < abi->GetRegisterCount(); a++) {
    const gdb_rsp::Abi::RegDef *reg = abi->GetRegisterDef(a);
    if (reg->type_ == gdb_rsp::Abi::READ_ONLY ||
        reg->type_ == gdb_rsp::Abi::READ_ONLY_ZERO) {
      // Do not change read-only registers.
      // TODO(eaeltsin): it is an error if new value isn't equal to old value.
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
      CHECK(reg->bytes_ == 8);
      memcpy((char *) &context_ + reg->offset_,
             src + reg->offset_, 4);
    } else {
      memcpy((char *) &context_ + reg->offset_,
             src + reg->offset_, reg->bytes_);
    }
  }
  return false;
}

void Thread::CopyRegistersFromAppThread() {
  NaClAppThreadGetSuspendedRegisters(natp_, &context_);
}

void Thread::CopyRegistersToAppThread() {
  NaClAppThreadSetSuspendedRegisters(natp_, &context_);
}

void Thread::SuspendThread() {
  NaClUntrustedThreadSuspend(natp_, /* save_registers= */ 1);
  CopyRegistersFromAppThread();
}

void Thread::ResumeThread() {
  CopyRegistersToAppThread();
  NaClUntrustedThreadResume(natp_);
}

// HasThreadFaulted() returns whether the given thread has been
// blocked as a result of faulting.  The thread does not need to be
// currently suspended.
bool Thread::HasThreadFaulted() {
  return natp_->fault_signal != 0;
}

// UnqueueFaultedThread() takes a thread that has been blocked as a
// result of faulting and unblocks it.  As a precondition, the
// thread must be currently suspended.
void Thread::UnqueueFaultedThread() {
  int exception_code;
  CHECK(NaClAppThreadUnblockIfFaulted(natp_, &exception_code));
  fault_signal_ = 0;
}

int Thread::GetFaultSignal() {
  return fault_signal_;
}

void Thread::SetFaultSignal(int signal) {
  fault_signal_ = signal;
}

struct NaClSignalContext *Thread::GetContext() { return &context_; }
struct NaClAppThread *Thread::GetAppThread() { return natp_; }

}  // namespace port
