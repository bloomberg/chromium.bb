#include "StdAfx.h"
#include <algorithm>

#include "debug_core.h"
#include "debug_system.h"

#pragma warning(disable : 4996)

namespace {
const int kVS2008_THREAD_INFO = 0x406D1388;
const unsigned char kBreakpointCode = 0xCC;
}  // namespace

namespace debug {
DebuggeeThread::DebuggeeThread(int id, HANDLE handle)
    : id_(id),
      handle_(handle),
      state_(RUNNING),
      last_user_command_(NONE),
      current_breakpoint_(NULL),
      is_nexe_(false),
      nexe_mem_base_(0),
      nexe_entry_point_(0) {
}

DebuggeeThread::~DebuggeeThread() {
}

bool DebuggeeThread::OnDebugEvent(DEBUG_EVENT& de, bool* continue_event, bool* exception_handled, DebuggeeProcess& process) {
  //TODO: implement
  return false;
}

bool DebuggeeThread::MakeContinueDecision(DEBUG_EVENT& de,
                                    DebuggeeProcess& process,
                                    bool* continue_event,
                                    bool* exception_handled) {
  //TODO: implement
  return false;
}

void DebuggeeThread::Continue() {
  //last_user_command_ = 
  //TODO: implement
}

void DebuggeeThread::SingleStep() {
  //TODO: implement
}

const char* DebuggeeThread::GetStateName() const {
  switch (state_) {
    case RUNNING: return "RUNNING";
    case HALTED: return "HALTED";
    case ZOMBIE: return "ZOMBIE";
    case DEAD: return "DEAD";
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
    event_observer_(NULL) {
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
  DebuggeeProcess* proc = GetProcess(de.dwProcessId);
  DebuggeeThread* thread = NULL;
  if (NULL != proc)
    thread = proc->GetThread(de.dwThreadId);

  switch (de.dwDebugEventCode) {
    case LOAD_DLL_DEBUG_EVENT: break;
    case UNLOAD_DLL_DEBUG_EVENT: break;
    default:
      if (NULL != event_observer_) {
        event_observer_->OnDebugEvent(de);
      }
  }

  bool continue_event = true;
  int continue_status = DBG_CONTINUE;

  switch (de.dwDebugEventCode) {
    case OUTPUT_DEBUG_STRING_EVENT: {
      char* tmp = (char*)malloc(de.u.DebugString.nDebugStringLength + 1);
      SIZE_T rd = 0;
      BOOL res = ::ReadProcessMemory(proc->handle_, de.u.DebugString.lpDebugStringData, tmp, de.u.DebugString.nDebugStringLength + 1, &rd);
      tmp[rd] = 0;
      printf("OutputDebugString=[%s]\n", tmp);
      const char* nexe_uuid_str = "{7AA7C9CF-89EC-4ed3-8DAD-6DC84302AB11}";
      if (strncmp(tmp, nexe_uuid_str, strlen(nexe_uuid_str)) == 0) {
        if (NULL != proc) {
          DebuggeeThread* thread = proc->GetThread(de.dwThreadId);
          if (NULL != thread) {
            continue_event = false;
            thread->is_nexe_ = true;
            thread->state_ = DebuggeeThread::HALTED;
            proc->current_debug_event_code_ = OUTPUT_DEBUG_STRING_EVENT;
            proc->halted_thread_ = thread;
  
            void* tread_inf_addr = 0;
            sscanf(tmp + strlen(nexe_uuid_str), "%p", &tread_inf_addr);
            NaclInfo nacl_info;
            nacl_info.Init(proc, tread_inf_addr);
            thread->nexe_mem_base_ = nacl_info.GetMemBase();
            thread->nexe_entry_point_ = nacl_info.GetEntryPoint();
            printf("Nexe: mem_base=0x%p entry_point=0x%p\n", thread->nexe_mem_base_, thread->nexe_entry_point_);
          }
        }
      }
      break;
    }
    case LOAD_DLL_DEBUG_EVENT: break;
    case UNLOAD_DLL_DEBUG_EVENT: break;
    case CREATE_PROCESS_DEBUG_EVENT: {
        proc = new DebuggeeProcess(de.dwProcessId,
                                   de.u.CreateProcessInfo.hProcess,
                                   de.dwThreadId,
                                   de.u.CreateProcessInfo.hThread,
                                   de.u.CreateProcessInfo.hFile);
        if (NULL != proc)
          processes_.push_back(proc);
    }
    default: {
      bool decided = false;
      if (proc) {
        bool exception_handled = true;
        continue_status = DBG_CONTINUE;
        decided = proc->OnDebugEvent(de, &continue_event, &exception_handled);
        if (decided && !exception_handled)
          continue_status = DBG_EXCEPTION_NOT_HANDLED;
      } 
      if (!decided) {
        continue_event = true;
        continue_status = DBG_EXCEPTION_NOT_HANDLED;
        if (de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT)
          continue_status = DBG_CONTINUE;
      }
    }
  }

  if (continue_event)
    ::ContinueDebugEvent(de.dwProcessId,
                         de.dwThreadId,
                         continue_status);
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

DebuggeeProcess::DebuggeeProcess(int process_id,
                               HANDLE handle,
                               int thread_id,
                               HANDLE thread_handle,
                               HANDLE file_handle)
  : state_(RUNNING),
    id_(process_id),
    handle_(handle),
    halted_thread_(NULL),
    file_handle_(file_handle) {
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
  DebuggeeThread* thread = new DebuggeeThread(id, handle);
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

void DebuggeeProcess::EnableSingleStep(bool enable) {
  if (NULL != halted_thread_) {
    CONTEXT context;
    halted_thread_->GetContext(&context, NULL);
    if (enable)
      context.EFlags |= 1 << 8;
    else
      context.EFlags &= ~(1 << 8);
    halted_thread_->SetContext(context, NULL);
  }
}

void DebuggeeProcess::SingleStep() {
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

bool DebuggeeProcess::OnDebugEvent(DEBUG_EVENT& de, bool* continue_event, bool* exception_handled) {
  switch (de.dwDebugEventCode) {
    case CREATE_PROCESS_DEBUG_EVENT:
    case CREATE_THREAD_DEBUG_EVENT: break;
    case EXIT_PROCESS_DEBUG_EVENT: state_ = DEAD; break;
  }
  
  bool continue_desision_made = false;
  DebuggeeThread* thread = GetThread(de.dwThreadId);
  if (NULL != thread) {
    continue_desision_made = thread->OnDebugEvent(de, continue_event, exception_handled, *this);
    if (thread->state_ == DebuggeeThread::DEAD) {
      if (thread == halted_thread_)
        halted_thread_ = NULL;
      RemoveThread(de.dwThreadId);
    } else if (continue_desision_made && (false == *continue_event)) {
      halted_thread_ = thread;
    }
  }
  return continue_desision_made;
}


void DebuggeeProcess::Continue() {
  if (NULL != halted_thread_) {
    halted_thread_->Continue();
    halted_thread_ = NULL;
  }
}

bool DebuggeeProcess::IsHalted() const {
  return (NULL != halted_thread_);
}

void DebuggeeProcess::SetBreakpoint(void* addr) {
  if (NULL == halted_thread_)
    return;

  RemoveBreakpoint(addr);

  if (halted_thread_->is_nexe_)
    addr = reinterpret_cast<char*>(addr) + reinterpret_cast<size_t>(halted_thread_->nexe_mem_base_);

  Breakpoint br(addr);
  if (ReadMemory(addr, sizeof(br.original_code_byte_), &br.original_code_byte_)) {
    if (!WriteMemory(addr, &kBreakpointCode, sizeof(kBreakpointCode)))
      printf("WriteMemory failed.\n");
    breakpoints_[addr] = br;
  }
}

void DebuggeeProcess::RemoveBreakpoint(void* addr) {
  if (NULL == halted_thread_)
    return;

  if (halted_thread_->is_nexe_)
    addr = reinterpret_cast<char*>(addr) + reinterpret_cast<size_t>(halted_thread_->nexe_mem_base_);

  Breakpoint br = breakpoints_[addr];
  if (NULL != br.address_) {
    WriteMemory(addr, &br.original_code_byte_, sizeof(br.original_code_byte_));
    breakpoints_.erase(breakpoints_.find(addr));
  }
}

}  // namespace debug