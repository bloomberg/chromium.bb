// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_directory_reader_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Resource Create(PP_Resource directory_ref) {
  ppapi::ProxyAutoLock lock;
  Resource* object =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(directory_ref);
  if (!object)
    return 0;
  EnterResourceCreationNoLock enter(object->pp_instance());
  if (enter.failed())
    return 0;
  return enter.functions()->CreateDirectoryReader(
      object->pp_instance(), directory_ref);
}

PP_Bool IsDirectoryReader(PP_Resource resource) {
  EnterResource<PPB_DirectoryReader_API> enter(resource, false);
  return PP_FromBool(enter.succeeded());
}

int32_t GetNextEntry(PP_Resource directory_reader,
                     PP_DirectoryEntry_Dev* entry,
                     PP_CompletionCallback callback) {
  EnterResource<PPB_DirectoryReader_API> enter(
      directory_reader, callback, true);
  if (enter.failed())
    return enter.retval();
  return enter.SetResult(enter.object()->GetNextEntry(entry, enter.callback()));
}

const PPB_DirectoryReader_Dev g_ppb_directory_reader_thunk = {
  &Create,
  &IsDirectoryReader,
  &GetNextEntry
};

}  // namespace

const PPB_DirectoryReader_Dev_0_5* GetPPB_DirectoryReader_Dev_0_5_Thunk() {
  return &g_ppb_directory_reader_thunk;
}

}  // namespace thunk
}  // namespace ppapi
