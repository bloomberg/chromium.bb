// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_DIRECTORY_READER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_DIRECTORY_READER_IMPL_H_

#include <queue>

#include "base/file_util_proxy.h"
#include "webkit/plugins/ppapi/resource.h"

struct PP_CompletionCallback;
struct PP_DirectoryEntry_Dev;
struct PPB_DirectoryReader_Dev;

namespace webkit {
namespace ppapi {

class PPB_FileRef_Impl;

class PPB_DirectoryReader_Impl : public Resource {
 public:
  explicit PPB_DirectoryReader_Impl(PPB_FileRef_Impl* directory_ref);
  virtual ~PPB_DirectoryReader_Impl();

  // Returns a pointer to the interface implementing PPB_DirectoryReader that
  // is exposed to the plugin.
  static const PPB_DirectoryReader_Dev* GetInterface();

  // Resource overrides.
  virtual PPB_DirectoryReader_Impl* AsPPB_DirectoryReader_Impl();

  // PPB_DirectoryReader implementation.
  int32_t GetNextEntry(PP_DirectoryEntry_Dev* entry,
                       PP_CompletionCallback callback);

  void AddNewEntries(const std::vector<base::FileUtilProxy::Entry>& entries,
                     bool has_more);

 private:
  bool FillUpEntry();

  scoped_refptr<PPB_FileRef_Impl> directory_ref_;
  std::queue<base::FileUtilProxy::Entry> entries_;
  bool has_more_;
  PP_DirectoryEntry_Dev* entry_;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_DIRECTORY_READER_IMPL_H_
