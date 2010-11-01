// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/file_system_dev.h"

#include "ppapi/c/dev/ppb_file_system_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_FileSystem_Dev> file_sys_f(PPB_FILESYSTEM_DEV_INTERFACE);

}  // namespace

namespace pp {

FileSystem_Dev::FileSystem_Dev(Instance* instance,
                               PP_FileSystemType_Dev type) {
  if (!file_sys_f)
    return;
  PassRefFromConstructor(file_sys_f->Create(instance->pp_instance(), type));
}

int32_t FileSystem_Dev::Open(int64_t expected_size,
                             const CompletionCallback& cc) {
  if (!file_sys_f)
    return PP_ERROR_NOINTERFACE;
  return file_sys_f->Open(pp_resource(), expected_size,
                          cc.pp_completion_callback());
}

}  // namespace pp
