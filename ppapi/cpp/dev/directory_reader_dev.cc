// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/directory_reader_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/directory_entry_dev.h"
#include "ppapi/cpp/dev/file_ref_dev.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_DirectoryReader_Dev> directory_reader_f(
    PPB_DIRECTORYREADER_DEV_INTERFACE);

}  // namespace

namespace pp {

DirectoryReader_Dev::DirectoryReader_Dev(const FileRef_Dev& directory_ref) {
  if (!directory_reader_f)
    return;
  PassRefFromConstructor(
      directory_reader_f->Create(directory_ref.pp_resource()));
}

DirectoryReader_Dev::DirectoryReader_Dev(const DirectoryReader_Dev& other)
    : Resource(other) {
}

DirectoryReader_Dev& DirectoryReader_Dev::operator=(
    const DirectoryReader_Dev& other) {
  DirectoryReader_Dev copy(other);
  swap(copy);
  return *this;
}

void DirectoryReader_Dev::swap(DirectoryReader_Dev& other) {
  Resource::swap(other);
}

int32_t DirectoryReader_Dev::GetNextEntry(DirectoryEntry_Dev* entry,
                                          const CompletionCallback& cc) {
  if (!directory_reader_f)
    return PP_ERROR_NOINTERFACE;
  return directory_reader_f->GetNextEntry(pp_resource(), &entry->data_,
                                          cc.pp_completion_callback());
}

}  // namespace pp
