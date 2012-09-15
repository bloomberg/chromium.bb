// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/flash_device_id_resource.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

FlashDeviceIDResource::FlashDeviceIDResource(Connection connection,
                                             PP_Instance instance)
    : PluginResource(connection, instance),
      dest_(NULL) {
  SendCreateToBrowser(PpapiHostMsg_FlashDeviceID_Create());
}

FlashDeviceIDResource::~FlashDeviceIDResource() {
}

thunk::PPB_Flash_DeviceID_API*
FlashDeviceIDResource::AsPPB_Flash_DeviceID_API() {
  return this;
}

int32_t FlashDeviceIDResource::GetDeviceID(
    PP_Var* id,
    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(callback_))
    return PP_ERROR_INPROGRESS;
  if (!id)
    return PP_ERROR_BADARGUMENT;

  dest_ = id;
  callback_ = callback;

  CallBrowser(PpapiHostMsg_FlashDeviceID_GetDeviceID());
  return PP_OK_COMPLETIONPENDING;
}

void FlashDeviceIDResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(FlashDeviceIDResource, msg)
    PPAPI_DISPATCH_RESOURCE_REPLY(
        PpapiPluginMsg_FlashDeviceID_GetDeviceIDReply,
        OnPluginMsgGetDeviceIDReply)
  IPC_END_MESSAGE_MAP()
}

void FlashDeviceIDResource::OnPluginMsgGetDeviceIDReply(
    const ResourceMessageReplyParams& params,
    const std::string& id) {
  if (params.result() == PP_OK)
    *dest_ = StringVar::StringToPPVar(id);
  else
    *dest_ = PP_MakeUndefined();
  dest_ = NULL;
  TrackedCallback::ClearAndRun(&callback_, params.result());
}

}  // namespace proxy
}  // namespace ppapi
