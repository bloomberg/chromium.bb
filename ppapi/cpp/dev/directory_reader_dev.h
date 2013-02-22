// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_DIRECTORY_READER_DEV_H_
#define PPAPI_CPP_DEV_DIRECTORY_READER_DEV_H_

#include <stdlib.h>

#include "ppapi/c/dev/ppb_directory_reader_dev.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class DirectoryEntry_Dev;
class FileRef;

class DirectoryReader_Dev : public Resource {
 public:
  // Creates a DirectoryReader for the given directory.
  explicit DirectoryReader_Dev(const FileRef& directory_ref);

  DirectoryReader_Dev(const DirectoryReader_Dev& other);

  // See PPB_DirectoryReader::GetNextEntry.
  int32_t GetNextEntry(DirectoryEntry_Dev* entry,
                       const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_DIRECTORY_READER_H_
