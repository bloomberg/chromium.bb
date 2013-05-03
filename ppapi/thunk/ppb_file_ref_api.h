// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FILE_REF_API_H_
#define PPAPI_THUNK_PPB_FILE_REF_API_H_

#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

struct PPB_FileRef_CreateInfo;
class TrackedCallback;

namespace thunk {

class PPAPI_THUNK_EXPORT PPB_FileRef_API {
 public:
  virtual ~PPB_FileRef_API() {}

  virtual PP_FileSystemType GetFileSystemType() const = 0;
  virtual PP_Var GetName() const = 0;
  virtual PP_Var GetPath() const = 0;
  virtual PP_Resource GetParent() = 0;
  virtual int32_t MakeDirectory(PP_Bool make_ancestors,
                                scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Touch(PP_Time last_access_time,
                        PP_Time last_modified_time,
                        scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Delete(scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Rename(PP_Resource new_file_ref,
                         scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Query(PP_FileInfo* info,
                        scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t ReadDirectoryEntries(
      const PP_ArrayOutput& output,
      scoped_refptr<TrackedCallback> callback) = 0;
  // We define variants of Query and ReadDirectoryEntries because
  // 1. we need to take linked_ptr instead of raw pointers to avoid
  // use-after-free, and 2. we don't use PP_ArrayOutput for the
  // communication between renderers and the browser in
  // ReadDirectoryEntries. The *InHost functions must not be called in
  // plugins, and Query and ReadDirectoryEntries must not be called in
  // renderers.
  // TODO(hamaji): These functions must be removed when we move
  // FileRef to the new resource design. http://crbug.com/225441
  virtual int32_t QueryInHost(linked_ptr<PP_FileInfo> info,
                              scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t ReadDirectoryEntriesInHost(
      linked_ptr<std::vector<ppapi::PPB_FileRef_CreateInfo> > files,
      linked_ptr<std::vector<PP_FileType> > file_types,
      scoped_refptr<TrackedCallback> callback) = 0;

  // Internal function for use in proxying. Returns the internal CreateInfo
  // (the contained resource does not carry a ref on behalf of the caller).
  virtual const PPB_FileRef_CreateInfo& GetCreateInfo() const = 0;

  // Private API
  virtual PP_Var GetAbsolutePath() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FILE_REF_API_H_
