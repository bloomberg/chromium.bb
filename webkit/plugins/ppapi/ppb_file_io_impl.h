// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FILE_IO_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FILE_IO_IMPL_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/scoped_callback_factory.h"
#include "base/scoped_ptr.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_time.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/resource.h"

struct PP_CompletionCallback;
struct PPB_FileIO_Dev;
struct PPB_FileIOTrusted_Dev;

namespace webkit {
namespace ppapi {

class PluginModule;
class PPB_FileRef_Impl;

class PPB_FileIO_Impl : public Resource {
 public:
  explicit PPB_FileIO_Impl(PluginInstance* instance);
  virtual ~PPB_FileIO_Impl();

  // Returns a pointer to the interface implementing PPB_FileIO that is exposed
  // to the plugin.
  static const PPB_FileIO_Dev* GetInterface();

  // Returns a pointer to the interface implementing PPB_FileIOTrusted that is
  // exposed to the plugin.
  static const PPB_FileIOTrusted_Dev* GetTrustedInterface();

  // Resource overrides.
  virtual PPB_FileIO_Impl* AsPPB_FileIO_Impl();

  // PPB_FileIO implementation.
  int32_t Open(PPB_FileRef_Impl* file_ref,
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

 private:
  // Verifies:
  //  - that |callback| is valid (only nonblocking operation supported);
  //  - that the file is already open or not, depending on |should_be_open|; and
  //  - that no callback is already pending.
  // Returns |PP_OK| to indicate that everything is valid or |PP_ERROR_...| if
  // the call should be aborted and that code returned to the plugin.
  int32_t CommonCallValidation(bool should_be_open,
                               PP_CompletionCallback callback);

  // Sets up |callback| as the pending callback. This should only be called once
  // it is certain that |PP_ERROR_WOULDBLOCK| will be returned.
  void RegisterCallback(PP_CompletionCallback callback);

  void RunPendingCallback(int result);

  void StatusCallback(base::PlatformFileError error_code);
  void AsyncOpenFileCallback(base::PlatformFileError error_code,
                             base::PlatformFile file);
  void QueryInfoCallback(base::PlatformFileError error_code,
                         const base::PlatformFileInfo& file_info);
  void ReadCallback(base::PlatformFileError error_code,
                    const char* data, int bytes_read);
  void WriteCallback(base::PlatformFileError error_code, int bytes_written);

  base::ScopedCallbackFactory<PPB_FileIO_Impl> callback_factory_;

  base::PlatformFile file_;
  PP_FileSystemType_Dev file_system_type_;

  // Any pending callback for any PPB_FileIO(Trusted) call taking a callback.
  scoped_refptr<TrackedCompletionCallback> callback_;

  // Output buffer pointer for |Query()|; only non-null when a callback is
  // pending for it.
  PP_FileInfo_Dev* info_;

  // Pointer back to the caller's read buffer; used by |Read()|. Not owned.
  char* read_buffer_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileIO_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FILE_IO_IMPL_H_
