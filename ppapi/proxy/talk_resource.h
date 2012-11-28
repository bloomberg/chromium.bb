// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_TALK_RESOURCE_H_
#define PPAPI_PROXY_TALK_RESOURCE_H_

#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_talk_private_api.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT TalkResource
    : public PluginResource,
      public NON_EXPORTED_BASE(thunk::PPB_Talk_Private_API) {
 public:
  TalkResource(Connection connection, PP_Instance instance);
  virtual ~TalkResource();

  // Resource overrides.
  thunk::PPB_Talk_Private_API* AsPPB_Talk_Private_API();

  // PPB_Talk_API implementation.
  virtual int32_t GetPermission(
      scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  void GetPermissionReply(const ResourceMessageReplyParams& params);

  scoped_refptr<TrackedCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(TalkResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_TALK_RESOURCE_H_
