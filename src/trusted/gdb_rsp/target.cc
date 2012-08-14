/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/gdb_rsp/abi.h"
#include "native_client/src/trusted/gdb_rsp/packet.h"
#include "native_client/src/trusted/gdb_rsp/target.h"
#include "native_client/src/trusted/gdb_rsp/session.h"
#include "native_client/src/trusted/gdb_rsp/util.h"

#include "native_client/src/trusted/port/platform.h"
#include "native_client/src/trusted/port/thread.h"

#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"

#if NACL_WINDOWS
#define snprintf sprintf_s
#endif

using std::string;

using port::IEvent;
using port::IMutex;
using port::IPlatform;
using port::IThread;
using port::MutexLock;

namespace gdb_rsp {


Target::Target(struct NaClApp *nap, const Abi* abi)
  : nap_(nap),
    abi_(abi),
    mutex_(NULL),
    session_(NULL),
    ctx_(NULL),
    cur_signal_(0),
    sig_thread_(0),
    reg_thread_(0),
    step_over_breakpoint_thread_(0),
    mem_base_(0) {
  if (NULL == abi_) abi_ = Abi::Get();
}

Target::~Target() {
  Destroy();
}

bool Target::Init() {
  string targ_xml = "l<target><architecture>";

  targ_xml += abi_->GetName();
  targ_xml += "</architecture><osabi>NaCl</osabi></target>";

  // Set a more specific result which won't change.
  properties_["target.xml"] = targ_xml;
  properties_["Supported"] =
    "PacketSize=7cf;qXfer:features:read+";

  mutex_ = IMutex::Allocate();
  ctx_ = new uint8_t[abi_->GetContextSize()];

  if ((NULL == mutex_) || (NULL == ctx_)) {
    Destroy();
    return false;
  }
  return true;
}

void Target::Destroy() {
  if (mutex_) IMutex::Free(mutex_);

  delete[] ctx_;
}

bool Target::AddTemporaryBreakpoint(uint64_t address) {
  const Abi::BPDef *bp = abi_->GetBreakpointDef();

  // If this ABI does not support breakpoints then fail
  if (NULL == bp) return false;

  // If we alreay have a breakpoint here then don't add it
  BreakMap_t::iterator itr = breakMap_.find(address);
  if (itr != breakMap_.end()) return false;

  uint8_t *data = new uint8_t[bp->size_];
  if (NULL == data) return false;

  // Copy the old code from here
  if (IPlatform::GetMemory(address, bp->size_, data) == false) {
    delete[] data;
    return false;
  }
  if (IPlatform::SetMemory(address, bp->size_, bp->code_) == false) {
    delete[] data;
    return false;
  }

  breakMap_[address] = data;
  return true;
}

bool Target::RemoveTemporaryBreakpoints(IThread *thread) {
  const Abi::BPDef *bp_def = abi_->GetBreakpointDef();
  const Abi::RegDef *ip_def = abi_->GetInstPtrDef();
  uint64_t new_ip = 0;

  // If this ABI does not support breakpoints then fail.
  if (!bp_def) {
    return false;
  }

  if (bp_def->after_) {
    // Instruction pointer needs adjustment.
    // WARNING! Little-endian only, as we are fetching a potentially 32-bit
    // value into uint64_t! Do we need to worry about big-endian?
    // TODO(eaeltsin): fix register access functions to avoid this problem!
    thread->GetRegister(ip_def->index_, &new_ip, ip_def->bytes_);
    new_ip -= bp_def->size_;
  }

  // Iterate through the map, removing breakpoints
  while (!breakMap_.empty()) {
    // Copy the key/value locally
    BreakMap_t::iterator cur = breakMap_.begin();
    uint64_t addr = cur->first;
    uint8_t *data = cur->second;

    // Then remove it from the map
    breakMap_.erase(cur);

    // Copy back the old code, and free the data
    if (!IPlatform::SetMemory(addr, bp_def->size_, data))
      NaClLog(LOG_ERROR, "Failed to undo breakpoint.\n");
    delete[] data;

    if (bp_def->after_) {
      // Adjust thread instruction pointer.
      // WARNING:
      // - addr contains trusted address
      if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 &&
          NACL_BUILD_SUBARCH == 64) {
        // - rip contains trusted address
        if (addr == new_ip) {
          thread->SetRegister(ip_def->index_, &new_ip, ip_def->bytes_);
        }
      } else if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 &&
                 NACL_BUILD_SUBARCH == 32) {
        // - eip contains untrusted address
        if (addr == new_ip + mem_base_) {
          thread->SetRegister(ip_def->index_, &new_ip, ip_def->bytes_);
        }
      } else {
        NaClLog(
            LOG_FATAL,
            "Target::RemoveTemporaryBreakpoints: Unknown CPU architecture\n");
      }
    }
  }

  return true;
}


void Target::Run(Session *ses) {
  bool first = true;
  mutex_->Lock();
  session_ = ses;
  mutex_->Unlock();
  do {
    // We poll periodically for faulted threads or input on the socket.
    // TODO(mseaborn): This is slow.  We should use proper thread
    // wakeups instead.
    // See http://code.google.com/p/nativeclient/issues/detail?id=2952

    // Give everyone else a chance to use the lock
    IPlatform::Relinquish(100);

    // Lock to prevent anyone else from modifying threads
    // or updating the signal information.
    MutexLock lock(mutex_);
    Packet recv, reply;

    uint32_t id = 0;

    if (step_over_breakpoint_thread_ != 0) {
      // We are waiting for a specific thread to fault while all other
      // threads are suspended.  Note that faulted_thread_count might
      // be >1, because multiple threads can fault simultaneously
      // before the debug stub gets a chance to suspend all threads.
      // This is why we must check the status of a specific thread --
      // we cannot call UnqueueAnyFaultedThread() and expect it to
      // return step_over_breakpoint_thread_.
      if (!IThread::HasThreadFaulted(step_over_breakpoint_thread_)) {
        // The thread has not faulted.  Nothing to do, so try again.
        // Note that we do not respond to input from GDB while in this
        // state.
        // TODO(mseaborn): We should allow GDB to interrupt execution.
        continue;
      }
      // All threads but one are already suspended.  We only need to
      // suspend the single thread that we allowed to run.
      IThread::SuspendSingleThread(step_over_breakpoint_thread_);
      IThread::UnqueueSpecificFaultedThread(step_over_breakpoint_thread_,
                                            &cur_signal_);
      id = step_over_breakpoint_thread_;
      step_over_breakpoint_thread_ = 0;
      sig_thread_ = id;
      reg_thread_ = id;
      // Reset single stepping.
      threads_[id]->SetStep(false);
    } else if (nap_->faulted_thread_count != 0) {
      // At least one untrusted thread has got an exception.  First we
      // need to ensure that all threads are suspended.  Then we can
      // retrieve a thread from the set of faulted threads.
      IThread::SuspendAllThreads();
      IThread::UnqueueAnyFaultedThread(&id, &cur_signal_);
      sig_thread_ = id;
      reg_thread_ = id;
      RemoveTemporaryBreakpoints(threads_[id]);
    } else {
      // Otherwise look for messages from GDB.
      if (!ses->DataAvailable()) {
        // No input from GDB.  Nothing to do, so try again.
        continue;
      }
      // GDB should have tried to interrupt the target.
      // See http://sourceware.org/gdb/current/onlinedocs/gdb/Interrupts.html
      // TODO(eaeltsin): should we verify the interrupt sequence?

      // Indicate we have no current thread.
      // TODO(eaeltsin): or pick any thread? Add a test.
      // See http://code.google.com/p/nativeclient/issues/detail?id=2743
      sig_thread_ = 0;
      IThread::SuspendAllThreads();
    }

    // Next update the current thread info
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "QC%x", id);
    properties_["C"] = tmp;

    if (first) {
      // First time on a connection, we don't sent the signal
      first = false;
    } else {
      // All other times, send the signal that triggered us
      Packet pktOut;
      SetStopReply(&pktOut);
      ses->SendPacketOnly(&pktOut);
    }

    // Now we are ready to process commands
    // Loop through packets until we process a continue
    // packet.
    do {
      if (ses->GetPacket(&recv)) {
        reply.Clear();
        if (ProcessPacket(&recv, &reply)) {
          // If this is a continue command, break out of this loop
          break;
        } else {
          // Othwerise send the reponse
          ses->SendPacket(&reply);
        }
      }
    } while (ses->Connected());

    // Reset the signal value
    cur_signal_ = 0;

    // TODO(eaeltsin): it might make sense to resume signaled thread before
    // others, though it is not required by GDB docs.
    if (step_over_breakpoint_thread_ == 0) {
      IThread::ResumeAllThreads();
    } else {
      // Resume one thread while leaving all others suspended.
      IThread::ResumeSingleThread(step_over_breakpoint_thread_);
    }

    // Continue running until the connection is lost.
  } while (ses->Connected());
  mutex_->Lock();
  session_ = NULL;
  mutex_->Unlock();
}


void Target::SetStopReply(Packet *pktOut) const {
  pktOut->AddRawChar('T');
  pktOut->AddWord8(cur_signal_);

  // gdbserver handles GDB interrupt by sending SIGINT to the debuggee, thus
  // GDB interrupt is also a case of a signalled thread.
  // At the moment we handle GDB interrupt differently, without using a signal,
  // so in this case sig_thread_ is 0.
  // This might seem weird to GDB, so at least avoid reporting tid 0.
  // TODO(eaeltsin): http://code.google.com/p/nativeclient/issues/detail?id=2743
  if (sig_thread_ != 0) {
    // Add 'thread:<tid>;' pair. Note terminating ';' is required.
    pktOut->AddString("thread:");
    pktOut->AddNumberSep(sig_thread_, ';');
  }
}


bool Target::GetFirstThreadId(uint32_t *id) {
  threadItr_ = threads_.begin();
  return GetNextThreadId(id);
}

bool Target::GetNextThreadId(uint32_t *id) {
  if (threadItr_ == threads_.end()) return false;

  *id = (*threadItr_).first;
  threadItr_++;

  return true;
}



bool Target::ProcessPacket(Packet* pktIn, Packet* pktOut) {
  char cmd;
  int32_t seq = -1;
  ErrDef  err = NONE;

  // Clear the outbound message
  pktOut->Clear();

  // Pull out the sequence.
  pktIn->GetSequence(&seq);
  if (seq != -1) pktOut->SetSequence(seq);

  // Find the command
  pktIn->GetRawChar(&cmd);

  switch (cmd) {
    // IN : $?
    // OUT: $Sxx
    case '?':
      SetStopReply(pktOut);
      break;

    case 'c':
      return true;

    // IN : $d
    // OUT: -NONE-
    case 'd':
      Detach();
      break;

    // IN : $g
    // OUT: $xx...xx
    case 'g': {
      IThread *thread = GetRegThread();
      if (NULL == thread) {
        err = BAD_ARGS;
        break;
      }

      // Copy OS preserved registers to GDB payload
      for (uint32_t a = 0; a < abi_->GetRegisterCount(); a++) {
        const Abi::RegDef *def = abi_->GetRegisterDef(a);
        thread->GetRegister(a, &ctx_[def->offset_], def->bytes_);
      }

      pktOut->AddBlock(ctx_, abi_->GetContextSize());
      break;
    }

    // IN : $Gxx..xx
    // OUT: $OK
    case 'G': {
      IThread *thread = GetRegThread();
      if (NULL == thread) {
        err = BAD_ARGS;
        break;
      }

      pktIn->GetBlock(ctx_, abi_->GetContextSize());

      // GDB payload to OS registers
      for (uint32_t a = 0; a < abi_->GetRegisterCount(); a++) {
        const Abi::RegDef *def = abi_->GetRegisterDef(a);
        thread->SetRegister(a, &ctx_[def->offset_], def->bytes_);
      }

      pktOut->AddString("OK");
      break;
    }

    // IN : $H(c/g)(-1,0,xxxx)
    // OUT: $OK
    case 'H': {
        char type;
        uint64_t id;

        if (!pktIn->GetRawChar(&type)) {
          err = BAD_FORMAT;
          break;
        }
        if (!pktIn->GetNumberSep(&id, 0)) {
          err = BAD_FORMAT;
          break;
        }

        if (threads_.begin() == threads_.end()) {
            err = BAD_ARGS;
            break;
        }

        // If we are using "any" get the first thread
        if (id == static_cast<uint64_t>(-1)) id = threads_.begin()->first;

        // Verify that we have the thread
        if (threads_.find(static_cast<uint32_t>(id)) == threads_.end()) {
          err = BAD_ARGS;
          break;
        }

        pktOut->AddString("OK");
        switch (type) {
          case 'g':
            reg_thread_ = static_cast<uint32_t>(id);
            break;

          case 'c':
            // 'c' is deprecated in favor of vCont.
          default:
            err = BAD_ARGS;
            break;
        }
        break;
      }

    // IN : $maaaa,llll
    // OUT: $xx..xx
    case 'm': {
        uint64_t addr;
        uint64_t wlen;
        uint32_t len;
        if (!pktIn->GetNumberSep(&addr, 0)) {
          err = BAD_FORMAT;
          break;
        }
        if (addr < mem_base_) {
          addr += mem_base_;
        }
        if (!pktIn->GetNumberSep(&wlen, 0)) {
          err = BAD_FORMAT;
          break;
        }

        len = static_cast<uint32_t>(wlen);
        uint8_t *block = new uint8_t[len];
        if (!port::IPlatform::GetMemory(addr, len, block)) err = FAILED;

        pktOut->AddBlock(block, len);
        break;
      }

    // IN : $Maaaa,llll:xx..xx
    // OUT: $OK
    case 'M':  {
        uint64_t addr;
        uint64_t wlen;
        uint32_t len;

        if (!pktIn->GetNumberSep(&addr, 0)) {
          err = BAD_FORMAT;
          break;
        }
        if (addr < mem_base_) {
          addr += mem_base_;
        }

        if (!pktIn->GetNumberSep(&wlen, 0)) {
          err = BAD_FORMAT;
          break;
        }

        len = static_cast<uint32_t>(wlen);
        uint8_t *block = new uint8_t[len];
        pktIn->GetBlock(block, len);

        if (!port::IPlatform::SetMemory(addr, len, block)) err = FAILED;

        pktOut->AddString("OK");
        break;
      }

    case 'q': {
      string tmp;
      const char *str = &pktIn->GetPayload()[1];
      stringvec toks = StringSplit(str, ":;");
      PropertyMap_t::const_iterator itr = properties_.find(toks[0]);

      // If this is a thread query
      if (!strcmp(str, "fThreadInfo") || !strcmp(str, "sThreadInfo")) {
        uint32_t curr;
        bool more = false;
        if (str[0] == 'f') {
          more = GetFirstThreadId(&curr);
        } else {
          more = GetNextThreadId(&curr);
        }

        if (!more) {
          pktOut->AddString("l");
        } else {
          pktOut->AddString("m");
          pktOut->AddNumberSep(curr, 0);
        }
        break;
      }

      // Check for architecture query
      tmp = "Xfer:features:read:target.xml";
      if (!strncmp(str, tmp.data(), tmp.length())) {
        stringvec args = StringSplit(&str[tmp.length()+1], ",");
        if (args.size() != 2) break;

        const char *out = properties_["target.xml"].data();
        int offs = strtol(args[0].data(), NULL, 16);
        int max  = strtol(args[1].data(), NULL, 16) + offs;
        int len  = static_cast<int>(strlen(out));

        if (max >= len) max = len;

        while (offs < max) {
          pktOut->AddRawChar(out[offs]);
          offs++;
        }
        break;
      }

      // Check the property cache
      if (itr != properties_.end()) {
        pktOut->AddString(itr->second.data());
      }
      break;
    }

    case 's': {
      IThread *thread = GetRunThread();
      if (thread) thread->SetStep(true);
      return true;
    }

    case 'T': {
      uint64_t id;
      if (!pktIn->GetNumberSep(&id, 0)) {
        err = BAD_FORMAT;
        break;
      }

      if (GetThread(static_cast<uint32_t>(id)) == NULL) {
        err = BAD_ARGS;
        break;
      }

      pktOut->AddString("OK");
      break;
    }

    case 'v': {
      const char *str = pktIn->GetPayload() + 1;

      if (strncmp(str, "Cont", 4) == 0) {
        // vCont
        const char *subcommand = str + 4;

        if (strcmp(subcommand, "?") == 0) {
          // Report supported vCont actions. These 4 are required.
          pktOut->AddString("vCont;s;S;c;C");
          break;
        }

        if (strcmp(subcommand, ";c") == 0) {
          // Continue all threads.
          return true;
        }

        if (strncmp(subcommand, ";s:", 3) == 0) {
          // Single step one thread and optionally continue all other threads.
          char *end;
          uint32_t thread_id = static_cast<uint32_t>(
              strtol(subcommand + 3, &end, 16));
          if (end == subcommand + 3) {
            err = BAD_ARGS;
            break;
          }

          ThreadMap_t::iterator it = threads_.find(thread_id);
          if (it == threads_.end()) {
            err = BAD_ARGS;
            break;
          }

          if (*end == 0) {
            // Single step one thread and keep other threads stopped.
            // GDB uses this to continue from a breakpoint, which works by:
            // - replacing trap instruction with the original instruction;
            // - single-stepping through the original instruction. Other threads
            //   must remain stopped, otherwise they might execute the code at
            //   the same address and thus miss the breakpoint;
            // - replacing the original instruction with trap instruction;
            // - continuing all threads;
            if (thread_id != sig_thread_) {
              err = BAD_ARGS;
              break;
            }
            step_over_breakpoint_thread_ = sig_thread_;
          } else if (strcmp(end, ";c") == 0) {
            // Single step one thread and continue all other threads.
          } else {
            // Unsupported combination of single step and other args.
            err = BAD_ARGS;
            break;
          }

          it->second->SetStep(true);
          return true;
        }

        // Continue one thread and keep other threads stopped.
        //
        // GDB sends this for software single step, which is used:
        // - on Win64 to step over rsp modification and subsequent rsp
        //   sandboxing at once. For details, see:
        //     http://code.google.com/p/nativeclient/issues/detail?id=2903
        // - TODO: on ARM, which has no hardware support for single step
        // - TODO: to step over syscalls
        //
        // Unfortunately, we can't make this just Win-specific. We might
        // use Linux GDB to connect to Win debug stub, so even Linux GDB
        // should send software single step. Vice versa, software single
        // step-enabled Win GDB might be connected to Linux debug stub,
        // so even Linux debug stub should accept software single step.
        if (strncmp(subcommand, ";c:", 3) == 0) {
          char *end;
          uint32_t thread_id = static_cast<uint32_t>(
              strtol(subcommand + 3, &end, 16));
          if (end != subcommand + 3 && *end == 0) {
            if (thread_id == sig_thread_) {
              step_over_breakpoint_thread_ = sig_thread_;
              return true;
            }
          }

          err = BAD_ARGS;
          break;
        }

        // Unsupported form of vCont.
        err = BAD_FORMAT;
        break;
      }

      NaClLog(LOG_ERROR, "Unknown command: %s\n", pktIn->GetPayload());
      return false;
    }

    default: {
      // If the command is not recognzied, ignore it by sending an
      // empty reply.
      string str;
      pktIn->GetString(&str);
      NaClLog(LOG_ERROR, "Unknown command: %s\n", pktIn->GetPayload());
      return false;
    }
  }

  // If there is an error, return the error code instead of a payload
  if (err) {
    pktOut->Clear();
    pktOut->AddRawChar('E');
    pktOut->AddWord8(err);
  }
  return false;
}


void Target::TrackThread(IThread* thread) {
  uint32_t id = thread->GetId();
  mutex_->Lock();
  threads_[id] = thread;
  mutex_->Unlock();
}

void Target::IgnoreThread(IThread* thread) {
  uint32_t id = thread->GetId();
  mutex_->Lock();
  ThreadMap_t::iterator itr = threads_.find(id);

  if (itr != threads_.end()) threads_.erase(itr);
  mutex_->Unlock();
}

void Target::Exit(int err_code) {
  mutex_->Lock();
  if (session_ != NULL) {
    Packet exit_packet;
    exit_packet.AddRawChar('W');
    exit_packet.AddWord8(err_code);
    session_->SendPacket(&exit_packet);
  }
  mutex_->Unlock();
}

void Target::Detach() {
  NaClLog(LOG_INFO, "Requested Detach.\n");
}


IThread* Target::GetRegThread() {
  ThreadMap_t::const_iterator itr;

  switch (reg_thread_) {
    // If we wany "any" then try the signal'd thread first
    case 0:
    case 0xFFFFFFFF:
      itr = threads_.begin();
      break;

    default:
      itr = threads_.find(reg_thread_);
      break;
  }

  if (itr == threads_.end()) return 0;

  return itr->second;
}

IThread* Target::GetRunThread() {
  // This is used to select a thread for "s" (step) command only.
  // For multi-threaded targets, "s" is deprecated in favor of "vCont", which
  // always specifies the thread explicitly when needed. However, we want
  // to keep backward compatibility here, as using "s" when debugging
  // a single-threaded program might be a popular use case.
  if (threads_.size() == 1) {
    return threads_.begin()->second;
  }
  return NULL;
}

IThread* Target::GetThread(uint32_t id) {
  ThreadMap_t::const_iterator itr;
  itr = threads_.find(id);
  if (itr != threads_.end()) return itr->second;

  return NULL;
}


}  // namespace gdb_rsp
