// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/talk_resource.h"

#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {
namespace proxy {

TalkResource::TalkResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance) {
}

TalkResource::~TalkResource() {
}

thunk::PPB_Talk_Private_API* TalkResource::AsPPB_Talk_Private_API() {
  return this;
}

int32_t TalkResource::GetPermission(scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(callback_))
    return PP_ERROR_INPROGRESS;
  callback_ = callback;

  if (!sent_create_to_browser())
    SendCreate(BROWSER, PpapiHostMsg_Talk_Create());

  Call<PpapiPluginMsg_Talk_GetPermissionReply>(
      BROWSER,
      PpapiHostMsg_Talk_GetPermission(),
      base::Bind(&TalkResource::GetPermissionReply, base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

void TalkResource::GetPermissionReply(
    const ResourceMessageReplyParams& params) {
  callback_->Run(params.result());
}

}  // namespace proxy
}  // namespace ppapi
