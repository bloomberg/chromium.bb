// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_FILE_REF_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_FILE_REF_SHARED_H_

#include <string>

#include "base/compiler_specific.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_file_ref_api.h"

namespace ppapi {

class StringVar;

// FileRefs are created in a number of places and they include a number of
// return values. This struct encapsulates everything in one place.
struct PPB_FileRef_CreateInfo {
  PPB_FileRef_CreateInfo()
      : file_system_type(PP_FILESYSTEMTYPE_EXTERNAL),
        file_system_plugin_resource(0) {}

  ppapi::HostResource resource;
  int file_system_type;  // One of PP_FileSystemType values.
  std::string path;
  std::string name;

  // Since FileRef needs to hold a FileSystem reference, we need to pass the
  // resource in this CreateInfo.  Note that this is a plugin resource as
  // FileSystem is already in new design.
  PP_Resource file_system_plugin_resource;
};

// This class provides the shared implementation of a FileRef. The functions
// that actually "do stuff" like Touch and MakeDirectory are implemented
// differently for the proxied and non-proxied derived classes.
class PPAPI_SHARED_EXPORT PPB_FileRef_Shared
    : public Resource,
      public thunk::PPB_FileRef_API {
 public:
  PPB_FileRef_Shared(ResourceObjectType type,
                     const PPB_FileRef_CreateInfo& info);
  virtual ~PPB_FileRef_Shared();

  // Resource overrides.
  virtual thunk::PPB_FileRef_API* AsPPB_FileRef_API() OVERRIDE;

  // PPB_FileRef_API implementation (partial).
  virtual PP_FileSystemType GetFileSystemType() const OVERRIDE;
  virtual PP_Var GetName() const OVERRIDE;
  virtual PP_Var GetPath() const OVERRIDE;
  virtual const PPB_FileRef_CreateInfo& GetCreateInfo() const OVERRIDE;
  virtual PP_Var GetAbsolutePath() = 0;

 private:
  PPB_FileRef_CreateInfo create_info_;

  // Lazily initialized vars created from the create_info_. This is so we can
  // return the identical string object every time they're requested.
  mutable scoped_refptr<StringVar> name_var_;
  mutable scoped_refptr<StringVar> path_var_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PPB_FileRef_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_FILE_REF_SHARED_H_
