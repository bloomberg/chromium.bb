// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/file_io_resource.h"

#include "base/bind.h"
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

  if (callback->is_blocking() &&
      file_handle_ != base::kInvalidPlatformFileValue) {
    state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
    int32_t result = PP_ERROR_FAILED;
    base::PlatformFileInfo file_info;
    // Release the proxy lock while making a potentially blocking file call.
    {
      ProxyAutoUnlock unlock;
      result = base::GetPlatformFileInfo(file_handle_, &file_info) ?
          PP_OK : PP_ERROR_FAILED;
    }
    if ((result == PP_OK) && TrackedCallback::IsPending(callback)) {
      ppapi::PlatformFileInfoToPepperFileInfo(file_info, file_system_type_,
                                              info);
    }

    state_manager_.SetOperationFinished();
    return result;
  }

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
  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_READ);

  if (callback->is_blocking() &&
      file_handle_ != base::kInvalidPlatformFileValue) {
    int32_t result = PP_ERROR_FAILED;
    if (bytes_to_read >= 0) {
      // We need a buffer (and therefore we must copy) since we don't know until
      // reacquire the lock whether to write data to the plugin's buffer.
      scoped_ptr<char[]> buffer;
      // Release the proxy lock while making a potentially blocking file call.
      {
        ProxyAutoUnlock unlock;
        buffer.reset(new char[bytes_to_read]);
        int32_t bytes_read = base::ReadPlatformFile(
            file_handle_, offset, buffer.get(), bytes_to_read);
        result = (bytes_read < 0) ? PP_ERROR_FAILED : bytes_read;
      }
      if ((result >= 0) && TrackedCallback::IsPending(callback)) {
        ArrayWriter output;
        output.set_pp_array_output(array_output);
        if (output.is_valid())
          output.StoreArray(buffer.get(), result);
        else
          result = PP_ERROR_FAILED;
      }
    }

    state_manager_.SetOperationFinished();
    return result;
  }

  Call<PpapiPluginMsg_FileIO_ReadReply>(RENDERER,
      PpapiHostMsg_FileIO_Read(offset, bytes_to_read),
      base::Bind(&FileIOResource::OnPluginMsgReadComplete, this,
                 callback, array_output));
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

void FileIOResource::OnPluginMsgQueryComplete(
    scoped_refptr<TrackedCallback> callback,
    PP_FileInfo* output_info,
    const ResourceMessageReplyParams& params,
    const PP_FileInfo& info) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_EXCLUSIVE);
  *output_info = info;
  // End this operation now, so the user's callback can execute another FileIO
  // operation, assuming there are no other pending operations.
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
