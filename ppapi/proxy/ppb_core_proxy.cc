// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_core_proxy.h"

#include <stdlib.h>  // For malloc

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

void AddRefResource(PP_Resource resource) {
  PluginDispatcher::Get()->plugin_resource_tracker()->AddRefResource(resource);
}

void ReleaseResource(PP_Resource resource) {
  PluginDispatcher::Get()->plugin_resource_tracker()->ReleaseResource(resource);
}

void* MemAlloc(size_t num_bytes) {
  return malloc(num_bytes);
}

void MemFree(void* ptr) {
  free(ptr);
}

double GetTime() {
  double result;
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBCore_GetTime(
      INTERFACE_ID_PPB_CORE, &result));
  return result;
}

double GetTimeTicks() {
  double result;
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBCore_GetTimeTicks(
      INTERFACE_ID_PPB_CORE, &result));
  return result;
}

void CallOnMainThread(int delay_in_msec,
                      PP_CompletionCallback callback,
                      int32_t result) {
  Dispatcher* dispatcher = PluginDispatcher::Get();
  dispatcher->Send(new PpapiHostMsg_PPBCore_CallOnMainThread(
      INTERFACE_ID_PPB_CORE, delay_in_msec,
      dispatcher->callback_tracker().SendCallback(callback),
      result));
}

bool IsMainThread() {
  NOTIMPLEMENTED();
  return true;
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

}  // namespace

PPB_Core_Proxy::PPB_Core_Proxy(Dispatcher* dispatcher,
                               const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Core_Proxy::~PPB_Core_Proxy() {
}

const void* PPB_Core_Proxy::GetSourceInterface() const {
  return &core_interface;
}

InterfaceID PPB_Core_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_CORE;
}

void PPB_Core_Proxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PPB_Core_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCore_AddRefResource, OnMsgAddRefResource)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCore_ReleaseResource, OnMsgReleaseResource)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCore_GetTime, OnMsgGetTime)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCore_CallOnMainThread,
                        OnMsgCallOnMainThread)
  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
}

void PPB_Core_Proxy::OnMsgAddRefResource(PP_Resource resource) {
  ppb_core_target()->AddRefResource(resource);
}

void PPB_Core_Proxy::OnMsgReleaseResource(PP_Resource resource) {
  ppb_core_target()->ReleaseResource(resource);
}

void PPB_Core_Proxy::OnMsgGetTime(double* result) {
  *result = 0.0;  // FIXME(brettw);
}

void PPB_Core_Proxy::OnMsgCallOnMainThread(int delay_in_msec,
                                           uint32_t serialized_callback,
                                           int32_t result) {
  // TODO(brettw) this doesn't need to go to the other process, we should be
  // able to post this to our own message loop and get better performance.
  ppb_core_target()->CallOnMainThread(delay_in_msec,
                                      ReceiveCallback(serialized_callback),
                                      result);
}

}  // namespace proxy
}  // namespace pp
