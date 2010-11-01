// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_FILE_IO_DEV_H_
#define PPAPI_CPP_DEV_FILE_IO_DEV_H_

#include "ppapi/c/pp_time.h"
#include "ppapi/cpp/resource.h"

struct PP_FileInfo_Dev;

namespace pp {

class CompletionCallback;
class FileRef_Dev;

class FileIO_Dev : public Resource {
 public:
  FileIO_Dev();
  FileIO_Dev(const FileIO_Dev& other);

  FileIO_Dev& operator=(const FileIO_Dev& other);
  void swap(FileIO_Dev& other);

  // PPB_FileIO methods:
  int32_t Open(const FileRef_Dev& file_ref,
               int32_t open_flags,
               const CompletionCallback& cc);
  int32_t Query(PP_FileInfo_Dev* result_buf,
                const CompletionCallback& cc);
  int32_t Touch(PP_Time last_access_time,
                PP_Time last_modified_time,
                const CompletionCallback& cc);
  int32_t Read(int64_t offset,
               char* buffer,
               int32_t bytes_to_read,
               const CompletionCallback& cc);
  int32_t Write(int64_t offset,
                const char* buffer,
                int32_t bytes_to_write,
                const CompletionCallback& cc);
  int32_t SetLength(int64_t length,
                    const CompletionCallback& cc);
  int32_t Flush(const CompletionCallback& cc);
  void Close();

  // PPB_FileIOTrusted methods:
  // NOTE: These are only available to trusted plugins and will return
  // PP_ERROR_NOINTERFACE if called from an untrusted plugin.
  int32_t GetOSFileDescriptor();
  int32_t WillWrite(int64_t offset,
                    int32_t bytes_to_write,
                    const CompletionCallback& cc);
  int32_t WillSetLength(int64_t length,
                        const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_FILE_IO_DEV_H_
