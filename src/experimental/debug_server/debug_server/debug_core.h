#pragma once
#include <string>
#include <deque>
#include <windows.h>
#include "my_json.h"
#include "debug_registers.h"
#include "debug_nacl_info.h"

//#include "G:\\boost_1_43_0\\boost\\cstdint.hpp"

namespace debug {
class Core;
class DebuggeeProcess;
class DebuggeeThread;
class Breakpoint;
class CoreEventObserver;
class DecisionToContinue;
class ContinuePolicy;
class CoreInsideObserver;

class Core
{
public:
  Core();
  virtual ~Core();

  void SetEventObserver(CoreEventObserver* observer) { event_observer_ = observer; }
  void SetContinuePolicy(ContinuePolicy* continue_policy) { continue_policy_ = continue_policy; }
  void SetInsideObserver(CoreInsideObserver* inside_observer);
  CoreInsideObserver& inside_observer();
  
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
  ContinuePolicy* continue_policy_;
  CoreInsideObserver* inside_observer_;
};

class DebuggeeProcess {
 public:
  enum State {RUNNING, PAUSED, HIT_BREAKPOINT, DEAD};
  enum UserCommand {NONE, CONTINUE, STEP, BREAK, KILL};

  DebuggeeProcess(Core& core,
                  int process_id,
                  HANDLE handle,
                  int thread_id,
                  HANDLE thread_handle,
                  HANDLE file_handle);
  virtual ~DebuggeeProcess();

  void Continue();
  void ContinueAndPassExceptionToDebuggee();
  void SingleStep();
  void Break();
  void Kill();
  bool SetBreakpoint(void* addr);
  void RemoveBreakpoint(void* addr);
  Breakpoint* GetBreakpoint(void* addr);

  Core& core() { return core_; }
  int id() const {return id_;}
  HANDLE Handle() const { return handle_; }
  State GetState() const;
  bool IsHalted() const;
  bool HasNexeThread() const;
  CoreInsideObserver& inside_observer() { return core().inside_observer(); }

  DebuggeeThread* GetThread(int id);
  DebuggeeThread* GetHaltedThread();

  int GetCurrentException() const;
  int GetCurrentDebugEventCode() const;

  void GetThreadIds(std::deque<int>* threads) const;
  bool ReadMemory(const void* addr, size_t size, void* content);
  bool WriteMemory(const void* addr, const void* content, size_t size);
  
  UserCommand last_user_command() const { return last_user_command_;}

 protected:
  void OnDebugEvent(DEBUG_EVENT& de, DecisionToContinue* dtc);
  void Clean();
  void AddThread(int id, HANDLE handle);
  void RemoveThread(int id);
  
  Core& core_;
  State state_;
  int id_;
  HANDLE handle_;
  HANDLE file_handle_;
  std::deque<DebuggeeThread*> threads_;
  int current_exception_id_;
  int current_debug_event_code_;
  std::map<void*, Breakpoint> breakpoints_;
  DebuggeeThread* halted_thread_;
  UserCommand last_user_command_;

  friend class Core;
};

class Breakpoint {
 public:
  Breakpoint() : address_(NULL), original_code_byte_(0), valid_(false) {}
  Breakpoint(void* address) : address_(address), original_code_byte_(0), valid_(true) {}
  ~Breakpoint() {}

  void* address() { return address_; }
  void Invalidate() { valid_ = false; }
  bool IsValid() const { return valid_; }

  bool Init(DebuggeeProcess& process);
  bool WriteBreakpointCode(DebuggeeProcess& process);
  bool RecoverCodeAtBreakpoint(DebuggeeProcess& process);

 private:
  void* address_;
  char original_code_byte_;
  bool valid_;
};

class DebuggeeThread {
 public:
  enum State {INIT, RUNNING, HALTED, ZOMBIE, DEAD};
  enum UserCommand {NONE, CONTINUE, STEP, BREAK, KILL};

  DebuggeeThread(int id, HANDLE handle, DebuggeeProcess& parent_process);
  virtual ~DebuggeeThread();

  DebuggeeProcess& process() { return parent_process_; }
  int id() const {return id_;}
  void Kill();
  bool GetContext(CONTEXT* context, std::string* error);
  bool SetContext(const CONTEXT& context, std::string* error);
  long long GetIP();
  void SetIP(long long ip);
  void* IpToFlatAddress(long long ip) const;
  void* IpToFlatAddress(void* ip) const;
  bool IsHalted() const {return (state_ == HALTED);}
  State GetState() const {return state_;}
  static const char* GetStateName(State state);
  void Continue();
  void ContinueAndPassExceptionToDebuggee();
  void SingleStep();
  void EnableSingleStep(bool enable);

  virtual void OnDebugEvent(DEBUG_EVENT& de, DecisionToContinue* dtc);
  UserCommand GetLastUserCommand() const {return last_user_command_;}

  bool is_nexe() const { return is_nexe_; }
  void* nexe_mem_base() const { return nexe_mem_base_; }
  void* nexe_entry_point() const { return nexe_entry_point_; }

 public:  //TODO: change to private
  CoreInsideObserver& inside_observer();

  void OnOutputDebugString(DEBUG_EVENT& de, DecisionToContinue* dtc);
  void OnExitThread(DEBUG_EVENT& de, DecisionToContinue* dtc);
  void OnBreakpoint(DEBUG_EVENT& de, DecisionToContinue* dtc);
  void OnSingleStep(DEBUG_EVENT& de, DecisionToContinue* dtc);
  
  void ContinueFromHalted(bool single_step, bool pass_exception_to_debuggee);

  void EnterRunning();
  void EnterHalted();
  void EnterZombie();
  void EnterDead();

  void EmitNexeThreadStarted();
  void EmitHalted();
  void EmitRunning();
  void EmitDead();

  int id_;
  HANDLE handle_;
  DebuggeeProcess& parent_process_;
  State state_;
  UserCommand last_user_command_;

  //Current breakpoint, if any.
  Breakpoint current_breakpoint_;
  
  // Stuff related only to nexe threads.
  bool is_nexe_;
  void* nexe_mem_base_;
  void* nexe_entry_point_;
};

class CoreEventObserver {
 public:
  virtual ~CoreEventObserver() {}
  virtual void OnDebugEvent(DEBUG_EVENT de) = 0;
};

class DecisionToContinue {
 public:
  enum DecisionStrength {NO_DECISION, WEAK_DECISION, STRONG_DECISION};
  DecisionToContinue();
  DecisionToContinue(DecisionStrength strength,
                     bool halt_debuggee, 
                     bool pass_exception_to_debuggee=false);
  bool operator==(const DecisionToContinue& other) const;

  // Returns false if decisions are incompatible.
  bool Combine(const DecisionToContinue& other);
  
  bool IsHaltDecision() const;
  DecisionStrength decision_strength() const;
  bool halt_debuggee() const;
  bool pass_exception_to_debuggee() const;

 private:
  DecisionStrength decision_strength_;
  bool halt_debuggee_;
  bool pass_exception_to_debuggee_;
};

class ContinuePolicy {
 public:
  ContinuePolicy() {}
  virtual ~ContinuePolicy() {}
  virtual void MakeContinueDecision(DEBUG_EVENT& de, DecisionToContinue* dtc) = 0;
};

class StandardContinuePolicy : public ContinuePolicy {
 public:
  StandardContinuePolicy(Core& debug_core) : debug_core_(debug_core) {}

  virtual void MakeContinueDecision(DEBUG_EVENT& de, DecisionToContinue* dtc);

 private:
  Core& debug_core_;
};

class CoreInsideObserver {
 public:
  CoreInsideObserver() : debug_core_(NULL) {}
  virtual ~CoreInsideObserver() {}
  void SetCore(Core* debug_core) { debug_core_ = debug_core; }
  
  virtual void OnDEBUG_EVENT(DEBUG_EVENT& de) {}
  virtual void OnDecisionToContinue(int process_id, int thread_id, DecisionToContinue& dtc) {}
  virtual void OnBreakpointHit(DebuggeeThread* thread, void* addr, bool our) {}
  virtual void OnThreadStateChange(DebuggeeThread* thread, DebuggeeThread::State old_state, DebuggeeThread::State new_state) {}
  virtual void OnThreadSetSingleStep(DebuggeeThread* thread, bool enable) {}
  virtual void OnThreadOutputDebugString(DebuggeeThread* thread, const char* str) {}
  virtual void OnNexeThreadStarted(DebuggeeThread* thread) {}
  virtual void OnWriteBreakpointCode(int process_id, void* addr, bool result) {}
  virtual void OnRecoverCodeAtBreakpoint(int process_id, void* addr, char orig_code, bool result) {}
  virtual void OnSingleStepDueToContinueFromBreakpoint(DebuggeeThread* thread, void* breakpoint_addr) {}
  virtual void OnBreakinBreakpoint(DebuggeeThread* thread) {}

 protected:
  Core* debug_core_;
};
}  // namespace debug