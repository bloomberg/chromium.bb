#include "StdAfx.h"
#include "debug_simple_inside_observer.h"
#include <sys/timeb.h>
#include <sys/types.h> 
#include <sys/stat.h> 

namespace debug {
void Prefix(const char* func_name) {
  _timeb tmb;
	_ftime_s( &tmb );
	time_t now = tmb.time;
	int  ms = tmb.millitm;
  printf("----%d:%03d %s: ", now, ms, func_name);
}

void Suffix() {
  fflush(stdout);
}

void SimpleInsideObserver::OnDEBUG_EVENT(DEBUG_EVENT& de) {
  Prefix("OnDEBUG_EVENT");
  static int count = 0;
  count++;
  printf("%d dwDebugEventCode=0x%X dwProcessId=0x%X dwThreadId=0x%X\n", count, de.dwDebugEventCode, de.dwProcessId, de.dwThreadId);
  Suffix();
}

void SimpleInsideObserver::OnDecisionToContinue(int process_id, int thread_id, DecisionToContinue& dtc) {
  DebuggeeProcess* process = debug_core_->GetProcess(process_id);
  DebuggeeThread* thread = NULL;
  bool is_nexe = false;
  if (NULL != process)
    thread = process->GetThread(thread_id);

  if (NULL != thread)
    is_nexe = thread->is_nexe();

  if (!is_nexe)
    return;

  Prefix("OnDecisionToContinue");
  const char* dec_strength = "NO_DECISION";
  if (dtc.decision_strength() == DecisionToContinue::WEAK_DECISION)
    dec_strength = "WEAK_DECISION";
  else if (dtc.decision_strength() == DecisionToContinue::STRONG_DECISION)
    dec_strength = "STRONG_DECISION";

  printf("process_id=0x%X thread_id=0x%X decision=%s strength=%s pass_exception_to_debuggee=%s\n",
         process_id,
         thread_id,
         dtc.halt_debuggee() ? "Halt" : "Continue",
         dec_strength,
         dtc.pass_exception_to_debuggee() ? "yes" : "no");
  Suffix();
}

void SimpleInsideObserver::OnThreadOutputDebugString(DebuggeeThread* thread, const char* str) {
  Prefix("OnThreadOutputDebugString");
  printf("dwThreadId=0x%X str=[%s]\n", thread->id(), str);
  Suffix();
}

void SimpleInsideObserver::OnBreakpointHit(DebuggeeThread* thread, void* addr, bool our) {
  Prefix("OnBreakpointHit");
  if (thread->is_nexe())
    printf("*");

  printf("dwThreadId=0x%X br_addr=0x%p our=%s\n", thread->id(), addr, our ? "true" : "false");
  Suffix();
}

void SimpleInsideObserver::OnThreadStateChange(DebuggeeThread* thread, DebuggeeThread::State old_state, DebuggeeThread::State new_state) {
  Prefix("OnThreadStateChange");
  printf("dwThreadId=0x%X old_state=%s new_state=%s\n", thread->id(), DebuggeeThread::GetStateName(old_state), DebuggeeThread::GetStateName(new_state));
  Suffix();
}

void SimpleInsideObserver::OnThreadSetSingleStep(DebuggeeThread* thread, bool enable) {
  Prefix("OnThreadSetSingleStep");
  printf("dwThreadId=0x%X enable=%s\n", thread->id(), enable ? "true" : "false");
  Suffix();
}

void SimpleInsideObserver::OnNexeThreadStarted(DebuggeeThread* thread) {
  Prefix("OnNexeThreadStarted");
  printf("dwThreadId=0x%X nexe_mem_base=%p nexe_entry_point=%p\n", thread->id(), thread->nexe_mem_base(), thread->nexe_entry_point());
  Suffix();
}

void SimpleInsideObserver::OnWriteBreakpointCode(int process_id, void* addr, bool result) {
  Prefix("OnWriteBreakpointCode");
  printf("process_id=0x%X addr=%p result=%s\n", process_id, addr, result ? "ok" : "failed");
  Suffix();
}

void SimpleInsideObserver::OnRecoverCodeAtBreakpoint(int process_id, void* addr, char orig_code, bool result) {
  Prefix("RecoverCodeAtBreakpoint");
  printf("process_id=0x%X addr=%p code=0x%X result=%s\n", process_id, addr, (int)orig_code, result ? "ok" : "failed");
  Suffix();
}

void SimpleInsideObserver::OnSingleStepDueToContinueFromBreakpoint(DebuggeeThread* thread, void* breakpoint_addr) {
  Prefix("OnSingleStepDueToContinueFromBreakpoint");
  printf("thread_id=0x%X breakpoint_addr=%p\n", thread->id(), breakpoint_addr);
  Suffix();
}

//TODO: it'snot goint to work : break was initiated from thread A,
// but breakpoint hit is in thread B :(
//
void SimpleInsideObserver::OnBreakinBreakpoint(DebuggeeThread* thread) {  
  Prefix("OnBreakinBreakpoint");
  printf("thread_id=0x%X\n", thread->id());
  Suffix();
}
}  // namespace debug