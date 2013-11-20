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
      callback_count_(0),
      callback_result_(PP_OK) {
  DCHECK(type_ != PP_FILESYSTEMTYPE_INVALID);
  SendCreate(RENDERER, PpapiHostMsg_FileSystem_Create(type_));
  SendCreate(BROWSER, PpapiHostMsg_FileSystem_Create(type_));
}

FileSystemResource::FileSystemResource(Connection connection,
                                       PP_Instance instance,
                                       int pending_renderer_id,
                                       int pending_browser_id,
                                       PP_FileSystemType type)
    : PluginResource(connection, instance),
      type_(type),
      called_open_(true),
      callback_count_(0),
      callback_result_(PP_OK) {
  DCHECK(type_ != PP_FILESYSTEMTYPE_INVALID);
  AttachToPendingHost(RENDERER, pending_renderer_id);
  AttachToPendingHost(BROWSER, pending_browser_id);
}

FileSystemResource::~FileSystemResource() {
}

PPB_FileSystem_API* FileSystemResource::AsPPB_FileSystem_API() {
  return this;
}

int32_t FileSystemResource::Open(int64_t expected_size,
                                 scoped_refptr<TrackedCallback> callback) {
  DCHECK(type_ != PP_FILESYSTEMTYPE_ISOLATED);
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

int32_t FileSystemResource::InitIsolatedFileSystem(
    const std::string& fsid,
    PP_IsolatedFileSystemType_Private type,
    const base::Callback<void(int32_t)>& callback) {
  // This call is mutually exclusive with Open() above, so we can reuse the
  // called_open state.
  DCHECK(type_ == PP_FILESYSTEMTYPE_ISOLATED);
  if (called_open_)
    return PP_ERROR_FAILED;
  called_open_ = true;

  Call<PpapiPluginMsg_FileSystem_InitIsolatedFileSystemReply>(RENDERER,
      PpapiHostMsg_FileSystem_InitIsolatedFileSystem(fsid, type),
      base::Bind(&FileSystemResource::InitIsolatedFileSystemComplete,
      this,
      callback));
  Call<PpapiPluginMsg_FileSystem_InitIsolatedFileSystemReply>(BROWSER,
      PpapiHostMsg_FileSystem_InitIsolatedFileSystem(fsid, type),
      base::Bind(&FileSystemResource::InitIsolatedFileSystemComplete,
      this,
      callback));
  return PP_OK_COMPLETIONPENDING;
}

void FileSystemResource::OpenComplete(
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params) {
  ++callback_count_;
  // Prioritize worse result since only one status can be returned.
  if (params.result() != PP_OK)
    callback_result_ = params.result();
  // Received callback from browser and renderer.
  if (callback_count_ == 2)
    callback->Run(callback_result_);
}

void FileSystemResource::InitIsolatedFileSystemComplete(
    const base::Callback<void(int32_t)>& callback,
    const ResourceMessageReplyParams& params) {
  ++callback_count_;
  // Prioritize worse result since only one status can be returned.
  if (params.result() != PP_OK)
    callback_result_ = params.result();
  // Received callback from browser and renderer.
  if (callback_count_ == 2)
    callback.Run(callback_result_);
}

}  // namespace proxy
}  // namespace ppapi
