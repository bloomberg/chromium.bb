/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_FILE_IO_TRUSTED_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_FILE_IO_TRUSTED_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/dev/ppb_file_io_trusted_dev.h"

namespace fake_browser_ppapi {

// Implements the PPB_FileIOTrusted_Dev interface.
// At this point this class for the interface pointer only and
// is never instantiated. Hence, it does not need to inherit from Resource.
class FileIOTrusted {
 public:
  // Return an interface pointer usable by PPAPI.
  static const PPB_FileIOTrusted_Dev* GetInterface();

 private:
  FileIOTrusted() {}

  NACL_DISALLOW_COPY_AND_ASSIGN(FileIOTrusted);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_FILE_IO_TRUSTED_H_
