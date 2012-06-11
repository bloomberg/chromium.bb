// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_device_id_proxy.h"

#include "base/compiler_specific.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_flash_device_id_api.h"

using ppapi::thunk::PPB_Flash_DeviceID_API;

namespace ppapi {
namespace proxy {

namespace {

class DeviceID : public Resource, public PPB_Flash_DeviceID_API {
 public:
  DeviceID(PP_Instance instance);
  virtual ~DeviceID();

  // Resource overrides.
  virtual PPB_Flash_DeviceID_API* AsPPB_Flash_DeviceID_API() OVERRIDE;

  // PPB_Flash_DeviceID_API implementation.
  virtual int32_t GetDeviceID(PP_Var* id,
                              const PP_CompletionCallback& callback) OVERRIDE;

  void OnReply(int32_t result, const std::string& id);

 private:
  // Non-null when a callback is pending.
  PP_Var* dest_;

  scoped_refptr<TrackedCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(DeviceID);
};

DeviceID::DeviceID(PP_Instance instance)
    : Resource(OBJECT_IS_PROXY, instance),
      dest_(NULL) {
}

DeviceID::~DeviceID() {
}

PPB_Flash_DeviceID_API* DeviceID::AsPPB_Flash_DeviceID_API() {
  return this;
}

int32_t DeviceID::GetDeviceID(PP_Var* id,
                              const PP_CompletionCallback& callback) {
  if (TrackedCallback::IsPending(callback_))
    return PP_ERROR_INPROGRESS;
  if (!id)
    return PP_ERROR_BADARGUMENT;

  callback_ = new TrackedCallback(this, callback);
  dest_ = id;

  PluginDispatcher* dispatcher =
      PluginDispatcher::GetForInstance(pp_instance());

  PluginGlobals::Get()->plugin_proxy_delegate()->SendToBrowser(
      new PpapiHostMsg_PPBFlashDeviceID_Get(
          API_ID_PPB_FLASH_DEVICE_ID, dispatcher->plugin_dispatcher_id(),
          pp_resource()));
  return PP_OK_COMPLETIONPENDING;
}

void DeviceID::OnReply(int32_t result, const std::string& id) {
  if (result == PP_OK)
    *dest_ = StringVar::StringToPPVar(id);
  else
    *dest_ = PP_MakeUndefined();
  dest_ = NULL;
  TrackedCallback::ClearAndRun(&callback_, result);
}

}  // namespace

PPB_Flash_DeviceID_Proxy::PPB_Flash_DeviceID_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Flash_DeviceID_Proxy::~PPB_Flash_DeviceID_Proxy() {
}

// static
PP_Resource PPB_Flash_DeviceID_Proxy::CreateProxyResource(
    PP_Instance instance) {
  return (new DeviceID(instance))->GetReference();
}

bool PPB_Flash_DeviceID_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_DeviceID_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFlashDeviceID_GetReply,
                        OnPluginMsgGetReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Flash_DeviceID_Proxy::OnPluginMsgGetReply(int32 routing_id,
                                                   PP_Resource resource,
                                                   int32 result,
                                                   const std::string& id) {
  thunk::EnterResourceNoLock<PPB_Flash_DeviceID_API> enter(resource, false);
  if (enter.failed())
    return;  // Resource destroyed.
  static_cast<DeviceID*>(enter.object())->OnReply(result, id);
}

}  // namespace proxy
}  // namespace ppapi
