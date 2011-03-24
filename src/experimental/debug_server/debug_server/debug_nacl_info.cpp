#include "StdAfx.h"
#include "debug_nacl_info.h"
#include "debug_core.h"

//#include "c:\work\chromuim_648_12\src\native_client\src\trusted\service_runtime\nacl_app_thread.h"
//#include "c:\work\chromuim_648_12\src\native_client\src\trusted\service_runtime\sel_ldr.h"

#include "native_client\src\trusted\service_runtime\nacl_app_thread.h"
#include "native_client\src\trusted\service_runtime\sel_ldr.h"

namespace debug {
void NaclInfo::Init(DebuggeeProcess* process, void* thread_inf) {
  process_ = process;
  thread_inf_ = thread_inf;
}

void* NaclInfo::GetMemBase() {
  if (!process_)
    return 0;

  NaClAppThread app_thread;
  memset(&app_thread, 0, sizeof(app_thread));
  if (!process_->ReadMemory(thread_inf_, sizeof(app_thread), &app_thread))
    return 0;

  NaClApp* nap = app_thread.nap;
  NaClApp app;
  memset(&app, 0, sizeof(app));
  if (!process_->ReadMemory(nap, sizeof(app), &app))
    return 0;

  return (void*)app.mem_start;
}

void* NaclInfo::GetEntryPoint() {
  if (!process_)
    return 0;

  NaClAppThread app_thread;
  memset(&app_thread, 0, sizeof(app_thread));
  if (!process_->ReadMemory(thread_inf_, sizeof(app_thread), &app_thread))
    return 0;

  NaClApp* nap = app_thread.nap;
  NaClApp app;
  memset(&app, 0, sizeof(app));
  if (!process_->ReadMemory(nap, sizeof(app), &app))
    return 0;

  return (void*)app.entry_pt;
}

}  // namespace debug