/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_FILE_IO_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_FILE_IO_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"
#include "ppapi/c/dev/ppb_file_io_dev.h"

namespace fake_browser_ppapi {

// Implements the PPB_FileIO_Dev interface.
class FileIO : public Resource {
 public:
  explicit FileIO(PP_Module module_id)
      : module_id_(module_id), file_desc_(NACL_NO_FILE_DESC) {}
  ~FileIO() {
    // This crashes on a Mac with:
    // Program received signal EXC_BAD_ACCESS, Could not access memory.
    // Reason: KERN_INVALID_ADDRESS at address: 0x00235e0e
#if !defined(NACL_OSX)
    CLOSE(file_desc_);
#endif
  }

  FileIO* AsFileIO() { return this; }

  void set_file_desc(int32_t file_desc) { file_desc_ = file_desc; }
  int32_t file_desc() const { return file_desc_; }

  // Return an interface pointer usable by PPAPI.
  static const PPB_FileIO_Dev* GetInterface();

 private:
  PP_Module module_id_;
  int32_t file_desc_;

  NACL_DISALLOW_COPY_AND_ASSIGN(FileIO);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_FILE_IO_H_
