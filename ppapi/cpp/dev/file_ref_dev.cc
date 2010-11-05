// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/file_ref_dev.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/file_system_dev.h"
#include "ppapi/cpp/module_impl.h"

namespace {

DeviceFuncs<PPB_FileRef_Dev> file_ref_f(PPB_FILEREF_DEV_INTERFACE);

}  // namespace

namespace pp {

FileRef_Dev::FileRef_Dev(PP_Resource resource) : Resource(resource) {
}

FileRef_Dev::FileRef_Dev(PassRef, PP_Resource resource) {
  PassRefFromConstructor(resource);
}

FileRef_Dev::FileRef_Dev(const FileSystem_Dev& file_system,
                         const char* path) {
  if (!file_ref_f)
    return;
  PassRefFromConstructor(file_ref_f->Create(file_system.pp_resource(), path));
}

FileRef_Dev::FileRef_Dev(const FileRef_Dev& other)
    : Resource(other) {
}

FileRef_Dev& FileRef_Dev::operator=(const FileRef_Dev& other) {
  FileRef_Dev copy(other);
  swap(copy);
  return *this;
}

void FileRef_Dev::swap(FileRef_Dev& other) {
  Resource::swap(other);
}

PP_FileSystemType_Dev FileRef_Dev::GetFileSystemType() const {
  if (!file_ref_f)
    return PP_FILESYSTEMTYPE_EXTERNAL;
  return file_ref_f->GetFileSystemType(pp_resource());
}

Var FileRef_Dev::GetName() const {
  if (!file_ref_f)
    return Var();
  return Var(Var::PassRef(), file_ref_f->GetName(pp_resource()));
}

Var FileRef_Dev::GetPath() const {
  if (!file_ref_f)
    return Var();
  return Var(Var::PassRef(), file_ref_f->GetPath(pp_resource()));
}

FileRef_Dev FileRef_Dev::GetParent() const {
  if (!file_ref_f)
    return FileRef_Dev();
  return FileRef_Dev(PassRef(), file_ref_f->GetParent(pp_resource()));
}

int32_t FileRef_Dev::MakeDirectory(const CompletionCallback& cc) {
  if (!file_ref_f)
    return PP_ERROR_NOINTERFACE;
  return file_ref_f->MakeDirectory(pp_resource(),
                                   PP_FALSE,  // make_ancestors
                                   cc.pp_completion_callback());
}

int32_t FileRef_Dev::MakeDirectoryIncludingAncestors(
    const CompletionCallback& cc) {
  if (!file_ref_f)
    return PP_ERROR_NOINTERFACE;
  return file_ref_f->MakeDirectory(pp_resource(),
                                   PP_TRUE,  // make_ancestors
                                   cc.pp_completion_callback());
}

int32_t FileRef_Dev::Query(PP_FileInfo_Dev* result_buf,
                           const CompletionCallback& cc) {
  if (!file_ref_f)
    return PP_ERROR_NOINTERFACE;
  return file_ref_f->Query(pp_resource(),
                           result_buf,
                           cc.pp_completion_callback());
}

int32_t FileRef_Dev::Touch(PP_Time last_access_time,
                           PP_Time last_modified_time,
                           const CompletionCallback& cc) {
  if (!file_ref_f)
    return PP_ERROR_NOINTERFACE;
  return file_ref_f->Touch(pp_resource(),
                           last_access_time,
                           last_modified_time,
                           cc.pp_completion_callback());
}

int32_t FileRef_Dev::Delete(const CompletionCallback& cc) {
  if (!file_ref_f)
    return PP_ERROR_NOINTERFACE;
  return file_ref_f->Delete(pp_resource(), cc.pp_completion_callback());
}

int32_t FileRef_Dev::Rename(const FileRef_Dev& new_file_ref,
                            const CompletionCallback& cc) {
  if (!file_ref_f)
    return PP_ERROR_NOINTERFACE;
  return file_ref_f->Rename(pp_resource(),
                            new_file_ref.pp_resource(),
                            cc.pp_completion_callback());
}

}  // namespace pp
