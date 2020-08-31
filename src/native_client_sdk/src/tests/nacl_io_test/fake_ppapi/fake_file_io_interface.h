// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_TEST_FAKE_FILE_IO_INTERFACE_H_
#define LIBRARIES_NACL_IO_TEST_FAKE_FILE_IO_INTERFACE_H_

#include "fake_ppapi/fake_core_interface.h"
#include "sdk_util/macros.h"

class FakeFileIoInterface : public nacl_io::FileIoInterface {
 public:
  explicit FakeFileIoInterface(FakeCoreInterface* core_interface);

  virtual PP_Resource Create(PP_Resource instance);
  virtual int32_t Open(PP_Resource file_io,
                       PP_Resource file_ref,
                       int32_t open_flags,
                       PP_CompletionCallback callback);
  virtual int32_t Query(PP_Resource file_io,
                        PP_FileInfo* info,
                        PP_CompletionCallback callback);
  virtual int32_t Read(PP_Resource file_io,
                       int64_t offset,
                       char* buffer,
                       int32_t bytes_to_read,
                       PP_CompletionCallback callback);
  virtual int32_t Write(PP_Resource file_io,
                        int64_t offset,
                        const char* buffer,
                        int32_t bytes_to_write,
                        PP_CompletionCallback callback);
  virtual int32_t SetLength(PP_Resource file_io,
                            int64_t length,
                            PP_CompletionCallback callback);
  virtual int32_t Flush(PP_Resource file_io, PP_CompletionCallback callback);
  virtual void Close(PP_Resource file_io);

 private:
  FakeCoreInterface* core_interface_;  // Weak reference.

  DISALLOW_COPY_AND_ASSIGN(FakeFileIoInterface);
};

#endif  // LIBRARIES_NACL_IO_TEST_FAKE_FILE_IO_INTERFACE_H_
