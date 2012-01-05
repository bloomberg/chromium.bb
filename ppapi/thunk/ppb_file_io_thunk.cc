// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_file_io_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Instance instance) {
  EnterFunction<ResourceCreationAPI> enter(instance, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFileIO(instance);
}

PP_Bool IsFileIO(PP_Resource resource) {
  EnterResource<PPB_FileIO_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Open(PP_Resource file_io,
             PP_Resource file_ref,
             int32_t open_flags,
             PP_CompletionCallback callback) {
  EnterResource<PPB_FileIO_API> enter(file_io, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Open(file_ref, open_flags, callback);
  return MayForceCallback(callback, result);
}

int32_t Query(PP_Resource file_io,
              PP_FileInfo* info,
              PP_CompletionCallback callback) {
  EnterResource<PPB_FileIO_API> enter(file_io, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Query(info, callback);
  return MayForceCallback(callback, result);
}

int32_t Touch(PP_Resource file_io,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              PP_CompletionCallback callback) {
  EnterResource<PPB_FileIO_API> enter(file_io, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Touch(last_access_time, last_modified_time,
                                         callback);
  return MayForceCallback(callback, result);
}

int32_t Read(PP_Resource file_io,
             int64_t offset,
             char* buffer,
             int32_t bytes_to_read,
             PP_CompletionCallback callback) {
  EnterResource<PPB_FileIO_API> enter(file_io, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Read(offset, buffer, bytes_to_read,
                                        callback);
  return MayForceCallback(callback, result);
}

int32_t Write(PP_Resource file_io,
              int64_t offset,
              const char* buffer,
              int32_t bytes_to_write,
              PP_CompletionCallback callback) {
  EnterResource<PPB_FileIO_API> enter(file_io, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Write(offset, buffer, bytes_to_write,
                                         callback);
  return MayForceCallback(callback, result);
}

int32_t SetLength(PP_Resource file_io,
                  int64_t length,
                  PP_CompletionCallback callback) {
  EnterResource<PPB_FileIO_API> enter(file_io, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->SetLength(length, callback);
  return MayForceCallback(callback, result);
}

int32_t Flush(PP_Resource file_io,
              PP_CompletionCallback callback) {
  EnterResource<PPB_FileIO_API> enter(file_io, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->Flush(callback);
  return MayForceCallback(callback, result);
}

void Close(PP_Resource file_io) {
  EnterResource<PPB_FileIO_API> enter(file_io, true);
  if (enter.succeeded())
    enter.object()->Close();
}

const PPB_FileIO g_ppb_file_io_thunk = {
  &Create,
  &IsFileIO,
  &Open,
  &Query,
  &Touch,
  &Read,
  &Write,
  &SetLength,
  &Flush,
  &Close
};

}  // namespace

const PPB_FileIO_1_0* GetPPB_FileIO_1_0_Thunk() {
  return &g_ppb_file_io_thunk;
}

}  // namespace thunk
}  // namespace ppapi
