// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/directory_reader_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/directory_entry_dev.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_DirectoryReader_Dev_0_6>() {
  return PPB_DIRECTORYREADER_DEV_INTERFACE_0_6;
}

}  // namespace

DirectoryReader_Dev::DirectoryReader_Dev(const FileRef& directory_ref) {
  if (!has_interface<PPB_DirectoryReader_Dev_0_6>())
    return;
  PassRefFromConstructor(get_interface<PPB_DirectoryReader_Dev_0_6>()->Create(
      directory_ref.pp_resource()));
}

DirectoryReader_Dev::DirectoryReader_Dev(const DirectoryReader_Dev& other)
    : Resource(other) {
}

int32_t DirectoryReader_Dev::ReadEntries(
    const CompletionCallbackWithOutput< std::vector<DirectoryEntry_Dev> >&
        callback) {
  if (!has_interface<PPB_DirectoryReader_Dev_0_6>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_DirectoryReader_Dev_0_6>()->ReadEntries(
      pp_resource(), callback.output(), callback.pp_completion_callback());
}

}  // namespace pp
