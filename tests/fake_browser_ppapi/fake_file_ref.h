/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_FILE_REF_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_FILE_REF_H_

#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"
#include "ppapi/c/dev/ppb_file_ref_dev.h"

namespace fake_browser_ppapi {

// Implements the PPB_FileRef_Dev interface.
class FileRef : public Resource {
 public:
  explicit FileRef(const std::string& path) : path_(path) {}

  FileRef* AsFileRef() { return this; }

  const std::string& path() const { return path_; }

  // Return an interface pointer usable by PPAPI.
  static const PPB_FileRef_Dev* GetInterface();

 private:
  std::string path_;

  NACL_DISALLOW_COPY_AND_ASSIGN(FileRef);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_FILE_REF_H_
