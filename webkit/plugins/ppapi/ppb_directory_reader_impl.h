// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_DIRECTORY_READER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_DIRECTORY_READER_IMPL_H_

#include <queue>
#include <vector>

#include "base/file_util_proxy.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_directory_reader_api.h"

struct PP_CompletionCallback;
struct PP_DirectoryEntry_Dev;

namespace webkit {
namespace ppapi {

class PPB_FileRef_Impl;

class PPB_DirectoryReader_Impl
    : public ::ppapi::Resource,
      public ::ppapi::thunk::PPB_DirectoryReader_API {
 public:
  explicit PPB_DirectoryReader_Impl(PPB_FileRef_Impl* directory_ref);
  virtual ~PPB_DirectoryReader_Impl();

  static PP_Resource Create(PP_Resource directory_ref);

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_DirectoryReader_API* AsPPB_DirectoryReader_API()
      OVERRIDE;

  // PPB_DirectoryReader_API implementation.
  virtual int32_t GetNextEntry(PP_DirectoryEntry_Dev* entry,
                               PP_CompletionCallback callback) OVERRIDE;

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
