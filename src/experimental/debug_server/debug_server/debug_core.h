#pragma once
#include <string>
#include <deque>
#include <windows.h>
#include "my_json.h"
#include "debug_registers.h"
//#include "debug_breakin_point.h"
#include "debug_nacl_info.h"

namespace debug {
class DebuggeeProcess;
class DebuggeeThread;
class Breakpoint;
class CoreEventObserver;

class Core
{
public:
  Core();
  virtual ~Core();

  void SetEventObserver(CoreEventObserver* observer) { event_observer_ = observer; }
  bool StartProcess(const char* cmd, const char* work_dir, std::string* error);
  void DoWork(int wait_ms);
  void Stop();  // Blocks until all debugee processes terminated.

  DebuggeeProcess* GetProcess(int id);
  void GetProcessesIds(std::deque<int>* processes) const;

 protected:
  void OnDebugEvent(DEBUG_EVENT& de);
  bool HasAliveDebugee() const;

  std::deque<DebuggeeProcess*> processes_;
  int root_process_id_;
  CoreEventObserver* event_observer_;
};

class DebuggeeThread {
 public:
  enum State {RUNNING, HALTED, ZOMBIE, DEAD};
  enum UserCommand {NONE, CONTINUE, STEP, BREAK, KILL};

  DebuggeeThread(int id, HANDLE handle);
  virtual ~DebuggeeThread();
  //void OnCREATE_THREAD_DEBUG_INFO(CREATE_THREAD_DEBUG_INFO inf);

  void Kill();
  bool GetContext(CONTEXT* context, std::string* error);
  bool SetContext(const CONTEXT& context, std::string* error);
  long long GetIP();
  void SetIP(long long ip);
  bool IsHalted() const {return (state_ == HALTED);}
  State GetState() const {return state_;}
  const char* GetStateName() const;
  void Continue();
  void SingleStep();

  virtual bool OnDebugEvent(DEBUG_EVENT& de, bool* continue_event, bool* exception_handled, DebuggeeProcess& process);
  virtual bool MakeContinueDecision(DEBUG_EVENT& de,
                                    DebuggeeProcess& process,
                                    bool* continue_event,
                                    bool* exception_handled);

  UserCommand GetLastUserCommand() const {return last_user_command_;}
 public:
  int id_;
  HANDLE handle_;
  State state_;
  UserCommand last_user_command_;

  //Current breakpoint, if any.
  Breakpoint* current_breakpoint_;
  
  // Stuff related only to nexe threads.
  bool is_nexe_;
  void* nexe_mem_base_;
  void* nexe_entry_point_;
};

class Breakpoint {
 public:
  Breakpoint() : address_(NULL), original_code_byte_(0) {}
  Breakpoint(void* address) : address_(address), original_code_byte_(0) {}
  ~Breakpoint() {}

  void* address() { return address_; }

 public:
  void* address_;
  char original_code_byte_;
};

class DebuggeeProcess {
 public:
  enum State {RUNNING, PAUSED, HIT_BREAKPOINT, DEAD};

  DebuggeeProcess(int process_id,
                 HANDLE handle,
                 int thread_id,
                 HANDLE thread_handle,
                 HANDLE file_handle);
  virtual ~DebuggeeProcess();

  void Continue();
  void SingleStep();
  void Break();
  void Kill();
  void SetBreakpoint(void* addr);
  void RemoveBreakpoint(void* addr);

  int id() const {return id_;}
  HANDLE Handle() const { return handle_; }
  State GetState() const;
  bool IsHalted() const;
  DebuggeeThread* GetThread(int id);
  DebuggeeThread* GetHaltedThread();

  int GetCurrentException() const;
  int GetCurrentDebugEventCode() const;

  void GetThreadIds(std::deque<int>* threads) const;
  bool ReadMemory(const void* addr, size_t size, void* content);
  bool WriteMemory(const void* addr, const void* content, size_t size);
  void EnableSingleStep(bool enable);
  
 protected:
  // Returns true if decision has been made on how to proseed.
  bool OnDebugEvent(DEBUG_EVENT& de, bool* continue_event, bool* exception_handled);
  void Clean();
  void AddThread(int id, HANDLE handle);
  void RemoveThread(int id);
  
  State state_;
  int id_;
  HANDLE handle_;
  HANDLE file_handle_;
  std::deque<DebuggeeThread*> threads_;
  int current_exception_id_;
  int current_debug_event_code_;
  std::map<void*, Breakpoint> breakpoints_;
  DebuggeeThread* halted_thread_;

  friend class Core;
};

class CoreEventObserver {
 public:
  virtual ~CoreEventObserver() {}
  virtual void OnDebugEvent(DEBUG_EVENT de) = 0;
};

}  // namespace debug