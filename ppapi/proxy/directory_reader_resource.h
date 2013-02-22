// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_DIRECTORY_READER_RESOURCE_H_
#define PPAPI_PROXY_DIRECTORY_READER_RESOURCE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/ppb_file_ref_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_directory_reader_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class PPAPI_PROXY_EXPORT DirectoryReaderResource
    : public PluginResource,
      public NON_EXPORTED_BASE(thunk::PPB_DirectoryReader_API) {
 public:
  DirectoryReaderResource(Connection connection,
                          PP_Instance instance,
                          PP_Resource directory_ref);
  virtual ~DirectoryReaderResource();

  // Resource overrides.
  virtual thunk::PPB_DirectoryReader_API* AsPPB_DirectoryReader_API() OVERRIDE;

  // PPB_DirectoryReader_API.
  virtual int32_t ReadEntries(
      const PP_ArrayOutput& output,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  void OnPluginMsgGetEntriesReply(
      const PP_ArrayOutput& output,
      const ResourceMessageReplyParams& params,
      const std::vector<ppapi::PPB_FileRef_CreateInfo>& infos,
      const std::vector<PP_FileType>& file_types);

  scoped_refptr<Resource> directory_resource_;
  scoped_refptr<TrackedCallback> callback_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryReaderResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_DIRECTORY_READER_RESOURCE_H_
