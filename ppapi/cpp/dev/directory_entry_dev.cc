// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/directory_entry_dev.h"

#include <string.h>

#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"

namespace pp {

DirectoryEntry_Dev::DirectoryEntry_Dev() {
  memset(&data_, 0, sizeof(data_));
}

DirectoryEntry_Dev::DirectoryEntry_Dev(
    PassRef, const PP_DirectoryEntry_Dev& data) {
  data_.file_ref = data.file_ref;
  data_.file_type = data.file_type;
}

DirectoryEntry_Dev::DirectoryEntry_Dev(const DirectoryEntry_Dev& other) {
  data_.file_ref = other.data_.file_ref;
  data_.file_type = other.data_.file_type;
  if (data_.file_ref)
    Module::Get()->core()->AddRefResource(data_.file_ref);
}

DirectoryEntry_Dev::~DirectoryEntry_Dev() {
  if (data_.file_ref)
    Module::Get()->core()->ReleaseResource(data_.file_ref);
}

DirectoryEntry_Dev& DirectoryEntry_Dev::operator=(
    const DirectoryEntry_Dev& other) {
  if (data_.file_ref)
    Module::Get()->core()->ReleaseResource(data_.file_ref);
  data_ = other.data_;
  if (data_.file_ref)
    Module::Get()->core()->AddRefResource(data_.file_ref);
  return *this;
}

namespace internal {

DirectoryEntryArrayOutputAdapterWithStorage::
    DirectoryEntryArrayOutputAdapterWithStorage() {
  set_output(&temp_storage_);
}

DirectoryEntryArrayOutputAdapterWithStorage::
    ~DirectoryEntryArrayOutputAdapterWithStorage() {
  if (!temp_storage_.empty()) {
    // An easy way to release the resource references held by |temp_storage_|.
    // A destructor for PP_DirectoryEntry_Dev will release them.
    output();
  }
}

std::vector<DirectoryEntry_Dev>&
    DirectoryEntryArrayOutputAdapterWithStorage::output() {
  PP_DCHECK(output_storage_.empty());
  typedef std::vector<PP_DirectoryEntry_Dev> Entries;
  for (Entries::iterator it = temp_storage_.begin();
       it != temp_storage_.end();
       ++it) {
    output_storage_.push_back(DirectoryEntry_Dev(PASS_REF, *it));
  }
  temp_storage_.clear();
  return output_storage_;
}

}  // namespace internal
}  // namespace pp
