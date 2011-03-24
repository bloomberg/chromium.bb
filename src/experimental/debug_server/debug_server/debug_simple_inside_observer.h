#pragma once
#include "debug_core.h"

//TODO: redirect output to text file.
//TODO: add DEBUG_EVENT->json.
//TODO: add OnError(...)

namespace debug {
class SimpleInsideObserver : public CoreInsideObserver{
 public:
  
  virtual void OnDEBUG_EVENT(DEBUG_EVENT& de);
  virtual void OnDecisionToContinue(int process_id, int thread_id, DecisionToContinue& dtc);
  virtual void OnBreakpointHit(DebuggeeThread* thread, void* addr, bool our);
  virtual void OnThreadStateChange(DebuggeeThread* thread, DebuggeeThread::State old_state, DebuggeeThread::State new_state);
  virtual void OnThreadSetSingleStep(DebuggeeThread* thread, bool enable);
  virtual void OnThreadOutputDebugString(DebuggeeThread* thread, const char* str);
  virtual void OnNexeThreadStarted(DebuggeeThread* thread);
  virtual void OnWriteBreakpointCode(int process_id, void* addr, bool result);
  virtual void OnRecoverCodeAtBreakpoint(int process_id, void* addr, char orig_code, bool result);
  virtual void OnSingleStepDueToContinueFromBreakpoint(DebuggeeThread* thread, void* breakpoint_addr);
  virtual void OnBreakinBreakpoint(DebuggeeThread* thread);
};
}  // namespace debug
