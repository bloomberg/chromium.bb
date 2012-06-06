/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <windows.h>
#include <exception>
#include <stdexcept>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/gdb_rsp/abi.h"
#include "native_client/src/trusted/port/mutex.h"
#include "native_client/src/trusted/port/thread.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"

/*
 * Define the OS specific portions of IThread interface.
 */

namespace {

const int kX86TrapFlag = 1 << 8;

const int kDBG_PRINTEXCEPTION_C = 0x40010006;

}  // namespace

namespace port {

static IThread::CatchFunc_t s_CatchFunc = NULL;
static void* s_CatchCookie = NULL;
static PVOID s_OldCatch = NULL;

enum PosixSignals {
  SIGINT  = 2,
  SIGQUIT = 3,
  SIGILL  = 4,
  SIGTRACE= 5,
  SIGBUS  = 7,
  SIGFPE  = 8,
  SIGKILL = 9,
  SIGSEGV = 11,
  SIGSTKFLT = 16,
};


static IMutex* ThreadGetLock() {
  static IMutex* mutex_ = IMutex::Allocate();
  return mutex_;
}

static IThread::ThreadMap_t *ThreadGetMap() {
  static IThread::ThreadMap_t* map_ = new IThread::ThreadMap_t;
  return map_;
}

static int8_t ExceptionToSignal(int ex) {
  switch (ex) {
    case EXCEPTION_GUARD_PAGE:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
      return SIGSEGV;

    case EXCEPTION_BREAKPOINT:
    case EXCEPTION_SINGLE_STEP:
      return SIGTRACE;

    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
      return SIGFPE;

    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
      return SIGILL;

    case EXCEPTION_STACK_OVERFLOW:
      return SIGSTKFLT;

    case CONTROL_C_EXIT:
      return SIGQUIT;

    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_INVALID_HANDLE:
      return SIGILL;
  }
  return SIGILL;
}


class Thread : public IThread {
 public:
  Thread(uint32_t id, struct NaClAppThread *natp)
      : ref_(1), id_(id), natp_(natp), state_(RUNNING) {}

  ~Thread() {}

  uint32_t GetId() {
    return id_;
  }

  State GetState() {
    return state_;
  }

  virtual bool Suspend() {
    MutexLock lock(ThreadGetLock());
    if (state_ != RUNNING) return false;

    // Attempt to suspend the thread
    DWORD count = SuspendThread(natp_->thread.tid);

    CONTEXT win_context;
    win_context.ContextFlags = CONTEXT_ALL;
    if (!GetThreadContext(natp_->thread.tid, &win_context)) {
      NaClLog(LOG_FATAL, "Thread::Suspend: GetThreadContext failed\n");
    }
    NaClSignalContextFromHandler(&context_, &win_context);

    if (count != -1) {
      state_ = SUSPENDED;
      return true;
    }

    return false;
  }

  virtual bool Resume() {
    MutexLock lock(ThreadGetLock());
    if (state_ != SUSPENDED) return false;

    CONTEXT win_context;
    win_context.ContextFlags = CONTEXT_ALL;
    NaClSignalContextToHandler(&win_context, &context_);
    if (!SetThreadContext(natp_->thread.tid, &win_context)) {
      NaClLog(LOG_FATAL, "Thread::Resume: SetThreadContext failed\n");
    }

    // Attempt to resume the thread
    if (ResumeThread(natp_->thread.tid) != -1) {
      state_ = RUNNING;
      return true;
    }

    return false;
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

  virtual bool SetRegister(uint32_t index, void *src, uint32_t len) {
    const gdb_rsp::Abi *abi = gdb_rsp::Abi::Get();
    const gdb_rsp::Abi::RegDef *reg = abi->GetRegisterDef(index);
    memcpy((char *) &context_ + reg->offset_, src, len);
    return false;
  }

  virtual void* GetContext() { return &context_; }

  static LONG NTAPI ExceptionCatch(PEXCEPTION_POINTERS ep) {
    uint32_t id = static_cast<uint32_t>(GetCurrentThreadId());
    Thread* thread = static_cast<Thread*>(Acquire(id));

    // This 2 lines is a fix for the bug:
    // 366: Linux GDB doesn't work for Chrome
    // http://code.google.com/p/nativeclient/issues/detail?id=366
    // When debug stub thread opens socket to listen (for RSP debugger),
    // it triggers some component to send DBG_PRINTEXCEPTION(with string
    // "swi_lsp: non-browser app; disable"), then VEH handler goes into wait
    // for debugger to resolve exception.
    // But debugger is not connected, and debug thread is not listening on
    // connection! It get stuck.
    // Ignoring this exception - for now - helps debug stub start on chrome.
    // Now it can listen on RSP connection and can get debugger connected etc.
    if (kDBG_PRINTEXCEPTION_C == ep->ExceptionRecord->ExceptionCode) {
      return EXCEPTION_CONTINUE_EXECUTION;
    }

    // If we are not tracking this thread, then ignore it
    if (NULL == thread) return EXCEPTION_CONTINUE_SEARCH;

    State old_state = thread->state_;
    thread->state_ = SIGNALED;
    int8_t sig = ExceptionToSignal(ep->ExceptionRecord->ExceptionCode);

    // Handle EXCEPTION_BREAKPOINT SEH/VEH handler special case:
    // Here instruction pointer from the CONTEXT structure points to the int3
    // instruction, not after the int3 instruction.
    // This is different from the hardware context, and (thus) different from
    // the context obtained via GetThreadContext on Windows and from signal
    // handler context on Linux.
    // See http://code.google.com/p/nativeclient/issues/detail?id=1730.
    // We adjust instruction pointer to match the hardware.
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT) {
#if NACL_BUILD_SUBARCH == 64
      ep->ContextRecord->Rip += 1;
#else
      ep->ContextRecord->Eip += 1;
#endif
    }

    struct NaClSignalContext *context =
        (struct NaClSignalContext *)thread->GetContext();
    NaClSignalContextFromHandler(context, ep->ContextRecord);
    if (NULL != s_CatchFunc) s_CatchFunc(id, sig, s_CatchCookie);
    NaClSignalContextToHandler(ep->ContextRecord, context);

    thread->state_ = old_state;
    Release(thread);
    return EXCEPTION_CONTINUE_EXECUTION;
  }


 private:
  uint32_t ref_;
  uint32_t id_;
  struct NaClAppThread *natp_;
  State  state_;
  struct NaClSignalContext context_;

  friend class IThread;
};

IThread* IThread::Create(uint32_t id, struct NaClAppThread* natp) {
  MutexLock lock(ThreadGetLock());
  Thread* thread;
  ThreadMap_t &map = *ThreadGetMap();

  if (map.count(id)) {
    NaClLog(LOG_FATAL, "IThread::Create: thread 0x%x already exists\n", id);
  }

  thread = new Thread(id, natp);
  map[id] = thread;
  return thread;
}

IThread* IThread::Acquire(uint32_t id) {
  MutexLock lock(ThreadGetLock());
  Thread* thread;
  ThreadMap_t &map = *ThreadGetMap();

  if (map.count(id) == 0) {
    NaClLog(LOG_FATAL, "IThread::Acquire: thread 0x%x does not exist\n", id);
  }

  thread = static_cast<Thread*>(map[id]);
  thread->ref_++;
  return thread;
}

void IThread::Release(IThread *ithread) {
  MutexLock lock(ThreadGetLock());
  Thread* thread = static_cast<Thread*>(ithread);
  thread->ref_--;

  if (thread->ref_ == 0) {
    ThreadGetMap()->erase(thread->id_);
    delete static_cast<IThread*>(thread);
  }
}

void IThread::SetExceptionCatch(IThread::CatchFunc_t func, void *cookie) {
  MutexLock lock(ThreadGetLock());

  // Remove our old catch if there is one, this allows us to add again
  if (NULL != s_OldCatch) RemoveVectoredExceptionHandler(s_OldCatch);

  // Add the new one, at the front of the list
  s_OldCatch = AddVectoredExceptionHandler(1, Thread::ExceptionCatch);
  s_CatchFunc = func;
  s_CatchCookie = cookie;
}


}  // End of port namespace

