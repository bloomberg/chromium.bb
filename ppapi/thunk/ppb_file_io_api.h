// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FILE_IO_API_H_
#define PPAPI_THUNK_PPB_FILE_IO_API_H_

#include "ppapi/c/dev/ppb_file_io_dev.h"

namespace ppapi {
namespace thunk {

class PPB_FileIO_API {
 public:
  virtual ~PPB_FileIO_API() {}

  virtual int32_t Open(PP_Resource file_ref,
                       int32_t open_flags,
                       PP_CompletionCallback callback) = 0;
  virtual int32_t Query(PP_FileInfo_Dev* info,
                        PP_CompletionCallback callback) = 0;
  virtual int32_t Touch(PP_Time last_access_time,
                        PP_Time last_modified_time,
                        PP_CompletionCallback callback) = 0;
  virtual int32_t Read(int64_t offset,
                       char* buffer,
                       int32_t bytes_to_read,
                       PP_CompletionCallback callback) = 0;
  virtual int32_t Write(int64_t offset,
                        const char* buffer,
                        int32_t bytes_to_write,
                        PP_CompletionCallback callback) = 0;
  virtual int32_t SetLength(int64_t length,
                            PP_CompletionCallback callback) = 0;
  virtual int32_t Flush(PP_CompletionCallback callback) = 0;
  virtual void Close() = 0;

  // Trusted API.
  virtual int32_t GetOSFileDescriptor() = 0;
  virtual int32_t WillWrite(int64_t offset,
                            int32_t bytes_to_write,
                            PP_CompletionCallback callback) = 0;
  virtual int32_t WillSetLength(int64_t length,
                                PP_CompletionCallback callback) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FILE_IO_API_H_
