// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_DIRECTORY_ENTRY_DEV_H_
#define PPAPI_CPP_DEV_DIRECTORY_ENTRY_DEV_H_

#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "ppapi/cpp/file_ref.h"

namespace pp {

class DirectoryEntry_Dev {
 public:
  DirectoryEntry_Dev();
  DirectoryEntry_Dev(const DirectoryEntry_Dev& other);
  ~DirectoryEntry_Dev();

  DirectoryEntry_Dev& operator=(const DirectoryEntry_Dev& other);

  // Returns true if the DirectoryEntry is invalid or uninitialized.
  bool is_null() const { return !data_.file_ref; }

  // Returns the FileRef held by this DirectoryEntry.
  FileRef file_ref() const { return FileRef(data_.file_ref); }

  // Returns the type of the file referenced by this DirectoryEntry.
  PP_FileType file_type() const { return data_.file_type; }

 private:
  friend class DirectoryReader_Dev;
  PP_DirectoryEntry_Dev data_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_DIRECTORY_ENTRY_DEV_H_
