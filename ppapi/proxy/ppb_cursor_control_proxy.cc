// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_cursor_control_proxy.h"

#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterFunctionNoLock;
using ppapi::thunk::PPB_CursorControl_FunctionAPI;

namespace ppapi {
namespace proxy {

PPB_CursorControl_Proxy::PPB_CursorControl_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_CursorControl_Proxy::~PPB_CursorControl_Proxy() {
}

ppapi::thunk::PPB_CursorControl_FunctionAPI*
PPB_CursorControl_Proxy::AsPPB_CursorControl_FunctionAPI() {
  return this;
}

PP_Bool PPB_CursorControl_Proxy::SetCursor(PP_Instance instance,
                                           PP_CursorType_Dev type,
                                           PP_Resource custom_image_id,
                                           const PP_Point* hot_spot) {
  // It's legal for the image ID to be null if the type is not custom.
  HostResource cursor_image_resource;
  if (type == PP_CURSORTYPE_CUSTOM) {
    Resource* cursor_image = PluginResourceTracker::GetInstance()->
        GetResource(custom_image_id);
    if (!cursor_image || cursor_image->pp_instance() != instance)
      return PP_FALSE;
    cursor_image_resource = cursor_image->host_resource();
  } else {
    if (custom_image_id)
      return PP_FALSE;  // Image specified for a predefined type.
  }

  PP_Bool result = PP_FALSE;
  PP_Point empty_point = { 0, 0 };
  dispatcher()->Send(new PpapiHostMsg_PPBCursorControl_SetCursor(
      INTERFACE_ID_PPB_CURSORCONTROL,
      instance, static_cast<int32_t>(type), cursor_image_resource,
      hot_spot ? *hot_spot : empty_point, &result));
  return result;
}

PP_Bool PPB_CursorControl_Proxy::LockCursor(PP_Instance instance) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBCursorControl_LockCursor(
      INTERFACE_ID_PPB_CURSORCONTROL, instance, &result));
  return result;
}

PP_Bool PPB_CursorControl_Proxy::UnlockCursor(PP_Instance instance) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBCursorControl_UnlockCursor(
      INTERFACE_ID_PPB_CURSORCONTROL, instance, &result));
  return result;
}

PP_Bool PPB_CursorControl_Proxy::HasCursorLock(PP_Instance instance) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBCursorControl_HasCursorLock(
      INTERFACE_ID_PPB_CURSORCONTROL, instance, &result));
  return result;
}

PP_Bool PPB_CursorControl_Proxy::CanLockCursor(PP_Instance instance) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBCursorControl_CanLockCursor(
      INTERFACE_ID_PPB_CURSORCONTROL, instance, &result));
  return result;
}

bool PPB_CursorControl_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_CursorControl_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCursorControl_SetCursor,
                        OnMsgSetCursor)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCursorControl_LockCursor,
                        OnMsgLockCursor)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCursorControl_UnlockCursor,
                        OnMsgUnlockCursor)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCursorControl_HasCursorLock,
                        OnMsgHasCursorLock)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCursorControl_CanLockCursor,
                        OnMsgCanLockCursor)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw): handle bad messages!
  return handled;
}

void PPB_CursorControl_Proxy::OnMsgSetCursor(PP_Instance instance,
                                             int32_t type,
                                             const HostResource& custom_image,
                                             const PP_Point& hot_spot,
                                             PP_Bool* result) {
  EnterFunctionNoLock<PPB_CursorControl_FunctionAPI> enter(instance, true);
  if (enter.succeeded()) {
    *result = enter.functions()->SetCursor(
        instance, static_cast<PP_CursorType_Dev>(type),
        custom_image.host_resource(), &hot_spot);
  }
}

void PPB_CursorControl_Proxy::OnMsgLockCursor(PP_Instance instance,
                                              PP_Bool* result) {
  EnterFunctionNoLock<PPB_CursorControl_FunctionAPI> enter(instance, true);
  if (enter.succeeded())
    *result = enter.functions()->LockCursor(instance);
}

void PPB_CursorControl_Proxy::OnMsgUnlockCursor(PP_Instance instance,
                                                PP_Bool* result) {
  EnterFunctionNoLock<PPB_CursorControl_FunctionAPI> enter(instance, true);
  if (enter.succeeded())
    *result = enter.functions()->UnlockCursor(instance);
}

void PPB_CursorControl_Proxy::OnMsgHasCursorLock(PP_Instance instance,
                                                 PP_Bool* result) {
  EnterFunctionNoLock<PPB_CursorControl_FunctionAPI> enter(instance, true);
  if (enter.succeeded())
    *result = enter.functions()->HasCursorLock(instance);
}

void PPB_CursorControl_Proxy::OnMsgCanLockCursor(PP_Instance instance,
                                                 PP_Bool* result) {
  EnterFunctionNoLock<PPB_CursorControl_FunctionAPI> enter(instance, true);
  if (enter.succeeded())
    *result = enter.functions()->CanLockCursor(instance);
}

}  // namespace proxy
}  // namespace ppapi
