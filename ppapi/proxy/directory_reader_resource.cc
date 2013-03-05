// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/directory_reader_resource.h"

#include "base/bind.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_file_ref_proxy.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"

using ppapi::proxy::PPB_FileRef_Proxy;

namespace ppapi {
namespace proxy {

namespace {

void ReleaseEntries(const std::vector<PP_DirectoryEntry_Dev>& entries) {
  ResourceTracker* tracker = PpapiGlobals::Get()->GetResourceTracker();
  for (std::vector<PP_DirectoryEntry_Dev>::const_iterator it = entries.begin();
       it != entries.end(); ++it)
    tracker->ReleaseResource(it->file_ref);
}

}  // namespace

DirectoryReaderResource::DirectoryReaderResource(
    Connection connection,
    PP_Instance instance,
    PP_Resource directory_ref)
    : PluginResource(connection, instance) {
  directory_resource_ =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(directory_ref);
}

DirectoryReaderResource::~DirectoryReaderResource() {
}

thunk::PPB_DirectoryReader_API*
DirectoryReaderResource::AsPPB_DirectoryReader_API() {
  return this;
}

int32_t DirectoryReaderResource::ReadEntries(
    const PP_ArrayOutput& output,
    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(callback_))
    return PP_ERROR_INPROGRESS;

  callback_ = callback;

  if (!sent_create_to_renderer())
    SendCreate(RENDERER, PpapiHostMsg_DirectoryReader_Create());

  PpapiHostMsg_DirectoryReader_GetEntries msg(
      directory_resource_->host_resource());
  Call<PpapiPluginMsg_DirectoryReader_GetEntriesReply>(
      RENDERER, msg,
      base::Bind(&DirectoryReaderResource::OnPluginMsgGetEntriesReply,
                 this, output));
  return PP_OK_COMPLETIONPENDING;
}

void DirectoryReaderResource::OnPluginMsgGetEntriesReply(
    const PP_ArrayOutput& output,
    const ResourceMessageReplyParams& params,
    const std::vector<ppapi::PPB_FileRef_CreateInfo>& infos,
    const std::vector<PP_FileType>& file_types) {
  CHECK_EQ(infos.size(), file_types.size());

  std::vector<PP_DirectoryEntry_Dev> entries;
  for (std::vector<ppapi::PPB_FileRef_CreateInfo>::size_type i = 0;
       i < infos.size(); ++i) {
    PP_DirectoryEntry_Dev entry;
    entry.file_ref = PPB_FileRef_Proxy::DeserializeFileRef(infos[i]);
    entry.file_type = file_types[i];
    entries.push_back(entry);
  }

  if (!TrackedCallback::IsPending(callback_)) {
    ReleaseEntries(entries);
    entries.clear();
    return;
  }

  ArrayWriter writer(output);
  if (!writer.is_valid()) {
    ReleaseEntries(entries);
    entries.clear();
    callback_->Run(PP_ERROR_FAILED);
    return;
  }

  writer.StoreVector(entries);
  entries.clear();
  callback_->Run(params.result());
}

}  // namespace proxy
}  // namespace ppapi
