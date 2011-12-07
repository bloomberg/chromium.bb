// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_file_ref_shared.h"

#include "base/logging.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

PPB_FileRef_Shared::PPB_FileRef_Shared(const InitAsImpl&,
                                       const PPB_FileRef_CreateInfo& info)
    : Resource(info.resource.instance()),
      create_info_(info) {
  // Should not have been passed a host resource for the trusted constructor.
  DCHECK(info.resource.is_null());

  // Resource's constructor assigned a PP_Resource, so we can fill out our
  // host resource now.
  create_info_.resource = host_resource();
  DCHECK(!create_info_.resource.is_null());
}

PPB_FileRef_Shared::PPB_FileRef_Shared(const InitAsProxy&,
                                       const PPB_FileRef_CreateInfo& info)
    : Resource(info.resource),
      create_info_(info) {
}

PPB_FileRef_Shared::~PPB_FileRef_Shared() {
}

thunk::PPB_FileRef_API* PPB_FileRef_Shared::AsPPB_FileRef_API() {
  return this;
}

PP_FileSystemType PPB_FileRef_Shared::GetFileSystemType() const {
  return static_cast<PP_FileSystemType>(create_info_.file_system_type);
}

PP_Var PPB_FileRef_Shared::GetName() const {
  if (!name_var_.get()) {
    name_var_ = new StringVar(
        PpapiGlobals::Get()->GetModuleForInstance(pp_instance()),
        create_info_.name);
  }
  return name_var_->GetPPVar();
}

PP_Var PPB_FileRef_Shared::GetPath() const {
  if (create_info_.file_system_type == PP_FILESYSTEMTYPE_EXTERNAL)
    return PP_MakeUndefined();
  if (!path_var_.get()) {
    path_var_ = new StringVar(
        PpapiGlobals::Get()->GetModuleForInstance(pp_instance()),
        create_info_.path);
  }
  return path_var_->GetPPVar();
}

const PPB_FileRef_CreateInfo& PPB_FileRef_Shared::GetCreateInfo() const {
  return create_info_;
}

}  // namespace ppapi
