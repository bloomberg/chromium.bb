// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/file_io_resource.h"

#include "base/bind.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileIO_API;
using ppapi::thunk::PPB_FileRef_API;

namespace {

// An adapter to let Read() share the same implementation with ReadToArray().
void* DummyGetDataBuffer(void* user_data, uint32_t count, uint32_t size) {
  return user_data;
}

}  // namespace

namespace ppapi {
namespace proxy {

FileIOResource::FileIOResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(RENDERER, PpapiHostMsg_FileIO_Create());
}

FileIOResource::~FileIOResource() {
}

PPB_FileIO_API* FileIOResource::AsPPB_FileIO_API() {
  return this;
}

int32_t FileIOResource::Open(PP_Resource file_ref,
                             int32_t open_flags,
                             scoped_refptr<TrackedCallback> callback) {
  EnterResourceNoLock<PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, false);
  if (rv != PP_OK)
    return rv;

  Call<PpapiPluginMsg_FileIO_OpenReply>(RENDERER,
      PpapiHostMsg_FileIO_Open(
          enter.resource()->host_resource().host_resource(),
          open_flags),
      base::Bind(&FileIOResource::OnPluginMsgOpenFileComplete, this,
                 callback));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::Query(PP_FileInfo* info,
                              scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  Call<PpapiPluginMsg_FileIO_QueryReply>(RENDERER,
      PpapiHostMsg_FileIO_Query(),
      base::Bind(&FileIOResource::OnPluginMsgQueryComplete, this,
                 callback, info));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::Touch(PP_Time last_access_time,
                              PP_Time last_modified_time,
                              scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  Call<PpapiPluginMsg_FileIO_GeneralReply>(RENDERER,
      PpapiHostMsg_FileIO_Touch(last_access_time, last_modified_time),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this,
                 callback));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::Read(int64_t offset,
                             char* buffer,
                             int32_t bytes_to_read,
                             scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_READ, true);
  if (rv != PP_OK)
    return rv;

  PP_ArrayOutput output_adapter;
  output_adapter.GetDataBuffer = &DummyGetDataBuffer;
  output_adapter.user_data = buffer;
  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_READ);
  return ReadValidated(offset, bytes_to_read, output_adapter, callback);
}

int32_t FileIOResource::ReadToArray(int64_t offset,
                                    int32_t max_read_length,
                                    PP_ArrayOutput* array_output,
                                    scoped_refptr<TrackedCallback> callback) {
  DCHECK(array_output);
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_READ, true);
  if (rv != PP_OK)
    return rv;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_READ);
  return ReadValidated(offset, max_read_length, *array_output, callback);
}

int32_t FileIOResource::Write(int64_t offset,
                              const char* buffer,
                              int32_t bytes_to_write,
                              scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_WRITE, true);
  if (rv != PP_OK)
    return rv;

  // TODO(brettw) it would be nice to use a shared memory buffer for large
  // writes rather than having to copy to a string (which will involve a number
  // of extra copies to serialize over IPC).
  Call<PpapiPluginMsg_FileIO_GeneralReply>(RENDERER,
      PpapiHostMsg_FileIO_Write(offset, std::string(buffer, bytes_to_write)),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this,
                 callback));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_WRITE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::SetLength(int64_t length,
                                  scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  Call<PpapiPluginMsg_FileIO_GeneralReply>(RENDERER,
      PpapiHostMsg_FileIO_SetLength(length),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this,
                 callback));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::Flush(scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  Call<PpapiPluginMsg_FileIO_GeneralReply>(RENDERER,
      PpapiHostMsg_FileIO_Flush(),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this,
                 callback));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

void FileIOResource::Close() {
  Post(RENDERER, PpapiHostMsg_FileIO_Close());
}

int32_t FileIOResource::GetOSFileDescriptor() {
  int32_t file_descriptor;
  // Only available when running in process.
  SyncCall<PpapiPluginMsg_FileIO_GetOSFileDescriptorReply>(
      RENDERER, PpapiHostMsg_FileIO_GetOSFileDescriptor(), &file_descriptor);
  return file_descriptor;
}

int32_t FileIOResource::WillWrite(int64_t offset,
                                  int32_t bytes_to_write,
                                  scoped_refptr<TrackedCallback> callback) {
  Call<PpapiPluginMsg_FileIO_GeneralReply>(RENDERER,
      PpapiHostMsg_FileIO_WillWrite(offset, bytes_to_write),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this,
                 callback));
  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::WillSetLength(int64_t length,
                                      scoped_refptr<TrackedCallback> callback) {
  Call<PpapiPluginMsg_FileIO_GeneralReply>(RENDERER,
      PpapiHostMsg_FileIO_WillSetLength(length),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this,
                 callback));
  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::ReadValidated(int64_t offset,
                                      int32_t bytes_to_read,
                                      const PP_ArrayOutput& array_output,
                                      scoped_refptr<TrackedCallback> callback) {
  Call<PpapiPluginMsg_FileIO_ReadReply>(RENDERER,
      PpapiHostMsg_FileIO_Read(offset, bytes_to_read),
      base::Bind(&FileIOResource::OnPluginMsgReadComplete, this,
                 callback, array_output));
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::RequestOSFileHandle(
    PP_FileHandle* handle,
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  Call<PpapiPluginMsg_FileIO_RequestOSFileHandleReply>(RENDERER,
      PpapiHostMsg_FileIO_RequestOSFileHandle(),
      base::Bind(&FileIOResource::OnPluginMsgRequestOSFileHandleComplete, this,
                 callback, handle));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

void FileIOResource::OnPluginMsgGeneralComplete(
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_EXCLUSIVE ||
         state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_WRITE);
  // End the operation now. The callback may perform another file operation.
  state_manager_.SetOperationFinished();
  callback->Run(params.result());
}

void FileIOResource::OnPluginMsgOpenFileComplete(
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_EXCLUSIVE);
  if (params.result() == PP_OK)
    state_manager_.SetOpenSucceed();
  // End the operation now. The callback may perform another file operation.
  state_manager_.SetOperationFinished();
  callback->Run(params.result());
}

void FileIOResource::OnPluginMsgQueryComplete(
    scoped_refptr<TrackedCallback> callback,
    PP_FileInfo* output_info,
    const ResourceMessageReplyParams& params,
    const PP_FileInfo& info) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_EXCLUSIVE);
  *output_info = info;
  // End the operation now. The callback may perform another file operation.
  state_manager_.SetOperationFinished();
  callback->Run(params.result());
}

void FileIOResource::OnPluginMsgReadComplete(
    scoped_refptr<TrackedCallback> callback,
    PP_ArrayOutput array_output,
    const ResourceMessageReplyParams& params,
    const std::string& data) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_READ);

  // The result code should contain the data size if it's positive.
  int32_t result = params.result();
  DCHECK((result < 0 && data.size() == 0) ||
         result == static_cast<int32_t>(data.size()));

  ArrayWriter output;
  output.set_pp_array_output(array_output);
  if (output.is_valid())
    output.StoreArray(data.data(), std::max(0, result));
  else
    result = PP_ERROR_FAILED;

  // End the operation now. The callback may perform another file operation.
  state_manager_.SetOperationFinished();
  callback->Run(result);
}

void FileIOResource::OnPluginMsgRequestOSFileHandleComplete(
    scoped_refptr<TrackedCallback> callback,
    PP_FileHandle* output_handle,
    const ResourceMessageReplyParams& params) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_EXCLUSIVE);

  if (!TrackedCallback::IsPending(callback)) {
    state_manager_.SetOperationFinished();
    return;
  }

  int32_t result = params.result();
  IPC::PlatformFileForTransit transit_file;
  if (!params.TakeFileHandleAtIndex(0, &transit_file))
    result = PP_ERROR_FAILED;
  *output_handle = IPC::PlatformFileForTransitToPlatformFile(transit_file);

  // End the operation now. The callback may perform another file operation.
  state_manager_.SetOperationFinished();
  callback->Run(result);
}

}  // namespace proxy
}  // namespace ppapi
