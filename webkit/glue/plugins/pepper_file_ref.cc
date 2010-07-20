// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_file_ref.h"

#include "base/string_util.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_var.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"

namespace pepper {

namespace {

bool IsValidLocalPath(const std::string& path) {
  // The path must start with '/'
  if (path.empty() || path[0] != '/')
    return false;

  // The path must contain valid UTF-8 characters.
  if (!IsStringUTF8(path))
    return false;

  return true;
}

void TrimTrailingSlash(std::string* path) {
  // If this path ends with a slash, then normalize it away unless path is the
  // root path.
  if (path->size() > 1 && path->at(path->size() - 1) == '/')
    path->erase(path->size() - 1, 1);
}

PP_Resource CreateFileRef(PP_Instance instance_id,
                          PP_FileSystemType fs_type,
                          const char* path) {
  PluginInstance* instance = PluginInstance::FromPPInstance(instance_id);
  if (!instance)
    return 0;

  std::string origin;  // TODO(darin): Extract from PluginInstance.

  std::string validated_path(path);
  if (!IsValidLocalPath(validated_path))
    return 0;
  TrimTrailingSlash(&validated_path);

  FileRef* file_ref = new FileRef(instance->module(),
                                  fs_type,
                                  validated_path,
                                  origin);
  return file_ref->GetReference();
}

PP_Resource CreatePersistentFileRef(PP_Instance instance_id, const char* path) {
  return CreateFileRef(instance_id, PP_FILESYSTEMTYPE_LOCALPERSISTENT, path);
}

PP_Resource CreateTemporaryFileRef(PP_Instance instance_id, const char* path) {
  return CreateFileRef(instance_id, PP_FILESYSTEMTYPE_LOCALTEMPORARY, path);
}

bool IsFileRef(PP_Resource resource) {
  return !!Resource::GetAs<FileRef>(resource);
}

PP_FileSystemType GetFileSystemType(PP_Resource file_ref_id) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_FILESYSTEMTYPE_EXTERNAL;

  return file_ref->file_system_type();
}

PP_Var GetName(PP_Resource file_ref_id) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_MakeVoid();

  return StringToPPVar(file_ref->GetName());
}

PP_Var GetPath(PP_Resource file_ref_id) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_MakeVoid();

  if (file_ref->file_system_type() == PP_FILESYSTEMTYPE_EXTERNAL)
    return PP_MakeVoid();

  return StringToPPVar(file_ref->path());
}

PP_Resource GetParent(PP_Resource file_ref_id) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return 0;

  if (file_ref->file_system_type() == PP_FILESYSTEMTYPE_EXTERNAL)
    return 0;

  scoped_refptr<FileRef> parent_ref(file_ref->GetParent());
  if (!parent_ref)
    return 0;

  return parent_ref->GetReference();
}

const PPB_FileRef ppb_fileref = {
  &CreatePersistentFileRef,
  &CreateTemporaryFileRef,
  &IsFileRef,
  &GetFileSystemType,
  &GetName,
  &GetPath,
  &GetParent
};

}  // namespace

FileRef::FileRef(PluginModule* module,
                 PP_FileSystemType file_system_type,
                 const std::string& validated_path,
                 const std::string& origin)
    : Resource(module),
      fs_type_(file_system_type),
      path_(validated_path),
      origin_(origin) {
  // TODO(darin): Need to initialize system_path_.
}

FileRef::~FileRef() {
}

// static
const PPB_FileRef* FileRef::GetInterface() {
  return &ppb_fileref;
}

std::string FileRef::GetName() const {
  if (path_.size() == 1 && path_[0] == '/')
    return path_;

  // There should always be a leading slash at least!
  size_t pos = path_.rfind('/');
  DCHECK(pos != std::string::npos);

  return path_.substr(pos + 1);
}

scoped_refptr<FileRef> FileRef::GetParent() {
  if (path_.size() == 1 && path_[0] == '/')
    return this;

  // There should always be a leading slash at least!
  size_t pos = path_.rfind('/');
  DCHECK(pos != std::string::npos);

  // If the path is "/foo", then we want to include the slash.
  if (pos == 0)
    pos++;
  std::string parent_path = path_.substr(0, pos);

  FileRef* parent_ref = new FileRef(module(), fs_type_, parent_path, origin_);
  return parent_ref;
}

}  // namespace pepper
