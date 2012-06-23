// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_talk_private_proxy.h"

#include "ipc/ipc_message_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_talk_private_api.h"

namespace ppapi {
namespace proxy {

namespace {

class Talk : public Resource, public thunk::PPB_Talk_Private_API {
 public:
  Talk(PP_Instance instance) : Resource(OBJECT_IS_PROXY, instance) {
  }

  // Resource overrides.
  thunk::PPB_Talk_Private_API* AsPPB_Talk_Private_API() { return this; }

  // PPB_Talk_API implementation.
  int32_t GetPermission(scoped_refptr<TrackedCallback> callback) {
    if (TrackedCallback::IsPending(callback_))
      return PP_ERROR_INPROGRESS;
    PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
        pp_instance());
    if (!dispatcher)
      return PP_ERROR_FAILED;

    callback_ = callback;

    if (PluginGlobals::Get()->plugin_proxy_delegate()->SendToBrowser(
        new PpapiHostMsg_PPBTalk_GetPermission(
            API_ID_PPB_TALK,
            dispatcher->plugin_dispatcher_id(),
            pp_resource())))
      return PP_OK_COMPLETIONPENDING;
    return PP_ERROR_FAILED;
  }

  void GotCompletion(int32_t result) {
    TrackedCallback::ClearAndRun(&callback_, result);
  }

 private:
  scoped_refptr<TrackedCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(Talk);
};

}  // namespace

PPB_Talk_Private_Proxy::PPB_Talk_Private_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

// static
PP_Resource PPB_Talk_Private_Proxy::CreateProxyResource(PP_Instance instance) {
  return (new Talk(instance))->GetReference();
}

bool PPB_Talk_Private_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Talk_Private_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBTalk_GetPermissionACK,
                        OnMsgGetPermissionACK)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP();
  return handled;
}

void PPB_Talk_Private_Proxy::OnMsgGetPermissionACK(uint32 /* dispatcher_id */,
                                                   PP_Resource resource,
                                                   int32_t result) {
  thunk::EnterResourceNoLock<thunk::PPB_Talk_Private_API> enter(
      resource, false);
  if (enter.succeeded())
    static_cast<Talk*>(enter.object())->GotCompletion(result);
}

}  // namespace proxy
}  // namespace ppapi
