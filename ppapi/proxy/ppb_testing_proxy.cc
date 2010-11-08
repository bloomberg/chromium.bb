// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_testing_proxy.h"

#include "base/message_loop.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

PP_Bool ReadImageData(PP_Resource device_context_2d,
                      PP_Resource image,
                      const PP_Point* top_left) {
  PP_Bool result = PP_FALSE;
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBTesting_ReadImageData(
      INTERFACE_ID_PPB_TESTING, device_context_2d, image, *top_left, &result));
  return result;
}

void RunMessageLoop() {
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void QuitMessageLoop() {
  MessageLoop::current()->QuitNow();
}

uint32_t GetLiveObjectCount(PP_Module module_id) {
  uint32_t result = 0;
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBTesting_GetLiveObjectCount(
      INTERFACE_ID_PPB_TESTING, module_id, &result));
  return result;
}

const PPB_Testing_Dev testing_interface = {
  &ReadImageData,
  &RunMessageLoop,
  &QuitMessageLoop,
  &GetLiveObjectCount
};

}  // namespace

PPB_Testing_Proxy::PPB_Testing_Proxy(Dispatcher* dispatcher,
                                     const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Testing_Proxy::~PPB_Testing_Proxy() {
}

const void* PPB_Testing_Proxy::GetSourceInterface() const {
  return &testing_interface;
}

InterfaceID PPB_Testing_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_TESTING;
}

void PPB_Testing_Proxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PPB_Testing_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTesting_ReadImageData,
                        OnMsgReadImageData)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTesting_RunMessageLoop,
                        OnMsgRunMessageLoop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTesting_QuitMessageLoop,
                        OnMsgQuitMessageLoop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTesting_GetLiveObjectCount,
                        OnMsgGetLiveObjectCount)
  IPC_END_MESSAGE_MAP()
}

void PPB_Testing_Proxy::OnMsgReadImageData(PP_Resource device_context_2d,
                                           PP_Resource image,
                                           const PP_Point& top_left,
                                           PP_Bool* result) {
  *result = ppb_testing_target()->ReadImageData(
      device_context_2d, image, &top_left);
}

void PPB_Testing_Proxy::OnMsgRunMessageLoop(bool* dummy) {
  ppb_testing_target()->RunMessageLoop();
  *dummy = false;
}

void PPB_Testing_Proxy::OnMsgQuitMessageLoop() {
  ppb_testing_target()->QuitMessageLoop();
}

void PPB_Testing_Proxy::OnMsgGetLiveObjectCount(PP_Module module_id,
                                                uint32_t* result) {
  *result = ppb_testing_target()->GetLiveObjectCount(module_id);
}

}  // namespace proxy
}  // namespace pp
