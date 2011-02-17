// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/file_ref_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_system_dev.h"
#include "ppapi/cpp/module_impl.h"


namespace pp {

namespace {

template <> const char* interface_name<PPB_FileRef_Dev>() {
  return PPB_FILEREF_DEV_INTERFACE;
}

}  // namespace

FileRef_Dev::FileRef_Dev(PP_Resource resource) : Resource(resource) {
}

FileRef_Dev::FileRef_Dev(PassRef, PP_Resource resource) {
  PassRefFromConstructor(resource);
}

FileRef_Dev::FileRef_Dev(const FileSystem_Dev& file_system,
                         const char* path) {
  if (!has_interface<PPB_FileRef_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_FileRef_Dev>()->Create(
      file_system.pp_resource(), path));
}

FileRef_Dev::FileRef_Dev(const FileRef_Dev& other)
    : Resource(other) {
}

PP_FileSystemType_Dev FileRef_Dev::GetFileSystemType() const {
  if (!has_interface<PPB_FileRef_Dev>())
    return PP_FILESYSTEMTYPE_EXTERNAL;
  return get_interface<PPB_FileRef_Dev>()->GetFileSystemType(pp_resource());
}

Var FileRef_Dev::GetName() const {
  if (!has_interface<PPB_FileRef_Dev>())
    return Var();
  return Var(Var::PassRef(),
             get_interface<PPB_FileRef_Dev>()->GetName(pp_resource()));
}

Var FileRef_Dev::GetPath() const {
  if (!has_interface<PPB_FileRef_Dev>())
    return Var();
  return Var(Var::PassRef(),
             get_interface<PPB_FileRef_Dev>()->GetPath(pp_resource()));
}

FileRef_Dev FileRef_Dev::GetParent() const {
  if (!has_interface<PPB_FileRef_Dev>())
    return FileRef_Dev();
  return FileRef_Dev(PassRef(),
                     get_interface<PPB_FileRef_Dev>()->GetParent(
                         pp_resource()));
}

int32_t FileRef_Dev::MakeDirectory(const CompletionCallback& cc) {
  if (!has_interface<PPB_FileRef_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_FileRef_Dev>()->MakeDirectory(
      pp_resource(),
      PP_FALSE,  // make_ancestors
      cc.pp_completion_callback());
}

int32_t FileRef_Dev::MakeDirectoryIncludingAncestors(
    const CompletionCallback& cc) {
  if (!has_interface<PPB_FileRef_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_FileRef_Dev>()->MakeDirectory(
      pp_resource(),
      PP_TRUE,  // make_ancestors
      cc.pp_completion_callback());
}

int32_t FileRef_Dev::Touch(PP_Time last_access_time,
                           PP_Time last_modified_time,
                           const CompletionCallback& cc) {
  if (!has_interface<PPB_FileRef_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_FileRef_Dev>()->Touch(
      pp_resource(), last_access_time, last_modified_time,
      cc.pp_completion_callback());
}

int32_t FileRef_Dev::Delete(const CompletionCallback& cc) {
  if (!has_interface<PPB_FileRef_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_FileRef_Dev>()->Delete(
      pp_resource(), cc.pp_completion_callback());
}

int32_t FileRef_Dev::Rename(const FileRef_Dev& new_file_ref,
                            const CompletionCallback& cc) {
  if (!has_interface<PPB_FileRef_Dev>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_FileRef_Dev>()->Rename(
      pp_resource(), new_file_ref.pp_resource(), cc.pp_completion_callback());
}

}  // namespace pp
