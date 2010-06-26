// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_DIRECTORY_READER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_DIRECTORY_READER_H_

#include "third_party/ppapi/c/ppb_directory_reader.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

class FileRef;

class DirectoryReader : public Resource {
 public:
  explicit DirectoryReader(FileRef* directory_ref);
  virtual ~DirectoryReader();

  // Returns a pointer to the interface implementing PPB_DirectoryReader that
  // is exposed to the plugin.
  static const PPB_DirectoryReader* GetInterface();

  // Resource overrides.
  DirectoryReader* AsDirectoryReader() { return this; }

  // PPB_DirectoryReader implementation.
  int32_t GetNextEntry(PP_DirectoryEntry* entry,
                       PP_CompletionCallback callback);

 private:
  scoped_refptr<FileRef> directory_ref_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_DIRECTORY_READER_H_
