// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_EXTENSIONS_COMMON_RESOURCE_H_
#define PPAPI_PROXY_EXTENSIONS_COMMON_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/thunk/extensions_common_api.h"

namespace base {
class ListValue;
}

namespace ppapi {
namespace proxy {

class ResourceMessageReplyParams;

class ExtensionsCommonResource : public PluginResource,
                                 public thunk::ExtensionsCommon_API {
 public:
  ExtensionsCommonResource(Connection connection, PP_Instance instance);
  virtual ~ExtensionsCommonResource();

  // Resource overrides.
  virtual thunk::ExtensionsCommon_API* AsExtensionsCommon_API() OVERRIDE;

  // ExtensionsCommon_API implementation.
  virtual int32_t Call(const std::string& request_name,
                       const std::vector<PP_Var>& input_args,
                       const std::vector<PP_Var*>& output_args,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void Post(const std::string& request_name,
                    const std::vector<PP_Var>& args) OVERRIDE;

 private:
  void OnPluginMsgCallReply(const std::vector<PP_Var*>& output_args,
                            scoped_refptr<TrackedCallback> callback,
                            const ResourceMessageReplyParams& params,
                            const base::ListValue& output);

  DISALLOW_COPY_AND_ASSIGN(ExtensionsCommonResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_EXTENSIONS_COMMON_RESOURCE_H_
