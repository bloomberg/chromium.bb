// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/flash_drm_resource.h"

#include "base/bind.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

FlashDRMResource::FlashDRMResource(Connection connection,
                                   PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(BROWSER, PpapiHostMsg_FlashDRM_Create());
}

FlashDRMResource::~FlashDRMResource() {
}

thunk::PPB_Flash_DRM_API* FlashDRMResource::AsPPB_Flash_DRM_API() {
  return this;
}

int32_t FlashDRMResource::GetDeviceID(PP_Var* id,
                                      scoped_refptr<TrackedCallback> callback) {
  if (!id)
    return PP_ERROR_BADARGUMENT;

  *id = PP_MakeUndefined();

  Call<PpapiPluginMsg_FlashDRM_GetDeviceIDReply>(
      BROWSER,
      PpapiHostMsg_FlashDRM_GetDeviceID(),
      base::Bind(&FlashDRMResource::OnPluginMsgGetDeviceIDReply, this,
      id, callback));
  return PP_OK_COMPLETIONPENDING;
}

void FlashDRMResource::OnPluginMsgGetDeviceIDReply(
    PP_Var* dest,
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params,
    const std::string& id) {
  if (TrackedCallback::IsPending(callback)) {
    if (params.result() == PP_OK)
      *dest = StringVar::StringToPPVar(id);
    callback->Run(params.result());
  }
}

}  // namespace proxy
}  // namespace ppapi
