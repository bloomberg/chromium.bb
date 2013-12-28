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
#include "ppapi/shared_impl/file_ref_create_info.h"
#include "ppapi/shared_impl/file_system_util.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "ppapi/thunk/ppb_file_system_api.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileIO_API;
using ppapi::thunk::PPB_FileRef_API;
using ppapi::thunk::PPB_FileSystem_API;

namespace {

// We must allocate a buffer sized according to the request of the plugin. To
// reduce the chance of out-of-memory errors, we cap the read and write size to
// 32MB. This is OK since the API specifies that it may perform a partial read
// or write.
static const int32_t kMaxReadWriteSize = 32 * 1024 * 1024;  // 32MB

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

FileIOResource::QueryOp::QueryOp(scoped_refptr<FileHandleHolder> file_handle)
    : file_handle_(file_handle) {
  DCHECK(file_handle_);
}

FileIOResource::QueryOp::~QueryOp() {
}

int32_t FileIOResource::QueryOp::DoWork() {
  return base::GetPlatformFileInfo(file_handle_->raw_handle(), &file_info_) ?
      PP_OK : PP_ERROR_FAILED;
}

FileIOResource::ReadOp::ReadOp(scoped_refptr<FileHandleHolder> file_handle,
                               int64_t offset,
                               int32_t bytes_to_read)
  : file_handle_(file_handle),
    offset_(offset),
    bytes_to_read_(bytes_to_read) {
  DCHECK(file_handle_);
}

FileIOResource::ReadOp::~ReadOp() {
}

int32_t FileIOResource::ReadOp::DoWork() {
  DCHECK(!buffer_.get());
  buffer_.reset(new char[bytes_to_read_]);
  return base::ReadPlatformFile(
      file_handle_->raw_handle(), offset_, buffer_.get(), bytes_to_read_);
}

FileIOResource::FileIOResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      called_close_(false) {
  SendCreate(BROWSER, PpapiHostMsg_FileIO_Create());
}

FileIOResource::~FileIOResource() {
  Close();
}

PPB_FileIO_API* FileIOResource::AsPPB_FileIO_API() {
  return this;
}

int32_t FileIOResource::Open(PP_Resource file_ref,
                             int32_t open_flags,
                             scoped_refptr<TrackedCallback> callback) {
  EnterResourceNoLock<PPB_FileRef_API> enter_file_ref(file_ref, true);
  if (enter_file_ref.failed())
    return PP_ERROR_BADRESOURCE;

  PPB_FileRef_API* file_ref_api = enter_file_ref.object();
  const FileRefCreateInfo& create_info = file_ref_api->GetCreateInfo();
  if (!FileSystemTypeIsValid(create_info.file_system_type)) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, false);
  if (rv != PP_OK)
    return rv;

  file_system_type_ = create_info.file_system_type;

  if (create_info.file_system_plugin_resource) {
    EnterResourceNoLock<PPB_FileSystem_API> enter_file_system(
        create_info.file_system_plugin_resource, true);
    if (enter_file_system.failed())
      return PP_ERROR_FAILED;
    // Take a reference on the FileSystem resource. The FileIO host uses the
    // FileSystem host for running tasks and checking quota.
    file_system_resource_ = enter_file_system.resource();
  }

  // Take a reference on the FileRef resource while we're opening the file; we
  // don't want the plugin destroying it during the Open operation.
  file_ref_ = enter_file_ref.resource();

  Call<PpapiPluginMsg_FileIO_OpenReply>(BROWSER,
      PpapiHostMsg_FileIO_Open(
          file_ref,
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
  if (!FileHandleHolder::IsValid(file_handle_))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);

  // If the callback is blocking, perform the task on the calling thread.
  if (callback->is_blocking()) {
    int32_t result = PP_ERROR_FAILED;
    base::PlatformFileInfo file_info;
    // The plugin could release its reference to this instance when we release
    // the proxy lock below.
    scoped_refptr<FileIOResource> protect(this);
    {
      // Release the proxy lock while making a potentially slow file call.
      ProxyAutoUnlock unlock;
      if (base::GetPlatformFileInfo(file_handle_->raw_handle(), &file_info))
        result = PP_OK;
    }
    if (result == PP_OK) {
      // This writes the file info into the plugin's PP_FileInfo struct.
      ppapi::PlatformFileInfoToPepperFileInfo(file_info,
                                              file_system_type_,
                                              info);
    }
    state_manager_.SetOperationFinished();
    return result;
  }

  // For the non-blocking case, post a task to the file thread and add a
  // completion task to write the result.
  scoped_refptr<QueryOp> query_op(new QueryOp(file_handle_));
  base::PostTaskAndReplyWithResult(
      PpapiGlobals::Get()->GetFileTaskRunner(),
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

  Call<PpapiPluginMsg_FileIO_GeneralReply>(BROWSER,
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
  bytes_to_write = std::min(bytes_to_write, kMaxReadWriteSize);
  Call<PpapiPluginMsg_FileIO_GeneralReply>(BROWSER,
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

  Call<PpapiPluginMsg_FileIO_GeneralReply>(BROWSER,
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

  Call<PpapiPluginMsg_FileIO_GeneralReply>(BROWSER,
      PpapiHostMsg_FileIO_Flush(),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this,
                 callback));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

void FileIOResource::Close() {
  if (called_close_)
    return;

  called_close_ = true;
  if (file_handle_)
    file_handle_ = NULL;

  Post(BROWSER, PpapiHostMsg_FileIO_Close());
}

int32_t FileIOResource::RequestOSFileHandle(
    PP_FileHandle* handle,
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  Call<PpapiPluginMsg_FileIO_RequestOSFileHandleReply>(BROWSER,
      PpapiHostMsg_FileIO_RequestOSFileHandle(),
      base::Bind(&FileIOResource::OnPluginMsgRequestOSFileHandleComplete, this,
                 callback, handle));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

FileIOResource::FileHandleHolder::FileHandleHolder(PP_FileHandle file_handle)
    : raw_handle_(file_handle) {
}

// static
bool FileIOResource::FileHandleHolder::IsValid(
    const scoped_refptr<FileIOResource::FileHandleHolder>& handle) {
  return handle && (handle->raw_handle() != base::kInvalidPlatformFileValue);
}

FileIOResource::FileHandleHolder::~FileHandleHolder() {
  if (raw_handle_ != base::kInvalidPlatformFileValue) {
    base::TaskRunner* file_task_runner =
        PpapiGlobals::Get()->GetFileTaskRunner();
    file_task_runner->PostTask(FROM_HERE,
                               base::Bind(&DoClose, raw_handle_));
  }
}

int32_t FileIOResource::ReadValidated(int64_t offset,
                                      int32_t bytes_to_read,
                                      const PP_ArrayOutput& array_output,
                                      scoped_refptr<TrackedCallback> callback) {
  if (bytes_to_read < 0)
    return PP_ERROR_FAILED;
  if (!FileHandleHolder::IsValid(file_handle_))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_READ);

  bytes_to_read = std::min(bytes_to_read, kMaxReadWriteSize);
  if (callback->is_blocking()) {
    char* buffer = static_cast<char*>(
        array_output.GetDataBuffer(array_output.user_data, bytes_to_read, 1));
    int32_t result = PP_ERROR_FAILED;
    // The plugin could release its reference to this instance when we release
    // the proxy lock below.
    scoped_refptr<FileIOResource> protect(this);
    if (buffer) {
      // Release the proxy lock while making a potentially slow file call.
      ProxyAutoUnlock unlock;
      result = base::ReadPlatformFile(
          file_handle_->raw_handle(), offset, buffer, bytes_to_read);
      if (result < 0)
        result = PP_ERROR_FAILED;
    }
    state_manager_.SetOperationFinished();
    return result;
  }

  // For the non-blocking case, post a task to the file thread.
  scoped_refptr<ReadOp> read_op(
      new ReadOp(file_handle_, offset, bytes_to_read));
  base::PostTaskAndReplyWithResult(
      PpapiGlobals::Get()->GetFileTaskRunner(),
      FROM_HERE,
      Bind(&FileIOResource::ReadOp::DoWork, read_op),
      RunWhileLocked(Bind(&TrackedCallback::Run, callback)));
  callback->set_completion_task(
      Bind(&FileIOResource::OnReadComplete, this, read_op, array_output));

  return PP_OK_COMPLETIONPENDING;
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

  // Release the FileRef resource.
  file_ref_ = NULL;
  if (params.result() == PP_OK)
    state_manager_.SetOpenSucceed();

  int32_t result = params.result();
  IPC::PlatformFileForTransit transit_file;
  if ((result == PP_OK) && params.TakeFileHandleAtIndex(0, &transit_file)) {
    file_handle_ = new FileHandleHolder(
        IPC::PlatformFileForTransitToPlatformFile(transit_file));
  }
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
