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

template <> const char* interface_name<PPB_DirectoryReader_Dev>() {
  return PPB_DIRECTORYREADER_DEV_INTERFACE;
}

}  // namespace

DirectoryReader_Dev::DirectoryReader_Dev(const FileRef& directory_ref) {
  if (!has_interface<PPB_DirectoryReader_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_DirectoryReader_Dev>()->Create(
      directory_ref.pp_resource()));
}

DirectoryReader_Dev::DirectoryReader_Dev(const DirectoryReader_Dev& other)
    : Resource(other) {
}

int32_t DirectoryReader_Dev::GetNextEntry(DirectoryEntry_Dev* entry,
                                          const CompletionCallback& cc) {
  if (!has_interface<PPB_DirectoryReader_Dev>())
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  return get_interface<PPB_DirectoryReader_Dev>()->GetNextEntry(
      pp_resource(), &entry->data_, cc.pp_completion_callback());
}

}  // namespace pp
