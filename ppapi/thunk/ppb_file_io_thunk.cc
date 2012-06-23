// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_file_io_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_FileIO_API> EnterFileIO;

PP_Resource Create(PP_Instance instance) {
  EnterResourceCreation enter(instance);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateFileIO(instance);
}

PP_Bool IsFileIO(PP_Resource resource) {
  EnterFileIO enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t Open(PP_Resource file_io,
             PP_Resource file_ref,
             int32_t open_flags,
             PP_CompletionCallback callback) {
  EnterFileIO enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Open(file_ref, open_flags,
                         enter.callback()));
}

int32_t Query(PP_Resource file_io,
              PP_FileInfo* info,
              PP_CompletionCallback callback) {
  EnterFileIO enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Query(info, enter.callback()));
}

int32_t Touch(PP_Resource file_io,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              PP_CompletionCallback callback) {
  EnterFileIO enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Touch(
      last_access_time, last_modified_time, enter.callback()));
}

int32_t Read(PP_Resource file_io,
             int64_t offset,
             char* buffer,
             int32_t bytes_to_read,
             PP_CompletionCallback callback) {
  EnterFileIO enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Read(offset, buffer, bytes_to_read,
                                              enter.callback()));
}

int32_t Write(PP_Resource file_io,
              int64_t offset,
              const char* buffer,
              int32_t bytes_to_write,
              PP_CompletionCallback callback) {
  EnterFileIO enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Write(offset, buffer, bytes_to_write,
                                               enter.callback()));
}

int32_t SetLength(PP_Resource file_io,
                  int64_t length,
                  PP_CompletionCallback callback) {
  EnterFileIO enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->SetLength(length, enter.callback()));
}

int32_t Flush(PP_Resource file_io,
              PP_CompletionCallback callback) {
  EnterFileIO enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->Flush(enter.callback()));
}

void Close(PP_Resource file_io) {
  EnterFileIO enter(file_io, true);
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
