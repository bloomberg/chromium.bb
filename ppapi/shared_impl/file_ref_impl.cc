// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/file_ref_impl.h"

#include "base/logging.h"
#include "ppapi/shared_impl/tracker_base.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

FileRefImpl::FileRefImpl(const InitAsImpl&, const PPB_FileRef_CreateInfo& info)
    : Resource(info.resource.instance()),
      create_info_(info) {
  // Should not have been passed a host resource for the trusted constructor.
  DCHECK(info.resource.is_null());

  // Resource's constructor assigned a PP_Resource, so we can fill out our
  // host resource now.
  create_info_.resource = host_resource();
  DCHECK(!create_info_.resource.is_null());
}

FileRefImpl::FileRefImpl(const InitAsProxy&, const PPB_FileRef_CreateInfo& info)
    : Resource(info.resource),
      create_info_(info) {
}

FileRefImpl::~FileRefImpl() {
}

thunk::PPB_FileRef_API* FileRefImpl::AsPPB_FileRef_API() {
  return this;
}

PP_FileSystemType FileRefImpl::GetFileSystemType() const {
  return static_cast<PP_FileSystemType>(create_info_.file_system_type);
}

PP_Var FileRefImpl::GetName() const {
  if (!name_var_.get()) {
    name_var_ = new StringVar(
        TrackerBase::Get()->GetModuleForInstance(pp_instance()),
        create_info_.name);
  }
  return name_var_->GetPPVar();
}

PP_Var FileRefImpl::GetPath() const {
  if (create_info_.file_system_type == PP_FILESYSTEMTYPE_EXTERNAL)
    return PP_MakeUndefined();
  if (!path_var_.get()) {
    path_var_ = new StringVar(
        TrackerBase::Get()->GetModuleForInstance(pp_instance()),
        create_info_.path);
  }
  return path_var_->GetPPVar();
}

const PPB_FileRef_CreateInfo& FileRefImpl::GetCreateInfo() const {
  return create_info_;
}

}  // namespace ppapi
