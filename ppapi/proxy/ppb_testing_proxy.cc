// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_testing_proxy.h"

#include "base/message_loop.h"
#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

PP_Bool ReadImageData(PP_Resource graphics_2d,
                      PP_Resource image,
                      const PP_Point* top_left) {
  PluginResource* image_object = PluginResourceTracker::GetInstance()->
      GetResourceObject(image);
  if (!image_object)
    return PP_FALSE;
  PluginResource* graphics_2d_object =
      PluginResourceTracker::GetInstance()->GetResourceObject(graphics_2d);
  if (!graphics_2d_object ||
      image_object->instance() != graphics_2d_object->instance())
    return PP_FALSE;

  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      image_object->instance());
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBTesting_ReadImageData(
      INTERFACE_ID_PPB_TESTING, graphics_2d_object->host_resource(),
      image_object->host_resource(), *top_left, &result));
  return result;
}

void RunMessageLoop(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  IPC::SyncMessage* msg = new PpapiHostMsg_PPBTesting_RunMessageLoop(
        INTERFACE_ID_PPB_TESTING, instance);
  msg->EnableMessagePumping();
  dispatcher->Send(msg);
}

void QuitMessageLoop(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  dispatcher->Send(new PpapiHostMsg_PPBTesting_QuitMessageLoop(
        INTERFACE_ID_PPB_TESTING, instance));
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

InterfaceProxy* CreateTestingProxy(Dispatcher* dispatcher,
                                   const void* target_interface) {
  return new PPB_Testing_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Testing_Proxy::PPB_Testing_Proxy(Dispatcher* dispatcher,
                                     const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Testing_Proxy::~PPB_Testing_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Testing_Proxy::GetInfo() {
  static const Info info = {
    &testing_interface,
    PPB_TESTING_DEV_INTERFACE,
    INTERFACE_ID_PPB_TESTING,
    false,
    &CreateTestingProxy,
  };
  return &info;
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

void PPB_Testing_Proxy::OnMsgReadImageData(
    const HostResource& device_context_2d,
    const HostResource& image,
    const PP_Point& top_left,
    PP_Bool* result) {
  *result = ppb_testing_target()->ReadImageData(
      device_context_2d.host_resource(), image.host_resource(), &top_left);
}

void PPB_Testing_Proxy::OnMsgRunMessageLoop(PP_Instance instance) {
  ppb_testing_target()->RunMessageLoop(instance);
}

void PPB_Testing_Proxy::OnMsgQuitMessageLoop(PP_Instance instance) {
  ppb_testing_target()->QuitMessageLoop(instance);
}

void PPB_Testing_Proxy::OnMsgGetLiveObjectsForInstance(PP_Instance instance,
                                                       uint32_t* result) {
  *result = ppb_testing_target()->GetLiveObjectsForInstance(instance);
}

}  // namespace proxy
}  // namespace pp
