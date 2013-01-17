// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_file_io_shared.h"

#include <string.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"

namespace ppapi {

using thunk::EnterResourceNoLock;
using thunk::PPB_FileIO_API;
using thunk::PPB_FileRef_API;

namespace {

// An adapter to let Read() share the same implementation with ReadToArray().
void* DummyGetDataBuffer(void* user_data, uint32_t count, uint32_t size) {
  return user_data;
}

}  // namespace

PPB_FileIO_Shared::CallbackEntry::CallbackEntry()
    : info(NULL) {
}

PPB_FileIO_Shared::CallbackEntry::CallbackEntry(const CallbackEntry& entry)
    : callback(entry.callback),
      read_buffer(entry.read_buffer),
      info(entry.info) {
}

PPB_FileIO_Shared::CallbackEntry::~CallbackEntry() {
}

PPB_FileIO_Shared::PPB_FileIO_Shared(PP_Instance instance)
    : Resource(OBJECT_IS_IMPL, instance),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      file_open_(false),
      pending_op_(OPERATION_NONE) {
}

PPB_FileIO_Shared::PPB_FileIO_Shared(const HostResource& host_resource)
    : Resource(OBJECT_IS_PROXY, host_resource),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      file_open_(false),
      pending_op_(OPERATION_NONE) {
}

PPB_FileIO_Shared::~PPB_FileIO_Shared() {
}

thunk::PPB_FileIO_API* PPB_FileIO_Shared::AsPPB_FileIO_API() {
  return this;
}

int32_t PPB_FileIO_Shared::Open(PP_Resource file_ref,
                                int32_t open_flags,
                                scoped_refptr<TrackedCallback> callback) {
  EnterResourceNoLock<PPB_FileRef_API> enter(file_ref, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;

  int32_t rv = CommonCallValidation(false, OPERATION_EXCLUSIVE);
  if (rv != PP_OK)
    return rv;

  PP_FileSystemType type = enter.object()->GetFileSystemType();
  if (type != PP_FILESYSTEMTYPE_LOCALPERSISTENT &&
      type != PP_FILESYSTEMTYPE_LOCALTEMPORARY &&
      type != PP_FILESYSTEMTYPE_EXTERNAL)
    return PP_ERROR_FAILED;
  file_system_type_ = type;

  return OpenValidated(file_ref, enter.object(), open_flags, callback);
}

int32_t PPB_FileIO_Shared::Query(PP_FileInfo* info,
                                 scoped_refptr<TrackedCallback> callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE);
  if (rv != PP_OK)
    return rv;
  if (!info)
    return PP_ERROR_BADARGUMENT;
  return QueryValidated(info, callback);
}

int32_t PPB_FileIO_Shared::Touch(PP_Time last_access_time,
                                 PP_Time last_modified_time,
                                 scoped_refptr<TrackedCallback> callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE);
  if (rv != PP_OK)
    return rv;
  return TouchValidated(last_access_time, last_modified_time, callback);
}

int32_t PPB_FileIO_Shared::Read(int64_t offset,
                                char* buffer,
                                int32_t bytes_to_read,
                                scoped_refptr<TrackedCallback> callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_READ);
  if (rv != PP_OK)
    return rv;
  PP_ArrayOutput buffer_adapter = { &DummyGetDataBuffer, buffer };
  return ReadValidated(offset, buffer_adapter, bytes_to_read, callback);
}

int32_t PPB_FileIO_Shared::ReadToArray(
    int64_t offset,
    int32_t max_read_length,
    PP_ArrayOutput* output_array_buffer,
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_READ);
  if (rv != PP_OK)
    return rv;
  if (!output_array_buffer)
    return PP_ERROR_BADARGUMENT;
  return ReadValidated(offset, *output_array_buffer, max_read_length, callback);
}

int32_t PPB_FileIO_Shared::Write(int64_t offset,
                                 const char* buffer,
                                 int32_t bytes_to_write,
                                 scoped_refptr<TrackedCallback> callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_WRITE);
  if (rv != PP_OK)
    return rv;
  return WriteValidated(offset, buffer, bytes_to_write, callback);
}

int32_t PPB_FileIO_Shared::SetLength(int64_t length,
                                     scoped_refptr<TrackedCallback> callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE);
  if (rv != PP_OK)
    return rv;
  return SetLengthValidated(length, callback);
}

int32_t PPB_FileIO_Shared::Flush(scoped_refptr<TrackedCallback> callback) {
  int32_t rv = CommonCallValidation(true, OPERATION_EXCLUSIVE);
  if (rv != PP_OK)
    return rv;
  return FlushValidated(callback);
}

void PPB_FileIO_Shared::ExecuteGeneralCallback(int32_t pp_error) {
  RunAndRemoveFirstPendingCallback(pp_error);
}

void PPB_FileIO_Shared::ExecuteOpenFileCallback(int32_t pp_error) {
  if (pp_error == PP_OK)
    file_open_ = true;
  ExecuteGeneralCallback(pp_error);
}

void PPB_FileIO_Shared::ExecuteQueryCallback(int32_t pp_error,
                                             const PP_FileInfo& info) {
  if (pending_op_ != OPERATION_EXCLUSIVE || callbacks_.empty() ||
      !callbacks_.front().info) {
    NOTREACHED();
    return;
  }
  *callbacks_.front().info = info;
  RunAndRemoveFirstPendingCallback(pp_error);
}

void PPB_FileIO_Shared::ExecuteReadCallback(int32_t pp_error_or_bytes,
                                            const char* data) {
  if (pending_op_ != OPERATION_READ || callbacks_.empty()) {
    NOTREACHED();
    return;
  }

  // The result code contains the number of bytes if it's positive.
  ArrayWriter output;
  output.set_pp_array_output(callbacks_.front().read_buffer);
  if (output.is_valid()) {
    output.StoreArray(data, std::max(0, pp_error_or_bytes));
  }

  // Always issue the callback.
  RunAndRemoveFirstPendingCallback(pp_error_or_bytes);
}

int32_t PPB_FileIO_Shared::CommonCallValidation(bool should_be_open,
                                                OperationType new_op) {
  // Only asynchronous operation is supported.
  if (should_be_open) {
    if (!file_open_)
      return PP_ERROR_FAILED;
  } else {
    if (file_open_)
      return PP_ERROR_FAILED;
  }

  if (pending_op_ != OPERATION_NONE &&
      (pending_op_ != new_op || pending_op_ == OPERATION_EXCLUSIVE)) {
    return PP_ERROR_INPROGRESS;
  }

  return PP_OK;
}

void PPB_FileIO_Shared::RegisterCallback(
    OperationType op,
    scoped_refptr<TrackedCallback> callback,
    const PP_ArrayOutput* read_buffer,
    PP_FileInfo* info) {
  DCHECK(pending_op_ == OPERATION_NONE ||
         (pending_op_ != OPERATION_EXCLUSIVE && pending_op_ == op));

  CallbackEntry entry;
  entry.callback = callback;
  if (read_buffer)
    entry.read_buffer = *read_buffer;
  entry.info = info;
  callbacks_.push_back(entry);

  pending_op_ = op;
}

void PPB_FileIO_Shared::RunAndRemoveFirstPendingCallback(int32_t result) {
  DCHECK(!callbacks_.empty());

  CallbackEntry front = callbacks_.front();
  callbacks_.pop_front();
  if (callbacks_.empty())
    pending_op_ = OPERATION_NONE;

  front.callback->Run(result);
}

}  // namespace ppapi
