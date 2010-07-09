// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_file_system.h"

#include "third_party/ppapi/c/pp_completion_callback.h"
#include "third_party/ppapi/c/pp_errors.h"
#include "third_party/ppapi/c/ppb_file_system.h"

namespace pepper {

namespace {

int32_t MakeDirectory(PP_Resource directory_ref,
                      bool make_ancestors,
                      PP_CompletionCallback callback) {
  return PP_ERROR_FAILED;  // TODO(darin): Implement me!
}

int32_t Query(PP_Resource file_ref,
              PP_FileInfo* info,
              PP_CompletionCallback callback) {
  return PP_ERROR_FAILED;  // TODO(darin): Implement me!
}

int32_t Touch(PP_Resource file_ref,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              PP_CompletionCallback callback) {
  return PP_ERROR_FAILED;  // TODO(darin): Implement me!
}

int32_t Delete(PP_Resource file_ref,
               PP_CompletionCallback callback) {
  return PP_ERROR_FAILED;  // TODO(darin): Implement me!
}

int32_t Rename(PP_Resource file_ref,
               PP_Resource new_file_ref,
               PP_CompletionCallback callback) {
  return PP_ERROR_FAILED;  // TODO(darin): Implement me!
}

const PPB_FileSystem ppb_filesystem = {
  &MakeDirectory,
  &Query,
  &Touch,
  &Delete,
  &Rename
};

}  // namespace

const PPB_FileSystem* FileSystem::GetInterface() {
  return &ppb_filesystem;
}

}  // namespace pepper
