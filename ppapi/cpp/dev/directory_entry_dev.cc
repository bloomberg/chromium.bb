// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/directory_entry_dev.h"

#include <string.h>

#include "ppapi/cpp/module.h"

namespace pp {

DirectoryEntry_Dev::DirectoryEntry_Dev() {
  memset(&data_, 0, sizeof(data_));
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
  DirectoryEntry_Dev copy(other);
  swap(copy);
  return *this;
}

void DirectoryEntry_Dev::swap(DirectoryEntry_Dev& other) {
  std::swap(data_.file_ref, other.data_.file_ref);
  std::swap(data_.file_type, other.data_.file_type);
}

}  // namespace pp
