#include "StdAfx.h"
#include <algorithm>

#include "debug_core.h"
#include "debug_system.h"

//TODO: add error handling & reporting
//TODO: add logging of all system calls, debug::API calls, state changes...
//TODO: => add class DebugAPI w/logging
//TODO: use proper API for WoW processes
//TODO: develop 'debugger' events + observer
//TODO: break into multiple files
//TODO: add unit tests
//TODO: add comments
//TODO: write a design doc with FSM, sequence diagrams, class diagram, obj diagram...
//TODO: make sure debug::core has enough api to implement RSP debug_server

#pragma warning(disable : 4996)

namespace {
const int kVS2008_THREAD_INFO = 0x406D1388;
const unsigned char kBreakpointCode = 0xCC;
const size_t kMaxStringSize = 32 * 1024;
const char* kNexeUuid = "{7AA7C9CF-89EC-4ed3-8DAD-6DC84302AB11}";
}  // namespace

namespace debug {
bool Breakpoint::WriteBreakpointCode(DebuggeeProcess& process) {
  char code = kBreakpointCode;
  bool res = process.WriteMemory(address(), &code, sizeof(code));
  process.inside_observer().OnWriteBreakpointCode(process.id(), address(), res);
  return res;
}
 
bool Breakpoint::RecoverCodeAtBreakpoint(DebuggeeProcess& process) {
  bool res = process.WriteMemory(address(), &original_code_byte_, sizeof(original_code_byte_));
  process.inside_observer().OnRecoverCodeAtBreakpoint(process.id(), address(), original_code_byte_, res);
  return res;
}

bool Breakpoint::Init(DebuggeeProcess& process) {
  if (!process.ReadMemory(address(), sizeof(original_code_byte_), &original_code_byte_))
    return false;
  return WriteBreakpointCode(process);
}

void StandardContinuePolicy::MakeContinueDecision(DEBUG_EVENT& de, DecisionToContinue* dtc) {
  if (EXCEPTION_DEBUG_EVENT == de.dwDebugEventCode) {
    if (kVS2008_THREAD_INFO == de.u.Exception.ExceptionRecord.ExceptionCode) {
      dtc->Combine(DecisionToContinue(DecisionToContinue::WEAK_DECISION, false, false));  //TODO: change bool -> enum
    } else {
      DebuggeeProcess* proc = debug_core_.GetProcess(de.dwProcessId);
      DebuggeeThread* thread = NULL;
      if (NULL != proc)
        thread = proc->GetThread(de.dwThreadId);

      bool is_nexe = false;
      if (NULL != thread)
        is_nexe = thread->is_nexe_;

      bool pass_to_debuggee = (EXCEPTION_BREAKPOINT != de.u.Exception.ExceptionRecord.ExceptionCode);
      if (is_nexe) {
        dtc->Combine(DecisionToContinue(DecisionToContinue::WEAK_DECISION, true, pass_to_debuggee));
      } else {
        dtc->Combine(DecisionToContinue(DecisionToContinue::WEAK_DECISION, false, pass_to_debuggee));
      }
    }
  }
}

DecisionToContinue::DecisionToContinue()
    : decision_strength_(NO_DECISION),
      halt_debuggee_(false),
      pass_exception_to_debuggee_(true) {
}

DecisionToContinue::DecisionToContinue(DecisionStrength strength,
                                       bool halt_debuggee, 
                                       bool pass_exception_to_debuggee)
    : decision_strength_(strength),
      halt_debuggee_(halt_debuggee),
      pass_exception_to_debuggee_(pass_exception_to_debuggee) {
}

bool DecisionToContinue::operator==(const DecisionToContinue& other) const {
  return (other.decision_strength_ == decision_strength_) &&
         (other.halt_debuggee_ == halt_debuggee_) &&
         (other.pass_exception_to_debuggee_ == pass_exception_to_debuggee_);
}

bool DecisionToContinue::Combine(const DecisionToContinue& other) {
  if (other == *this)
    return true;

  if (other.decision_strength_ == decision_strength_)
    return false;

  if ((STRONG_DECISION == other.decision_strength_) || (NO_DECISION == decision_strength_))
    *this = other;
    
  return true;
}

DecisionToContinue::DecisionStrength DecisionToContinue::decision_strength() const {
  return decision_strength_;
}

bool DecisionToContinue::halt_debuggee() const {
  return halt_debuggee_;
}

bool DecisionToContinue::pass_exception_to_debuggee() const {
  return pass_exception_to_debuggee_;
}

bool DecisionToContinue::IsHaltDecision() const {
  return (NO_DECISION != decision_strength_) && halt_debuggee_;
}

DebuggeeThread::DebuggeeThread(int id, HANDLE handle, DebuggeeProcess& parent_process)
    : id_(id),
      handle_(handle),
      parent_process_(parent_process),
      state_(INIT),
      last_user_command_(NONE),
      is_nexe_(false),
      nexe_mem_base_(0) {
  EnterRunning();
}

DebuggeeThread::~DebuggeeThread() {
}

CoreInsideObserver& DebuggeeThread::inside_observer() {
  return process().inside_observer();
}

void DebuggeeThread::EmitNexeThreadStarted() {
  //TODO: implement
  inside_observer().OnNexeThreadStarted(this);
}

void DebuggeeThread::EmitRunning() {
  //TODO: implement
}

void DebuggeeThread::EmitHalted() {
  //TODO: implement
}

void DebuggeeThread::EmitDead() {
  //TODO: implement
}

void* DebuggeeThread::IpToFlatAddress(long long ip) const {
#ifndef _WIN64
  if (is_nexe_)   
    ip += reinterpret_cast<size_t>(nexe_mem_base_);
#endif
  return reinterpret_cast<void*>(ip);
}

void* DebuggeeThread::IpToFlatAddress(void* ip) const {
  return IpToFlatAddress(reinterpret_cast<long long>(ip));
}

void DebuggeeThread::OnOutputDebugString(DEBUG_EVENT& de, DecisionToContinue* dtc) {
  if (0 == de.u.DebugString.fUnicode) {
    size_t str_sz = min(kMaxStringSize, de.u.DebugString.nDebugStringLength + 1);
    char* tmp = static_cast<char*>(malloc(str_sz));
    if (NULL != tmp) {
      if (parent_process_.ReadMemory(de.u.DebugString.lpDebugStringData, str_sz, tmp)) {
        tmp[str_sz - 1] = 0;
        inside_observer().OnThreadOutputDebugString(this, tmp);

        if (strncmp(tmp, kNexeUuid, strlen(kNexeUuid)) == 0) {
          is_nexe_ = true;
          void* tread_inf_addr = 0;
          sscanf(tmp + strlen(kNexeUuid), "%p", &tread_inf_addr);  //TODO: cahnge to use command line parsing, direct passing of mem_base & entry_pt
          NaclInfo nacl_info;
          nacl_info.Init(&parent_process_, tread_inf_addr);
          nexe_mem_base_ = nacl_info.GetMemBase();
          nexe_entry_point_ = nacl_info.GetEntryPoint();
          EmitNexeThreadStarted();
          dtc->Combine(DecisionToContinue(DecisionToContinue::STRONG_DECISION, true));
          if (dtc->IsHaltDecision())
            EnterHalted();
          free(tmp);
          return;
        }
      }
      free(tmp);
    }
  }
  if (dtc->IsHaltDecision())
    EnterHalted();
}

void DebuggeeThread::OnExitThread(DEBUG_EVENT& de, DecisionToContinue* dtc) {
  if (dtc->IsHaltDecision())
    EnterZombie();
  else
    EnterDead();
}

void DebuggeeThread::EnterRunning() {
  inside_observer().OnThreadStateChange(this, state_, RUNNING);
  state_ = RUNNING;
  EmitRunning();
}

void DebuggeeThread::EnterHalted() {
  inside_observer().OnThreadStateChange(this, state_, HALTED);
  state_ = HALTED;
  EnableSingleStep(false);
  EmitHalted();
}

void DebuggeeThread::EnterZombie() {
  inside_observer().OnThreadStateChange(this, state_, ZOMBIE);
  state_ = ZOMBIE;
  EmitHalted();
}

void DebuggeeThread::EnterDead() {
  inside_observer().OnThreadStateChange(this, state_, DEAD);
  state_ = DEAD;
  EmitDead();
}

void DebuggeeThread::OnBreakpoint(DEBUG_EVENT& de, DecisionToContinue* dtc) {
  void* br_addr = IpToFlatAddress(de.u.Exception.ExceptionRecord.ExceptionAddress);

  Breakpoint* br = parent_process_.GetBreakpoint(br_addr);
  inside_observer().OnBreakpointHit(this, br_addr, (NULL != br));

  if (NULL != br) {
    // it's our breakpoint.
    current_breakpoint_ = *br;
    current_breakpoint_.RecoverCodeAtBreakpoint(process());
    SetIP(GetIP() - 1);
    *dtc = DecisionToContinue(DecisionToContinue::STRONG_DECISION, true);
  } else if (DebuggeeProcess::BREAK == process().last_user_command()) {
    inside_observer().OnBreakinBreakpoint(this);
     *dtc = DecisionToContinue(DecisionToContinue::STRONG_DECISION, true, false);
  }
  if (dtc->IsHaltDecision())
    EnterHalted();
}

void DebuggeeThread::ContinueAndPassExceptionToDebuggee() {
  if ((HALTED == state_) || (ZOMBIE == state_)) {
    last_user_command_ = CONTINUE;
    ContinueFromHalted(false, true);
  }
}

void DebuggeeThread::Continue() {
  if ((HALTED == state_) || (ZOMBIE == state_)) {
    last_user_command_ = CONTINUE;
    ContinueFromHalted(false, false);
  }
}

void DebuggeeThread::SingleStep() {
  if ((HALTED == state_) || (ZOMBIE == state_)) {
    last_user_command_ = STEP;
    ContinueFromHalted(true, false);
  }
}

void DebuggeeThread::ContinueFromHalted(bool single_step, bool pass_exception_to_debuggee) {
  if (current_breakpoint_.IsValid()) {
    // Check that breakpoint is not deleted
    if (NULL == process().GetBreakpoint(current_breakpoint_.address())) {
      current_breakpoint_.Invalidate();
    } else {
      void* flat_ip = IpToFlatAddress(GetIP());
      if (flat_ip == current_breakpoint_.address()) {
        EnableSingleStep(true);
      } else {
        current_breakpoint_.WriteBreakpointCode(process());
        current_breakpoint_.Invalidate();
      }
    }
  }
  if (single_step)
    EnableSingleStep(true);
  
  // pass_exception_to_debuggee_ is set by last decision to halt
  int flag = (pass_exception_to_debuggee ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE);
  ::ContinueDebugEvent(parent_process_.id(), id(), flag);

  if (ZOMBIE == state_)
    EnterDead();
  else
    EnterRunning();
}

void DebuggeeThread::OnSingleStep(DEBUG_EVENT& de, DecisionToContinue* dtc) {
  if (STEP == last_user_command_) {
    *dtc = DecisionToContinue(DecisionToContinue::STRONG_DECISION, true);
  } else {  // It was 'continue'.
    inside_observer().OnSingleStepDueToContinueFromBreakpoint(this, current_breakpoint_.address());
    *dtc = DecisionToContinue(DecisionToContinue::STRONG_DECISION, false, false);
  }

  if (current_breakpoint_.IsValid()) {
    current_breakpoint_.WriteBreakpointCode(process());  // recover breakpoint.
    current_breakpoint_.Invalidate();
  }

  if (dtc->IsHaltDecision())
    EnterHalted();
}

void DebuggeeThread::OnDebugEvent(DEBUG_EVENT& de, DecisionToContinue* dtc) {
  switch (de.dwDebugEventCode) {
    case OUTPUT_DEBUG_STRING_EVENT: OnOutputDebugString(de, dtc); break;
    case EXIT_THREAD_DEBUG_EVENT: OnExitThread(de, dtc); break;
    case EXCEPTION_DEBUG_EVENT: {
      switch (de.u.Exception.ExceptionRecord.ExceptionCode) {
        case EXCEPTION_BREAKPOINT: OnBreakpoint(de, dtc); break;
        case EXCEPTION_SINGLE_STEP: OnSingleStep(de, dtc); break;
      }
    }
  }
  if (dtc->IsHaltDecision() && (RUNNING == state_))
    EnterHalted();
}

void DebuggeeThread::EnableSingleStep(bool enable) {
  inside_observer().OnThreadSetSingleStep(this, enable);
  CONTEXT context;
  GetContext(&context, NULL);
  if (enable)
    context.EFlags |= 1 << 8;
  else
    context.EFlags &= ~(1 << 8);
  SetContext(context, NULL);
}

const char* DebuggeeThread::GetStateName(State state) {
  switch (state) {
    case RUNNING: return "RUNNING";
    case HALTED: return "HALTED";
    case ZOMBIE: return "ZOMBIE";
    case DEAD: return "DEAD";
    case INIT: return "INIT";
  }
  return "N/A";
}

void DebuggeeThread::Kill() {
 if (NULL != handle_) 
  ::TerminateThread(handle_, 0);
}

bool DebuggeeThread::GetContext(CONTEXT* context, std::string* error) {
  context->ContextFlags = CONTEXT_ALL;
  bool result = (::GetThreadContext(handle_, context) != FALSE);
  if (!result && (NULL != error))
    *error = System::GetLastErrorDescription();
  return result;
}

bool DebuggeeThread::SetContext(const CONTEXT& context, std::string* error) {
  bool result = (::SetThreadContext(handle_, &context) != FALSE);
  if (!result && (NULL != error))
    *error = System::GetLastErrorDescription();
  return result;
}

long long DebuggeeThread::GetIP() {
  CONTEXT ct;
  GetContext(&ct, NULL);
#ifdef _WIN64
  return ct.Rip;
#else
  return ct.Eip;
#endif
}

void DebuggeeThread::SetIP(long long ip) {
  CONTEXT ct;
  GetContext(&ct, NULL);
#ifdef _WIN64
  ct.Rip = ip;
#else
  ct.Eip = ip;
#endif
  SetContext(ct, NULL);
}

Core::Core()
  : root_process_id_(0),
    event_observer_(NULL),
    continue_policy_(NULL),
    inside_observer_(NULL) {
}

Core::~Core() {
  Stop();
}

bool Core::StartProcess(const char* cmd, const char* work_dir, std::string* error) {
  Stop();

  STARTUPINFO si;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(pi));

  char* cmd_dup = _strdup(cmd);
  if (NULL == cmd_dup) {
    *error = "Memory allocation error.";
    return false;
  }
  BOOL res = ::CreateProcess(NULL, 
                             cmd_dup,
                             NULL,
                             NULL,
                             FALSE,
                             DEBUG_PROCESS | CREATE_NEW_CONSOLE,
                             NULL,
                             work_dir,
                             &si,
                             &pi);
  free(cmd_dup);
  if (!res) {
    if (NULL != error)
      *error = System::GetLastErrorDescription();
    return false;
  }

  ::CloseHandle(pi.hThread);
  ::CloseHandle(pi.hProcess);
  root_process_id_ = pi.dwProcessId;
  return true;
}

void Core::SetInsideObserver(CoreInsideObserver* inside_observer) {
 inside_observer_ = inside_observer; 
 inside_observer_->SetCore(this);
}

CoreInsideObserver& Core::inside_observer() {
  static CoreInsideObserver default_inside_observer;  // Observer that does nothing.
  if (NULL == inside_observer_)
    return default_inside_observer;
 return *inside_observer_;
}

DebuggeeProcess* Core::GetProcess(int id) {
  std::deque<DebuggeeProcess*>::const_iterator it = processes_.begin();
  while (it != processes_.end()) {
    DebuggeeProcess* proc = *it;
    ++it;
    if ((NULL != proc) && (id == proc->id_))
      return proc;
  }
  return NULL;
}

void Core::GetProcessesIds(std::deque<int>* processes) const {
  processes->clear();
  std::deque<DebuggeeProcess*>::const_iterator it = processes_.begin();
  while (it != processes_.end()) {
    DebuggeeProcess* proc = *it;
    ++it;
    if (NULL != proc)
      processes->push_back(proc->id_);
  }
}

void Core::OnDebugEvent(DEBUG_EVENT& de) {
  if (NULL != event_observer_)
    event_observer_->OnDebugEvent(de);
  inside_observer().OnDEBUG_EVENT(de);

  DebuggeeProcess* process = GetProcess(de.dwProcessId);
  DecisionToContinue dtc;
  if (CREATE_PROCESS_DEBUG_EVENT == de.dwDebugEventCode) {
    process = new DebuggeeProcess(*this,
                                  de.dwProcessId,
                                  de.u.CreateProcessInfo.hProcess,
                                  de.dwThreadId,
                                  de.u.CreateProcessInfo.hThread,
                                  de.u.CreateProcessInfo.hFile);
    if (NULL != process)
      processes_.push_back(process);
  }

  if (NULL != continue_policy_)
    continue_policy_->MakeContinueDecision(de, &dtc);

  if (NULL != process)
    process->OnDebugEvent(de, &dtc);

  inside_observer().OnDecisionToContinue(de.dwProcessId, de.dwThreadId, dtc);

  if (!dtc.IsHaltDecision()) {
    int flag = (dtc.pass_exception_to_debuggee() ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE);
    ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, flag);
  }
}

void Core::DoWork(int wait_ms) {
  DEBUG_EVENT de;
  while (::WaitForDebugEvent(&de, 0))
    OnDebugEvent(de);

  if (::WaitForDebugEvent(&de, wait_ms))
    OnDebugEvent(de);
}

bool Core::HasAliveDebugee() const {
  std::deque<DebuggeeProcess*>::const_iterator it = processes_.begin();
  while (it != processes_.end()) {
    DebuggeeProcess* proc = *it;
    ++it;
    if ((NULL != proc) && (proc->GetState() != DebuggeeProcess::DEAD))
      return true;
  }
  return false;
}

void Core::Stop() {
  std::deque<DebuggeeProcess*>::const_iterator it = processes_.begin();
  while (it != processes_.end()) {
    DebuggeeProcess* proc = *it;
    ++it;
    if (NULL != proc)
      proc->Kill();
  }
  while (HasAliveDebugee())
    DoWork(20);

  while (processes_.size() > 0) {
    DebuggeeProcess* proc = processes_.front();
    processes_.pop_front();
    if (NULL != proc)
      delete proc;
  }
}

DebuggeeProcess::DebuggeeProcess(Core& core,
                                 int process_id,
                                 HANDLE handle,
                                 int thread_id,
                                 HANDLE thread_handle,
                                 HANDLE file_handle)
  : core_(core),
    state_(RUNNING),
    id_(process_id),
    handle_(handle),
    halted_thread_(NULL),
    file_handle_(file_handle),
    last_user_command_(NONE) {
    AddThread(thread_id, thread_handle);
}

DebuggeeProcess::~DebuggeeProcess() {
  std::deque<DebuggeeThread*>::const_iterator it = threads_.begin();
  while (it != threads_.end()) {
    DebuggeeThread* thread = *it;
    ++it;
    if (NULL != thread)
      delete thread;
  }  
  threads_.clear();
  ::CloseHandle(file_handle_);
}

void DebuggeeProcess::Kill() {
  last_user_command_ = KILL;
  std::deque<DebuggeeThread*>::const_iterator it = threads_.begin();
  while (it != threads_.end()) {
    DebuggeeThread* thread = *it;
    ++it;
    if (NULL != thread)
      thread->Kill();
  }
  Continue();
}

DebuggeeProcess::State DebuggeeProcess::GetState() const {
  return state_;
}

void DebuggeeProcess::AddThread(int id, HANDLE handle) {
  if (NULL != GetThread(id))
    return;
  DebuggeeThread* thread = new DebuggeeThread(id, handle, *this);
  threads_.push_back(thread);
}

DebuggeeThread* DebuggeeProcess::GetThread(int id) {
  std::deque<DebuggeeThread*>::iterator it = threads_.begin();
  while (it != threads_.end()) {
    DebuggeeThread* thread = *it;
    if (thread->id_ == id)
      return thread;
    ++it;
  }
  return NULL;
}

DebuggeeThread* DebuggeeProcess::GetHaltedThread() {
  return halted_thread_;
}

int DebuggeeProcess::GetCurrentException() const {
  return current_exception_id_;
}

void DebuggeeProcess::Break() {
  last_user_command_ = BREAK;
  if (!::DebugBreakProcess(handle_)) {
    std::string error = System::GetLastErrorDescription();
    printf("DebugBreakProcess error: [%s]\n", error.c_str());
  }
}

void DebuggeeProcess::GetThreadIds(std::deque<int>* threads) const {
  std::deque<DebuggeeThread*>::const_iterator it = threads_.begin();
  while (it != threads_.end()) {
    DebuggeeThread* thread = *it++;
    threads->push_back(thread->id_);
  }
}

void DebuggeeProcess::RemoveThread(int id) {
  std::deque<DebuggeeThread*>::iterator it = threads_.begin();
  while (it != threads_.end()) {
    DebuggeeThread* thread = *it;
    if (thread->id_ == id) {
      threads_.erase(it);
      if (halted_thread_ == thread) {
        halted_thread_ = NULL;
      }
      delete thread;
      break;
    }
    ++it;
  }
}

void DebuggeeProcess::SingleStep() {
  last_user_command_ = STEP;
  if (NULL != halted_thread_)
    halted_thread_->SingleStep();
}

bool DebuggeeProcess::ReadMemory(const void* addr, size_t size, void* content) {
  SIZE_T sz = 0;
  if (!::ReadProcessMemory(handle_, addr, content, size, &sz)) {
    std::string error = System::GetLastErrorDescription();
    printf("ReadProcessMemory error: rd %d bytes [%s]\n", sz, error.c_str());
    return false;
  }
  return true;
}

bool DebuggeeProcess::WriteMemory(const void* addr, const void* content, size_t size) { 
  SIZE_T wr = 0;
  BOOL res = ::WriteProcessMemory(handle_, (LPVOID)addr, content, size, &wr);
  if (!res) {
    std::string error = System::GetLastErrorDescription();
    printf("WriteProcessMemory error: [%s]\n", error.c_str());
    return false;
  }
  res = ::FlushInstructionCache(handle_, addr, size);
  if (!res) {
    std::string error = System::GetLastErrorDescription();
    printf("FlushInstructionCache error: [%s]\n", error.c_str());
    return false;
  }
  return true;
}

void DebuggeeProcess::OnDebugEvent(DEBUG_EVENT& de, DecisionToContinue* dtc) {
  switch (de.dwDebugEventCode) {
    case CREATE_THREAD_DEBUG_EVENT: AddThread(de.dwThreadId, de.u.CreateThread.hThread); break;
    case EXIT_PROCESS_DEBUG_EVENT: state_ = DEAD; break;
  }
  
  DebuggeeThread* thread = GetThread(de.dwThreadId);
  if (NULL != thread) {
    thread->OnDebugEvent(de, dtc);
    if (thread->state_ == DebuggeeThread::DEAD) {
      RemoveThread(de.dwThreadId);
    } else if (thread->state_ == DebuggeeThread::HALTED) {
      halted_thread_ = thread;
    }
  }
}


void DebuggeeProcess::ContinueAndPassExceptionToDebuggee() {
  last_user_command_ = CONTINUE;
  if (NULL != halted_thread_) {
    halted_thread_->ContinueAndPassExceptionToDebuggee();
    halted_thread_ = NULL;
  }
}

void DebuggeeProcess::Continue() {
  last_user_command_ = CONTINUE;
  if (NULL != halted_thread_) {
    halted_thread_->Continue();
    halted_thread_ = NULL;
  }
}

bool DebuggeeProcess::IsHalted() const {
  return (NULL != halted_thread_);
}

bool DebuggeeProcess::HasNexeThread() const {
  std::deque<DebuggeeThread*>::const_iterator it = threads_.begin();
  while (it != threads_.end()) {
    DebuggeeThread* thread = *it++;
    if (thread->is_nexe())
      return true;
  }
  return false;
}

bool DebuggeeProcess::SetBreakpoint(void* addr) {
  RemoveBreakpoint(addr);
  Breakpoint br(addr);
  if (br.Init(*this)) {
    breakpoints_[addr] = br;
    return true;
  } 
  return false;
}

Breakpoint* DebuggeeProcess::GetBreakpoint(void* addr) {
  std::map<void*, Breakpoint>::iterator it = breakpoints_.find(addr);
  if (breakpoints_.end() == it)
    return NULL;
  return &it->second;
}

void DebuggeeProcess::RemoveBreakpoint(void* addr) {
  Breakpoint br = breakpoints_[addr];
  if (br.IsValid()) {
    br.RecoverCodeAtBreakpoint(*this);
    breakpoints_.erase(breakpoints_.find(addr));
  }
}
}  // namespace debug