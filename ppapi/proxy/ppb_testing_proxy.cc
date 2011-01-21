// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_testing_proxy.h"

#include "base/message_loop.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/proxy/image_data.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

PP_Bool ReadImageData(PP_Resource device_context_2d,
                      PP_Resource image,
                      const PP_Point* top_left) {
  ImageData* image_object = PluginResource::GetAs<ImageData>(image);
  if (!image_object)
    return PP_FALSE;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      image_object->instance());
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBTesting_ReadImageData(
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

uint32_t GetLiveObjectsForInstance(PP_Instance instance_id) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return static_cast<uint32_t>(-1);

  uint32_t result = 0;
  dispatcher->Send(new PpapiHostMsg_PPBTesting_GetLiveObjectsForInstance(
      INTERFACE_ID_PPB_TESTING, instance_id, &result));
  return result;
}

const PPB_Testing_Dev testing_interface = {
  &ReadImageData,
  &RunMessageLoop,
  &QuitMessageLoop,
  &GetLiveObjectsForInstance
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

bool PPB_Testing_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Testing_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTesting_ReadImageData,
                        OnMsgReadImageData)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTesting_RunMessageLoop,
                        OnMsgRunMessageLoop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTesting_QuitMessageLoop,
                        OnMsgQuitMessageLoop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBTesting_GetLiveObjectsForInstance,
                        OnMsgGetLiveObjectsForInstance)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
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

void PPB_Testing_Proxy::OnMsgGetLiveObjectsForInstance(PP_Instance instance,
                                                uint32_t* result) {
  *result = ppb_testing_target()->GetLiveObjectsForInstance(instance);
}

}  // namespace proxy
}  // namespace pp
