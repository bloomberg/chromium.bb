// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_FILE_IO_H_
#define PPAPI_CPP_FILE_IO_H_

#include "ppapi/c/pp_time.h"
#include "ppapi/cpp/resource.h"

struct PP_FileInfo;

namespace pp {

class CompletionCallback;
class FileRef;
class Instance;

class FileIO : public Resource {
 public:
  // Constructs an is_null resource.
  FileIO();

  FileIO(Instance* instance);
  FileIO(const FileIO& other);

    // PPB_FileIO methods:
  int32_t Open(const FileRef& file_ref,
               int32_t open_flags,
               const CompletionCallback& cc);
  int32_t Query(PP_FileInfo* result_buf,
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
};

}  // namespace pp

#endif  // PPAPI_CPP_FILE_IO_H_
