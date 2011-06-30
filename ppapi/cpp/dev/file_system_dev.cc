// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/file_system_dev.h"

#include "ppapi/c/dev/ppb_file_system_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileSystem_Dev>() {
  return PPB_FILESYSTEM_DEV_INTERFACE;
}

}  // namespace

FileSystem_Dev::FileSystem_Dev(Instance* instance,
                               PP_FileSystemType_Dev type) {
  if (!has_interface<PPB_FileSystem_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_FileSystem_Dev>()->Create(
      instance->pp_instance(), type));
}

int32_t FileSystem_Dev::Open(int64_t expected_size,
                             const CompletionCallback& cc) {
  if (!has_interface<PPB_FileSystem_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileSystem_Dev>()->Open(
      pp_resource(), expected_size, cc.pp_completion_callback());
}

}  // namespace pp
