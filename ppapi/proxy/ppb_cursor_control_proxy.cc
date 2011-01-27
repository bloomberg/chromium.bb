// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_cursor_control_proxy.h"

#include "ppapi/c/dev/pp_cursor_type_dev.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

PP_Bool SetCursor(PP_Instance instance_id,
                  PP_CursorType_Dev type,
                  PP_Resource custom_image_id,
                  const PP_Point* hot_spot) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return PP_FALSE;

  // It's legal for the image ID to be null if the type is not custom.
  HostResource cursor_image_resource;
  if (type == PP_CURSORTYPE_CUSTOM) {
    PluginResource* cursor_image = PluginResourceTracker::GetInstance()->
        GetResourceObject(custom_image_id);
    if (!cursor_image || cursor_image->instance() != instance_id)
      return PP_FALSE;
    cursor_image_resource = cursor_image->host_resource();
  } else {
    if (custom_image_id)
      return PP_FALSE;  // Image specified for a predefined type.
  }

  PP_Bool result = PP_FALSE;
  PP_Point empty_point = { 0, 0 };
  dispatcher->Send(new PpapiHostMsg_PPBCursorControl_SetCursor(
      INTERFACE_ID_PPB_CURSORCONTROL,
      instance_id, static_cast<int32_t>(type), cursor_image_resource,
      hot_spot ? *hot_spot : empty_point, &result));
  return result;
}

PP_Bool LockCursor(PP_Instance instance_id) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBCursorControl_LockCursor(
      INTERFACE_ID_PPB_CURSORCONTROL, instance_id, &result));
  return result;
}

PP_Bool UnlockCursor(PP_Instance instance_id) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBCursorControl_UnlockCursor(
      INTERFACE_ID_PPB_CURSORCONTROL, instance_id, &result));
  return result;
}

PP_Bool HasCursorLock(PP_Instance instance_id) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBCursorControl_HasCursorLock(
      INTERFACE_ID_PPB_CURSORCONTROL, instance_id, &result));
  return result;
}

PP_Bool CanLockCursor(PP_Instance instance_id) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance_id);
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBCursorControl_CanLockCursor(
      INTERFACE_ID_PPB_CURSORCONTROL, instance_id, &result));
  return result;
}

const PPB_CursorControl_Dev cursor_control_interface = {
  &SetCursor,
  &LockCursor,
  &UnlockCursor,
  &HasCursorLock,
  &CanLockCursor
};

}  // namespace

PPB_CursorControl_Proxy::PPB_CursorControl_Proxy(Dispatcher* dispatcher,
                               const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_CursorControl_Proxy::~PPB_CursorControl_Proxy() {
}

const void* PPB_CursorControl_Proxy::GetSourceInterface() const {
  return &cursor_control_interface;
}

InterfaceID PPB_CursorControl_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_CURSORCONTROL;
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
                                             HostResource custom_image,
                                             const PP_Point& hot_spot,
                                             PP_Bool* result) {
  *result = ppb_cursor_control_target()->SetCursor(
      instance, static_cast<PP_CursorType_Dev>(type),
      custom_image.host_resource(), &hot_spot);
}

void PPB_CursorControl_Proxy::OnMsgLockCursor(PP_Instance instance,
                                              PP_Bool* result) {
  *result = ppb_cursor_control_target()->LockCursor(instance);
}

void PPB_CursorControl_Proxy::OnMsgUnlockCursor(PP_Instance instance,
                                                PP_Bool* result) {
  *result = ppb_cursor_control_target()->UnlockCursor(instance);
}

void PPB_CursorControl_Proxy::OnMsgHasCursorLock(PP_Instance instance,
                                                 PP_Bool* result) {
  *result = ppb_cursor_control_target()->HasCursorLock(instance);
}

void PPB_CursorControl_Proxy::OnMsgCanLockCursor(PP_Instance instance,
                                                 PP_Bool* result) {
  *result = ppb_cursor_control_target()->CanLockCursor(instance);
}

}  // namespace proxy
}  // namespace pp
