// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_file_io_trusted.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_file_io_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterResource<PPB_FileIO_API> EnterFileIO;

int32_t GetOSFileDescriptor(PP_Resource file_io) {
  EnterFileIO enter(file_io, true);
  if (enter.failed())
    return enter.retval();
  return enter.object()->GetOSFileDescriptor();
}

int32_t WillWrite(PP_Resource file_io,
                  int64_t offset,
                  int32_t bytes_to_write,
                  PP_CompletionCallback callback) {
  EnterFileIO enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->WillWrite(offset, bytes_to_write,
                                                   enter.callback()));
}

int32_t WillSetLength(PP_Resource file_io,
                      int64_t length,
                      PP_CompletionCallback callback) {
  EnterFileIO enter(file_io, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->WillSetLength(length,
                                                       enter.callback()));
}

const PPB_FileIOTrusted g_ppb_file_io_trusted_thunk = {
  &GetOSFileDescriptor,
  &WillWrite,
  &WillSetLength
};

}  // namespace

const PPB_FileIOTrusted_0_4* GetPPB_FileIOTrusted_0_4_Thunk() {
  return &g_ppb_file_io_trusted_thunk;
}

}  // namespace thunk
}  // namespace ppapi
