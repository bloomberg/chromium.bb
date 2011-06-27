// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/file_ref.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/module_impl.h"


namespace pp {

namespace {

template <> const char* interface_name<PPB_FileRef>() {
  return PPB_FILEREF_INTERFACE;
}

}  // namespace

FileRef::FileRef(PP_Resource resource) : Resource(resource) {
}

FileRef::FileRef(PassRef, PP_Resource resource) {
  PassRefFromConstructor(resource);
}

FileRef::FileRef(const FileSystem& file_system,
                 const char* path) {
  if (!has_interface<PPB_FileRef>())
    return;
  PassRefFromConstructor(get_interface<PPB_FileRef>()->Create(
      file_system.pp_resource(), path));
}

FileRef::FileRef(const FileRef& other)
    : Resource(other) {
}

PP_FileSystemType FileRef::GetFileSystemType() const {
  if (!has_interface<PPB_FileRef>())
    return PP_FILESYSTEMTYPE_EXTERNAL;
  return get_interface<PPB_FileRef>()->GetFileSystemType(pp_resource());
}

Var FileRef::GetName() const {
  if (!has_interface<PPB_FileRef>())
    return Var();
  return Var(Var::PassRef(),
             get_interface<PPB_FileRef>()->GetName(pp_resource()));
}

Var FileRef::GetPath() const {
  if (!has_interface<PPB_FileRef>())
    return Var();
  return Var(Var::PassRef(),
             get_interface<PPB_FileRef>()->GetPath(pp_resource()));
}

FileRef FileRef::GetParent() const {
  if (!has_interface<PPB_FileRef>())
    return FileRef();
  return FileRef(PassRef(),
                     get_interface<PPB_FileRef>()->GetParent(
                         pp_resource()));
}

int32_t FileRef::MakeDirectory(const CompletionCallback& cc) {
  if (!has_interface<PPB_FileRef>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_FileRef>()->MakeDirectory(
      pp_resource(),
      PP_FALSE,  // make_ancestors
      cc.pp_completion_callback());
}

int32_t FileRef::MakeDirectoryIncludingAncestors(
    const CompletionCallback& cc) {
  if (!has_interface<PPB_FileRef>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_FileRef>()->MakeDirectory(
      pp_resource(),
      PP_TRUE,  // make_ancestors
      cc.pp_completion_callback());
}

int32_t FileRef::Touch(PP_Time last_access_time,
                       PP_Time last_modified_time,
                       const CompletionCallback& cc) {
  if (!has_interface<PPB_FileRef>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_FileRef>()->Touch(
      pp_resource(), last_access_time, last_modified_time,
      cc.pp_completion_callback());
}

int32_t FileRef::Delete(const CompletionCallback& cc) {
  if (!has_interface<PPB_FileRef>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_FileRef>()->Delete(
      pp_resource(), cc.pp_completion_callback());
}

int32_t FileRef::Rename(const FileRef& new_file_ref,
                        const CompletionCallback& cc) {
  if (!has_interface<PPB_FileRef>())
    return PP_ERROR_NOINTERFACE;
  return get_interface<PPB_FileRef>()->Rename(
      pp_resource(), new_file_ref.pp_resource(), cc.pp_completion_callback());
}

}  // namespace pp
