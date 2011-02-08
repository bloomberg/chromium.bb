// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_core_proxy.h"

#include <stdlib.h>  // For malloc

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

base::MessageLoopProxy* GetMainThreadMessageLoop() {
  static scoped_refptr<base::MessageLoopProxy> proxy(
      base::MessageLoopProxy::CreateForCurrentThread());
  return proxy.get();
}

void AddRefResource(PP_Resource resource) {
  PluginResourceTracker::GetInstance()->AddRefResource(resource);
}

void ReleaseResource(PP_Resource resource) {
  PluginResourceTracker::GetInstance()->ReleaseResource(resource);
}

void* MemAlloc(size_t num_bytes) {
  return malloc(num_bytes);
}

void MemFree(void* ptr) {
  free(ptr);
}

double GetTime() {
  return base::Time::Now().ToDoubleT();
}

double GetTimeTicks() {
  // TODO(brettw) http://code.google.com/p/chromium/issues/detail?id=57448
  // This should be a tick timer rather than wall clock time, but needs to
  // match message times, which also currently use wall clock time.
  return GetTime();
}

void CallOnMainThread(int delay_in_ms,
                      PP_CompletionCallback callback,
                      int32_t result) {
  GetMainThreadMessageLoop()->PostDelayedTask(
      FROM_HERE,
      NewRunnableFunction(callback.func, callback.user_data, result),
      delay_in_ms);
}

PP_Bool IsMainThread() {
  return BoolToPPBool(GetMainThreadMessageLoop()->BelongsToCurrentThread());
}

const PPB_Core core_interface = {
  &AddRefResource,
  &ReleaseResource,
  &MemAlloc,
  &MemFree,
  &GetTime,
  &GetTimeTicks,
  &CallOnMainThread,
  &IsMainThread
};

InterfaceProxy* CreateCoreProxy(Dispatcher* dispatcher,
                                const void* target_interface) {
  return new PPB_Core_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Core_Proxy::PPB_Core_Proxy(Dispatcher* dispatcher,
                               const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Core_Proxy::~PPB_Core_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Core_Proxy::GetInfo() {
  static const Info info = {
    &core_interface,
    PPB_CORE_INTERFACE,
    INTERFACE_ID_PPB_CORE,
    false,
    &CreateCoreProxy,
  };
  return &info;
}

bool PPB_Core_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Core_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCore_AddRefResource,
                        OnMsgAddRefResource)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCore_ReleaseResource,
                        OnMsgReleaseResource)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Core_Proxy::OnMsgAddRefResource(HostResource resource) {
  ppb_core_target()->AddRefResource(resource.host_resource());
}

void PPB_Core_Proxy::OnMsgReleaseResource(HostResource resource) {
  ppb_core_target()->ReleaseResource(resource.host_resource());
}

}  // namespace proxy
}  // namespace pp
