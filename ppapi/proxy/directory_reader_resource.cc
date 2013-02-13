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

DirectoryReaderResource::DirectoryReaderResource(
    Connection connection,
    PP_Instance instance,
    PP_Resource directory_ref)
    : PluginResource(connection, instance),
      output_(NULL),
      did_receive_get_entries_reply_(false) {
  directory_resource_ =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(directory_ref);
}

DirectoryReaderResource::~DirectoryReaderResource() {
}

thunk::PPB_DirectoryReader_API*
DirectoryReaderResource::AsPPB_DirectoryReader_API() {
  return this;
}

int32_t DirectoryReaderResource::GetNextEntry(
    PP_DirectoryEntry_Dev* entry,
    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(callback_))
    return PP_ERROR_INPROGRESS;

  output_ = entry;
  callback_ = callback;

  if (FillUpEntry())
    return PP_OK;

  if (!sent_create_to_renderer())
    SendCreate(RENDERER, PpapiHostMsg_DirectoryReader_Create());

  PpapiHostMsg_DirectoryReader_GetEntries msg(
      directory_resource_->host_resource());
  Call<PpapiPluginMsg_DirectoryReader_GetEntriesReply>(
      RENDERER, msg,
      base::Bind(&DirectoryReaderResource::OnPluginMsgGetEntriesReply, this));
  return PP_OK_COMPLETIONPENDING;
}

void DirectoryReaderResource::OnPluginMsgGetEntriesReply(
    const ResourceMessageReplyParams& params,
    const std::vector<ppapi::PPB_FileRef_CreateInfo>& infos,
    const std::vector<PP_FileType>& file_types) {
  CHECK_EQ(infos.size(), file_types.size());
  did_receive_get_entries_reply_ = true;

  for (std::vector<ppapi::PPB_FileRef_CreateInfo>::size_type i = 0;
       i < infos.size(); ++i) {
    DirectoryEntry entry;
    entry.file_resource = ScopedPPResource(
        ScopedPPResource::PassRef(),
        PPB_FileRef_Proxy::DeserializeFileRef(infos[i]));
    entry.file_type = file_types[i];
    entries_.push(entry);
  }

  if (!TrackedCallback::IsPending(callback_))
    return;

  FillUpEntry();
  callback_->Run(params.result());
}

bool DirectoryReaderResource::FillUpEntry() {
  DCHECK(output_);

  if (!did_receive_get_entries_reply_)
    return false;

  if (output_->file_ref) {
    // Release an existing file reference before the next reference is stored.
    // (see the API doc)
    PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(
        output_->file_ref);
  }

  if (!entries_.empty()) {
    DirectoryEntry entry = entries_.front();
    entries_.pop();
    output_->file_ref = entry.file_resource.Release();
    output_->file_type = entry.file_type;
    output_ = NULL;
    return true;
  }

  // Set the reference to zero to indicates reaching the end of the directory.
  output_->file_ref = 0;
  output_->file_type = PP_FILETYPE_OTHER;
  output_ = NULL;
  return true;
}

}  // namespace proxy
}  // namespace ppapi
