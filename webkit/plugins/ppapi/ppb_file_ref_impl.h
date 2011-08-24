// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FILE_REF_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FILE_REF_IMPL_H_

#include <string>

#include "base/file_path.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_file_ref_api.h"

namespace webkit {
namespace ppapi {

class PPB_FileSystem_Impl;
class PluginDelegate;
class PluginModule;

class PPB_FileRef_Impl : public ::ppapi::Resource,
                         public ::ppapi::thunk::PPB_FileRef_API {
 public:
  PPB_FileRef_Impl();
  PPB_FileRef_Impl(PP_Instance instance,
                   scoped_refptr<PPB_FileSystem_Impl> file_system,
                   const std::string& validated_path);
  PPB_FileRef_Impl(PP_Instance instance,
                   const FilePath& external_file_path);
  virtual ~PPB_FileRef_Impl();

  static PP_Resource Create(PP_Resource file_system, const char* path);

  // Resource overrides.
  virtual PPB_FileRef_Impl* AsPPB_FileRef_Impl();

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_FileRef_API* AsPPB_FileRef_API() OVERRIDE;

  // PPB_FileRef_API implementation.
  virtual PP_FileSystemType GetFileSystemType() const OVERRIDE;
  virtual PP_Var GetName() const OVERRIDE;
  virtual PP_Var GetPath() const OVERRIDE;
  virtual PP_Resource GetParent() OVERRIDE;
  virtual int32_t MakeDirectory(PP_Bool make_ancestors,
                                PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Touch(PP_Time last_access_time,
                        PP_Time last_modified_time,
                        PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Delete(PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Rename(PP_Resource new_file_ref,
                         PP_CompletionCallback callback) OVERRIDE;

  PPB_FileSystem_Impl* file_system() const { return file_system_.get(); }

  // Returns the virtual path (i.e., the path that the pepper plugin sees)
  const std::string& virtual_path() const { return virtual_path_; }

  // Returns the system path corresponding to this file.
  FilePath GetSystemPath() const;

  // Returns the FileSystem API URL corresponding to this file.
  GURL GetFileSystemURL() const;

 private:
  // Many mutation functions are allow only to non-external filesystems, This
  // function returns true if the filesystem is opened and isn't external as an
  // access check for these functions.
  bool IsValidNonExternalFileSystem() const;

  scoped_refptr<PPB_FileSystem_Impl> file_system_;
  std::string virtual_path_;  // UTF-8 encoded
  FilePath system_path_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileRef_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FILE_REF_IMPL_H_
