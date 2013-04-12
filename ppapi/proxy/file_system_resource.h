// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FILE_SYSTEM_RESOURCE_H_
#define PPAPI_PROXY_FILE_SYSTEM_RESOURCE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/thunk/ppb_file_system_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class PPAPI_PROXY_EXPORT FileSystemResource
    : public PluginResource,
      public NON_EXPORTED_BASE(thunk::PPB_FileSystem_API) {
 public:
  FileSystemResource(Connection connection,
                     PP_Instance instance,
                     PP_FileSystemType type);
  virtual ~FileSystemResource();

  // Resource overrides.
  virtual thunk::PPB_FileSystem_API* AsPPB_FileSystem_API() OVERRIDE;

  // PPB_FileSystem_API implementation.
  virtual int32_t Open(int64_t expected_size,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_FileSystemType GetType() OVERRIDE;

 private:
  // Called when the host has responded to our open request.
  void OpenComplete(scoped_refptr<TrackedCallback> callback,
                    const ResourceMessageReplyParams& params);

  PP_FileSystemType type_;
  bool called_open_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FILE_SYSTEM_RESOURCE_H_
