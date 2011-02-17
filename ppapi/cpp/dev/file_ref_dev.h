// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_FILE_REF_DEV_H_
#define PPAPI_CPP_DEV_FILE_REF_DEV_H_

#include "ppapi/c/dev/ppb_file_ref_dev.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

namespace pp {

class CompletionCallback;
class FileSystem_Dev;

class FileRef_Dev : public Resource {
 public:
  // Creates an is_null() FileRef object.
  FileRef_Dev() {}

  // This constructor is used when we've gotten a PP_Resource as a return value
  // that we need to addref.
  explicit FileRef_Dev(PP_Resource resource);

  // This constructor is used when we've gotten a PP_Resource as a return value
  // that has already been addref'ed for us.
  struct PassRef {};
  FileRef_Dev(PassRef, PP_Resource resource);

  // Creates a FileRef pointing to a path in the given filesystem.
  FileRef_Dev(const FileSystem_Dev& file_system, const char* path);

  FileRef_Dev(const FileRef_Dev& other);

  // Returns the file system type.
  PP_FileSystemType_Dev GetFileSystemType() const;

  // Returns the name of the file.
  Var GetName() const;

  // Returns the absolute path of the file.  See PPB_FileRef::GetPath for more
  // details.
  Var GetPath() const;

  // Returns the parent directory of this file.  See PPB_FileRef::GetParent for
  // more details.
  FileRef_Dev GetParent() const;

  int32_t MakeDirectory(const CompletionCallback& cc);

  int32_t MakeDirectoryIncludingAncestors(const CompletionCallback& cc);

  int32_t Touch(PP_Time last_access_time,
                PP_Time last_modified_time,
                const CompletionCallback& cc);

  int32_t Delete(const CompletionCallback& cc);

  int32_t Rename(const FileRef_Dev& new_file_ref, const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_FILE_REF_H_
