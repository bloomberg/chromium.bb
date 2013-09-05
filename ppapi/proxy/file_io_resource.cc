// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/file_io_resource.h"

#include "base/bind.h"
#include "base/task_runner_util.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileIO_API;
using ppapi::thunk::PPB_FileRef_API;

namespace {

// We must allocate a buffer sized according to the request of the plugin. To
// reduce the chance of out-of-memory errors, we cap the read size to 32MB.
// This is OK since the API specifies that it may perform a partial read.
static const int32_t kMaxReadSize = 32 * 1024 * 1024;  // 32MB

// An adapter to let Read() share the same implementation with ReadToArray().
void* DummyGetDataBuffer(void* user_data, uint32_t count, uint32_t size) {
  return user_data;
}

// File thread task to close the file handle.
void DoClose(base::PlatformFile file) {
  base::ClosePlatformFile(file);
}

}  // namespace

namespace ppapi {
namespace proxy {

FileIOResource::QueryOp::QueryOp(PP_FileHandle file_handle)
    : file_handle_(file_handle) {
}

FileIOResource::QueryOp::~QueryOp() {
}

int32_t FileIOResource::QueryOp::DoWork() {
  return base::GetPlatformFileInfo(file_handle_, &file_info_) ?
      PP_OK : PP_ERROR_FAILED;
}

FileIOResource::ReadOp::ReadOp(PP_FileHandle file_handle,
                               int64_t offset,
                               int32_t bytes_to_read)
  : file_handle_(file_handle),
    offset_(offset),
    bytes_to_read_(bytes_to_read) {
}

FileIOResource::ReadOp::~ReadOp() {
}

int32_t FileIOResource::ReadOp::DoWork() {
  DCHECK(!buffer_.get());
  buffer_.reset(new char[bytes_to_read_]);
  return base::ReadPlatformFile(
      file_handle_, offset_, buffer_.get(), bytes_to_read_);
}

FileIOResource::FileIOResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance),
      file_handle_(base::kInvalidPlatformFileValue),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID) {
  SendCreate(RENDERER, PpapiHostMsg_FileIO_Create());
}

FileIOResource::~FileIOResource() {
  CloseFileHandle();
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

  PPB_FileRef_API* file_ref_api = enter.object();
  PP_FileSystemType type = file_ref_api->GetFileSystemType();
  if (type != PP_FILESYSTEMTYPE_LOCALPERSISTENT &&
      type != PP_FILESYSTEMTYPE_LOCALTEMPORARY &&
      type != PP_FILESYSTEMTYPE_EXTERNAL &&
      type != PP_FILESYSTEMTYPE_ISOLATED) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }
  file_system_type_ = type;

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
  if (!info)
    return PP_ERROR_BADARGUMENT;
  if (file_handle_ == base::kInvalidPlatformFileValue)
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  scoped_refptr<QueryOp> query_op(new QueryOp(file_handle_));

  // If the callback is blocking, perform the task on the calling thread.
  if (callback->is_blocking()) {
    int32_t result;
    {
      // Release the proxy lock while making a potentially slow file call.
      ProxyAutoUnlock unlock;
      result = query_op->DoWork();
    }
    return OnQueryComplete(query_op, info, result);
  }

  // For the non-blocking case, post a task to the file thread and add a
  // completion task to write the result.
  base::PostTaskAndReplyWithResult(
      PpapiGlobals::Get()->GetFileTaskRunner(pp_instance()),
      FROM_HERE,
      Bind(&FileIOResource::QueryOp::DoWork, query_op),
      RunWhileLocked(Bind(&TrackedCallback::Run, callback)));
  callback->set_completion_task(
      Bind(&FileIOResource::OnQueryComplete, this, query_op, info));

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
  CloseFileHandle();
  Post(RENDERER, PpapiHostMsg_FileIO_Close());
}

int32_t FileIOResource::GetOSFileDescriptor() {
  int32_t file_descriptor;
  // Only available when running in process.
  SyncCall<PpapiPluginMsg_FileIO_GetOSFileDescriptorReply>(
      RENDERER, PpapiHostMsg_FileIO_GetOSFileDescriptor(), &file_descriptor);
  return file_descriptor;
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

int32_t FileIOResource::WillWrite(int64_t offset,
                                  int32_t bytes_to_write,
                                  scoped_refptr<TrackedCallback> callback) {
  Call<PpapiPluginMsg_FileIO_GeneralReply>(RENDERER,
      PpapiHostMsg_FileIO_WillWrite(offset, bytes_to_write),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this, callback));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::WillSetLength(int64_t length,
                                      scoped_refptr<TrackedCallback> callback) {
  Call<PpapiPluginMsg_FileIO_GeneralReply>(RENDERER,
      PpapiHostMsg_FileIO_WillSetLength(length),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this, callback));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::ReadValidated(int64_t offset,
                                      int32_t bytes_to_read,
                                      const PP_ArrayOutput& array_output,
                                      scoped_refptr<TrackedCallback> callback) {
  if (bytes_to_read < 0)
    return PP_ERROR_FAILED;
  if (file_handle_ == base::kInvalidPlatformFileValue)
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_READ);

  bytes_to_read = std::min(bytes_to_read, kMaxReadSize);
  scoped_refptr<ReadOp> read_op(
      new ReadOp(file_handle_, offset, bytes_to_read));
  if (callback->is_blocking()) {
    int32_t result;
    {
      // Release the proxy lock while making a potentially slow file call.
      ProxyAutoUnlock unlock;
      result = read_op->DoWork();
    }
    return OnReadComplete(read_op, array_output, result);
  }

  // For the non-blocking case, post a task to the file thread.
  base::PostTaskAndReplyWithResult(
      PpapiGlobals::Get()->GetFileTaskRunner(pp_instance()),
      FROM_HERE,
      Bind(&FileIOResource::ReadOp::DoWork, read_op),
      RunWhileLocked(Bind(&TrackedCallback::Run, callback)));
  callback->set_completion_task(
      Bind(&FileIOResource::OnReadComplete, this, read_op, array_output));

  return PP_OK_COMPLETIONPENDING;
}

void FileIOResource::CloseFileHandle() {
  if (file_handle_ != base::kInvalidPlatformFileValue) {
    // Close our local fd on the file thread.
    base::TaskRunner* file_task_runner =
        PpapiGlobals::Get()->GetFileTaskRunner(pp_instance());
    file_task_runner->PostTask(FROM_HERE,
                               base::Bind(&DoClose, file_handle_));

    file_handle_ = base::kInvalidPlatformFileValue;
  }
}

int32_t FileIOResource::OnQueryComplete(scoped_refptr<QueryOp> query_op,
                                        PP_FileInfo* info,
                                        int32_t result) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_EXCLUSIVE);

  if (result == PP_OK) {
    // This writes the file info into the plugin's PP_FileInfo struct.
    ppapi::PlatformFileInfoToPepperFileInfo(query_op->file_info(),
                                            file_system_type_,
                                            info);
  }
  state_manager_.SetOperationFinished();
  return result;
}

int32_t FileIOResource::OnReadComplete(scoped_refptr<ReadOp> read_op,
                                       PP_ArrayOutput array_output,
                                       int32_t result) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_READ);
  if (result >= 0) {
    ArrayWriter output;
    output.set_pp_array_output(array_output);
    if (output.is_valid())
      output.StoreArray(read_op->buffer(), result);
    else
      result = PP_ERROR_FAILED;
  } else {
    // The read operation failed.
    result = PP_ERROR_FAILED;
  }
  state_manager_.SetOperationFinished();
  return result;
}

void FileIOResource::OnPluginMsgGeneralComplete(
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_EXCLUSIVE ||
         state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_WRITE);
  // End this operation now, so the user's callback can execute another FileIO
  // operation, assuming there are no other pending operations.
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

  int32_t result = params.result();
  IPC::PlatformFileForTransit transit_file;
  if ((result == PP_OK) && params.TakeFileHandleAtIndex(0, &transit_file))
    file_handle_ = IPC::PlatformFileForTransitToPlatformFile(transit_file);
  // End this operation now, so the user's callback can execute another FileIO
  // operation, assuming there are no other pending operations.
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

  // End this operation now, so the user's callback can execute another FileIO
  // operation, assuming there are no other pending operations.
  state_manager_.SetOperationFinished();
  callback->Run(result);
}

}  // namespace proxy
}  // namespace ppapi
