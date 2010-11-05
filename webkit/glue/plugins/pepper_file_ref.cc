// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_file_ref.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "ppapi/c/pp_errors.h"
#include "webkit/glue/plugins/pepper_common.h"
#include "webkit/glue/plugins/pepper_directory_reader.h"
#include "webkit/glue/plugins/pepper_file_callbacks.h"
#include "webkit/glue/plugins/pepper_file_system.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_var.h"

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

PP_Resource Create(PP_Resource file_system_id, const char* path) {
  scoped_refptr<FileSystem> file_system(
      Resource::GetAs<FileSystem>(file_system_id));
  if (!file_system)
    return 0;

  if (!file_system->instance())
    return 0;

  std::string validated_path(path);
  if (!IsValidLocalPath(validated_path))
    return 0;
  TrimTrailingSlash(&validated_path);

  FileRef* file_ref = new FileRef(file_system->instance()->module(),
                                  file_system,
                                  validated_path);
  return file_ref->GetReference();
}

PP_Bool IsFileRef(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<FileRef>(resource));
}

PP_FileSystemType_Dev GetFileSystemType(PP_Resource file_ref_id) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_FILESYSTEMTYPE_EXTERNAL;
  return file_ref->GetFileSystemType();
}

PP_Var GetName(PP_Resource file_ref_id) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_MakeUndefined();
  return StringVar::StringToPPVar(file_ref->module(), file_ref->GetName());
}

PP_Var GetPath(PP_Resource file_ref_id) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_MakeUndefined();

  if (file_ref->GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL)
    return PP_MakeUndefined();

  return StringVar::StringToPPVar(file_ref->module(), file_ref->GetPath());
}

PP_Resource GetParent(PP_Resource file_ref_id) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return 0;

  if (file_ref->GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL)
    return 0;

  scoped_refptr<FileRef> parent_ref(file_ref->GetParent());
  if (!parent_ref)
    return 0;

  return parent_ref->GetReference();
}

int32_t MakeDirectory(PP_Resource directory_ref_id,
                      PP_Bool make_ancestors,
                      PP_CompletionCallback callback) {
  scoped_refptr<FileRef> directory_ref(
      Resource::GetAs<FileRef>(directory_ref_id));
  if (!directory_ref)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<FileSystem> file_system = directory_ref->GetFileSystem();
  if (!file_system || !file_system->opened() ||
      (file_system->type() == PP_FILESYSTEMTYPE_EXTERNAL))
    return PP_ERROR_NOACCESS;

  PluginInstance* instance = file_system->instance();
  if (!instance->delegate()->MakeDirectory(
          directory_ref->GetSystemPath(), PPBoolToBool(make_ancestors),
          new FileCallbacks(instance->module()->AsWeakPtr(),
                            callback, NULL, NULL, NULL)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t Query(PP_Resource file_ref_id,
              PP_FileInfo_Dev* info,
              PP_CompletionCallback callback) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<FileSystem> file_system = file_ref->GetFileSystem();
  if (!file_system || !file_system->opened() ||
      (file_system->type() == PP_FILESYSTEMTYPE_EXTERNAL))
    return PP_ERROR_NOACCESS;

  PluginInstance* instance = file_system->instance();
  if (!instance->delegate()->Query(
          file_ref->GetSystemPath(),
          new FileCallbacks(instance->module()->AsWeakPtr(),
                            callback, info, file_system, NULL)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t Touch(PP_Resource file_ref_id,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              PP_CompletionCallback callback) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<FileSystem> file_system = file_ref->GetFileSystem();
  if (!file_system || !file_system->opened() ||
      (file_system->type() == PP_FILESYSTEMTYPE_EXTERNAL))
    return PP_ERROR_NOACCESS;

  PluginInstance* instance = file_system->instance();
  if (!instance->delegate()->Touch(
          file_ref->GetSystemPath(), base::Time::FromDoubleT(last_access_time),
          base::Time::FromDoubleT(last_modified_time),
          new FileCallbacks(instance->module()->AsWeakPtr(),
                            callback, NULL, NULL, NULL)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t Delete(PP_Resource file_ref_id,
               PP_CompletionCallback callback) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<FileSystem> file_system = file_ref->GetFileSystem();
  if (!file_system || !file_system->opened() ||
      (file_system->type() == PP_FILESYSTEMTYPE_EXTERNAL))
    return PP_ERROR_NOACCESS;

  PluginInstance* instance = file_system->instance();
  if (!instance->delegate()->Delete(
          file_ref->GetSystemPath(),
          new FileCallbacks(instance->module()->AsWeakPtr(),
                            callback, NULL, NULL, NULL)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t Rename(PP_Resource file_ref_id,
               PP_Resource new_file_ref_id,
               PP_CompletionCallback callback) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<FileRef> new_file_ref(
      Resource::GetAs<FileRef>(new_file_ref_id));
  if (!new_file_ref)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<FileSystem> file_system = file_ref->GetFileSystem();
  if (!file_system || !file_system->opened() ||
      (file_system != new_file_ref->GetFileSystem()) ||
      (file_system->type() == PP_FILESYSTEMTYPE_EXTERNAL))
    return PP_ERROR_NOACCESS;

  PluginInstance* instance = file_system->instance();
  if (!instance->delegate()->Rename(
          file_ref->GetSystemPath(), new_file_ref->GetSystemPath(),
          new FileCallbacks(instance->module()->AsWeakPtr(),
                            callback, NULL, NULL, NULL)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

const PPB_FileRef_Dev ppb_fileref = {
  &Create,
  &IsFileRef,
  &GetFileSystemType,
  &GetName,
  &GetPath,
  &GetParent,
  &MakeDirectory,
  &Query,
  &Touch,
  &Delete,
  &Rename
};

}  // namespace

FileRef::FileRef()
    : Resource(NULL),
      file_system_(NULL) {
}

FileRef::FileRef(PluginModule* module,
                 scoped_refptr<FileSystem> file_system,
                 const std::string& validated_path)
    : Resource(module),
      file_system_(file_system),
      virtual_path_(validated_path) {
}

FileRef::FileRef(PluginModule* module,
                 const FilePath& external_file_path)
    : Resource(module),
      file_system_(NULL),
      system_path_(external_file_path) {
}

FileRef::~FileRef() {
}

// static
const PPB_FileRef_Dev* FileRef::GetInterface() {
  return &ppb_fileref;
}

std::string FileRef::GetName() const {
  if (GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL)
    return std::string();

  if (virtual_path_.size() == 1 && virtual_path_[0] == '/')
    return virtual_path_;

  // There should always be a leading slash at least!
  size_t pos = virtual_path_.rfind('/');
  DCHECK(pos != std::string::npos);

  return virtual_path_.substr(pos + 1);
}

scoped_refptr<FileRef> FileRef::GetParent() {
  if (GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL)
    return new FileRef();

  // There should always be a leading slash at least!
  size_t pos = virtual_path_.rfind('/');
  DCHECK(pos != std::string::npos);

  // If the path is "/foo", then we want to include the slash.
  if (pos == 0)
    pos++;
  std::string parent_path = virtual_path_.substr(0, pos);

  FileRef* parent_ref = new FileRef(module(), file_system_, parent_path);
  return parent_ref;
}

scoped_refptr<FileSystem> FileRef::GetFileSystem() const {
  return file_system_;
}

PP_FileSystemType_Dev FileRef::GetFileSystemType() const {
  if (!file_system_)
    return PP_FILESYSTEMTYPE_EXTERNAL;

  return file_system_->type();
}

std::string FileRef::GetPath() const {
  return virtual_path_;
}

FilePath FileRef::GetSystemPath() const {
  if (GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL)
    return system_path_;

  // Since |virtual_path_| starts with a '/', it is considered an absolute path
  // on POSIX systems. We need to remove the '/' before calling Append() or we
  // will run into a DCHECK.
  FilePath virtual_file_path(
#if defined(OS_WIN)
      UTF8ToWide(virtual_path_.substr(1))
#elif defined(OS_POSIX)
      virtual_path_.substr(1)
#else
#error "Unsupported platform."
#endif
  );
  return file_system_->root_path().Append(virtual_file_path);
}

}  // namespace pepper
