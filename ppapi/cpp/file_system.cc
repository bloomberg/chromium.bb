// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/file_system.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_FileSystem>() {
  return PPB_FILESYSTEM_INTERFACE;
}

}  // namespace

FileSystem::FileSystem() {
}

FileSystem::FileSystem(Instance* instance,
                       PP_FileSystemType type) {
  if (!has_interface<PPB_FileSystem>())
    return;
  PassRefFromConstructor(get_interface<PPB_FileSystem>()->Create(
      instance->pp_instance(), type));
}

int32_t FileSystem::Open(int64_t expected_size,
                         const CompletionCallback& cc) {
  if (!has_interface<PPB_FileSystem>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_FileSystem>()->Open(
      pp_resource(), expected_size, cc.pp_completion_callback());
}

}  // namespace pp
