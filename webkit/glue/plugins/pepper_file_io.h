// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_FILE_IO_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_FILE_IO_H_

#include "third_party/ppapi/c/pp_time.h"
#include "webkit/glue/plugins/pepper_resource.h"

struct PP_CompletionCallback;
struct PP_FileInfo_Dev;
struct PPB_FileIO_Dev;
struct PPB_FileIOTrusted_Dev;

namespace pepper {

class PluginModule;

class FileIO : public Resource {
 public:
  explicit FileIO(PluginModule* module);
  virtual ~FileIO();

  // Returns a pointer to the interface implementing PPB_FileIO that is exposed
  // to the plugin.
  static const PPB_FileIO_Dev* GetInterface();

  // Returns a pointer to the interface implementing PPB_FileIOTrusted that is
  // exposed to the plugin.
  static const PPB_FileIOTrusted_Dev* GetTrustedInterface();

  // Resource overrides.
  FileIO* AsFileIO() { return this; }

  // PPB_FileIO implementation.
  int32_t Open(FileRef* file_ref,
               int32_t open_flags,
               PP_CompletionCallback callback);
  int32_t Query(PP_FileInfo_Dev* info,
                PP_CompletionCallback callback);
  int32_t Touch(PP_Time last_access_time,
                PP_Time last_modified_time,
                PP_CompletionCallback callback);
  int32_t Read(int64_t offset,
               char* buffer,
               int32_t bytes_to_read,
               PP_CompletionCallback callback);
  int32_t Write(int64_t offset,
                const char* buffer,
                int32_t bytes_to_write,
                PP_CompletionCallback callback);
  int32_t SetLength(int64_t length,
                    PP_CompletionCallback callback);
  int32_t Flush(PP_CompletionCallback callback);
  void Close();

  // PPB_FileIOTrusted implementation.
  int32_t GetOSFileDescriptor();
  int32_t WillWrite(int64_t offset,
                    int32_t bytes_to_write,
                    PP_CompletionCallback callback);
  int32_t WillSetLength(int64_t length,
                        PP_CompletionCallback callback);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_FILE_IO_H_
