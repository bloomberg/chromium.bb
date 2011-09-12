// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/common.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_directory_reader_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Resource directory_ref) {
  EnterFunctionGivenResource<ResourceCreationAPI> enter(directory_ref, true);
  if (enter.failed())
    return 0;
  return enter.functions()->CreateDirectoryReader(directory_ref);
}

PP_Bool IsDirectoryReader(PP_Resource resource) {
  EnterResource<PPB_DirectoryReader_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t GetNextEntry(PP_Resource directory_reader,
                     PP_DirectoryEntry_Dev* entry,
                     PP_CompletionCallback callback) {
  EnterResource<PPB_DirectoryReader_API> enter(directory_reader, true);
  if (enter.failed())
    return MayForceCallback(callback, PP_ERROR_BADRESOURCE);
  int32_t result = enter.object()->GetNextEntry(entry, callback);
  return MayForceCallback(callback, result);
}

const PPB_DirectoryReader_Dev g_ppb_directory_reader_thunk = {
  &Create,
  &IsDirectoryReader,
  &GetNextEntry
};

}  // namespace

const PPB_DirectoryReader_Dev* GetPPB_DirectoryReader_Dev_Thunk() {
  return &g_ppb_directory_reader_thunk;
}

}  // namespace thunk
}  // namespace ppapi
