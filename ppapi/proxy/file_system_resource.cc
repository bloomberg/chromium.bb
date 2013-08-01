// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/file_system_resource.h"

#include "base/bind.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/tracked_callback.h"

using ppapi::thunk::PPB_FileSystem_API;

namespace ppapi {
namespace proxy {

FileSystemResource::FileSystemResource(Connection connection,
                                       PP_Instance instance,
                                       PP_FileSystemType type)
    : PluginResource(connection, instance),
      type_(type),
      called_open_(false),
      callback_count_(0) {
  DCHECK(type_ != PP_FILESYSTEMTYPE_INVALID);
  // TODO(teravest): Temporarily create hosts in both the browser and renderer
  // while we move file related hosts to the browser.
  SendCreate(RENDERER, PpapiHostMsg_FileSystem_Create(type_));
  SendCreate(BROWSER, PpapiHostMsg_FileSystem_Create(type_));
}

FileSystemResource::~FileSystemResource() {
}

PPB_FileSystem_API* FileSystemResource::AsPPB_FileSystem_API() {
  return this;
}

int32_t FileSystemResource::Open(int64_t expected_size,
                                 scoped_refptr<TrackedCallback> callback) {
  if (called_open_)
    return PP_ERROR_FAILED;
  called_open_ = true;

  Call<PpapiPluginMsg_FileSystem_OpenReply>(RENDERER,
      PpapiHostMsg_FileSystem_Open(expected_size),
      base::Bind(&FileSystemResource::OpenComplete,
                 this,
                 callback));
  Call<PpapiPluginMsg_FileSystem_OpenReply>(BROWSER,
      PpapiHostMsg_FileSystem_Open(expected_size),
      base::Bind(&FileSystemResource::OpenComplete,
                 this,
                 callback));
  return PP_OK_COMPLETIONPENDING;
}

PP_FileSystemType FileSystemResource::GetType() {
  return type_;
}

void FileSystemResource::InitIsolatedFileSystem(const char* fsid) {
  Post(RENDERER,
       PpapiHostMsg_FileSystem_InitIsolatedFileSystem(std::string(fsid)));
  Post(BROWSER,
       PpapiHostMsg_FileSystem_InitIsolatedFileSystem(std::string(fsid)));
}

void FileSystemResource::OpenComplete(
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params) {
  ++callback_count_;
  // Received callback from browser and renderer.
  if (callback_count_ == 2)
    callback->Run(params.result());
}

}  // namespace proxy
}  // namespace ppapi
