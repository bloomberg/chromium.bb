// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "ppapi/c/pp_errors.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/file_callbacks.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_directory_reader_impl.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/plugins/ppapi/var.h"

namespace webkit {
namespace ppapi {

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
  scoped_refptr<PPB_FileSystem_Impl> file_system(
      Resource::GetAs<PPB_FileSystem_Impl>(file_system_id));
  if (!file_system)
    return 0;

  if (!file_system->instance())
    return 0;

  std::string validated_path(path);
  if (!IsValidLocalPath(validated_path))
    return 0;
  TrimTrailingSlash(&validated_path);

  PPB_FileRef_Impl* file_ref =
      new PPB_FileRef_Impl(file_system->instance(),
                           file_system, validated_path);
  return file_ref->GetReference();
}

PP_Bool IsFileRef(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_FileRef_Impl>(resource));
}

PP_FileSystemType_Dev GetFileSystemType(PP_Resource file_ref_id) {
  scoped_refptr<PPB_FileRef_Impl> file_ref(
      Resource::GetAs<PPB_FileRef_Impl>(file_ref_id));
  if (!file_ref)
    return PP_FILESYSTEMTYPE_EXTERNAL;
  return file_ref->GetFileSystemType();
}

PP_Var GetName(PP_Resource file_ref_id) {
  scoped_refptr<PPB_FileRef_Impl> file_ref(
      Resource::GetAs<PPB_FileRef_Impl>(file_ref_id));
  if (!file_ref)
    return PP_MakeUndefined();
  return StringVar::StringToPPVar(file_ref->instance()->module(),
                                  file_ref->GetName());
}

PP_Var GetPath(PP_Resource file_ref_id) {
  scoped_refptr<PPB_FileRef_Impl> file_ref(
      Resource::GetAs<PPB_FileRef_Impl>(file_ref_id));
  if (!file_ref)
    return PP_MakeUndefined();

  if (file_ref->GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL)
    return PP_MakeUndefined();

  return StringVar::StringToPPVar(file_ref->instance()->module(),
                                  file_ref->GetPath());
}

PP_Resource GetParent(PP_Resource file_ref_id) {
  scoped_refptr<PPB_FileRef_Impl> file_ref(
      Resource::GetAs<PPB_FileRef_Impl>(file_ref_id));
  if (!file_ref)
    return 0;

  if (file_ref->GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL)
    return 0;

  scoped_refptr<PPB_FileRef_Impl> parent_ref(file_ref->GetParent());
  if (!parent_ref)
    return 0;

  return parent_ref->GetReference();
}

int32_t MakeDirectory(PP_Resource directory_ref_id,
                      PP_Bool make_ancestors,
                      PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileRef_Impl> directory_ref(
      Resource::GetAs<PPB_FileRef_Impl>(directory_ref_id));
  if (!directory_ref)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PPB_FileSystem_Impl> file_system =
      directory_ref->GetFileSystem();
  if (!file_system || !file_system->opened() ||
      (file_system->type() == PP_FILESYSTEMTYPE_EXTERNAL))
    return PP_ERROR_NOACCESS;

  PluginInstance* instance = file_system->instance();
  if (!instance->delegate()->MakeDirectory(
          directory_ref->GetSystemPath(), PPBoolToBool(make_ancestors),
          new FileCallbacks(instance->module()->AsWeakPtr(), directory_ref_id,
                            callback, NULL, NULL, NULL)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t Touch(PP_Resource file_ref_id,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileRef_Impl> file_ref(
      Resource::GetAs<PPB_FileRef_Impl>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PPB_FileSystem_Impl> file_system = file_ref->GetFileSystem();
  if (!file_system || !file_system->opened() ||
      (file_system->type() == PP_FILESYSTEMTYPE_EXTERNAL))
    return PP_ERROR_NOACCESS;

  PluginInstance* instance = file_system->instance();
  if (!instance->delegate()->Touch(
          file_ref->GetSystemPath(), base::Time::FromDoubleT(last_access_time),
          base::Time::FromDoubleT(last_modified_time),
          new FileCallbacks(instance->module()->AsWeakPtr(), file_ref_id,
                            callback, NULL, NULL, NULL)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t Delete(PP_Resource file_ref_id,
               PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileRef_Impl> file_ref(
      Resource::GetAs<PPB_FileRef_Impl>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PPB_FileSystem_Impl> file_system = file_ref->GetFileSystem();
  if (!file_system || !file_system->opened() ||
      (file_system->type() == PP_FILESYSTEMTYPE_EXTERNAL))
    return PP_ERROR_NOACCESS;

  PluginInstance* instance = file_system->instance();
  if (!instance->delegate()->Delete(
          file_ref->GetSystemPath(),
          new FileCallbacks(instance->module()->AsWeakPtr(), file_ref_id,
                            callback, NULL, NULL, NULL)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t Rename(PP_Resource file_ref_id,
               PP_Resource new_file_ref_id,
               PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileRef_Impl> file_ref(
      Resource::GetAs<PPB_FileRef_Impl>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PPB_FileRef_Impl> new_file_ref(
      Resource::GetAs<PPB_FileRef_Impl>(new_file_ref_id));
  if (!new_file_ref)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PPB_FileSystem_Impl> file_system = file_ref->GetFileSystem();
  if (!file_system || !file_system->opened() ||
      (file_system != new_file_ref->GetFileSystem()) ||
      (file_system->type() == PP_FILESYSTEMTYPE_EXTERNAL))
    return PP_ERROR_NOACCESS;

  // TODO(viettrungluu): Also cancel when the new file ref is destroyed?
  // http://crbug.com/67624
  PluginInstance* instance = file_system->instance();
  if (!instance->delegate()->Rename(
          file_ref->GetSystemPath(), new_file_ref->GetSystemPath(),
          new FileCallbacks(instance->module()->AsWeakPtr(), file_ref_id,
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
  &Touch,
  &Delete,
  &Rename
};

}  // namespace

PPB_FileRef_Impl::PPB_FileRef_Impl()
    : Resource(NULL),
      file_system_(NULL) {
}

PPB_FileRef_Impl::PPB_FileRef_Impl(
    PluginInstance* instance,
    scoped_refptr<PPB_FileSystem_Impl> file_system,
    const std::string& validated_path)
    : Resource(instance),
      file_system_(file_system),
      virtual_path_(validated_path) {
}

PPB_FileRef_Impl::PPB_FileRef_Impl(PluginInstance* instance,
                                   const FilePath& external_file_path)
    : Resource(instance),
      file_system_(NULL),
      system_path_(external_file_path) {
}

PPB_FileRef_Impl::~PPB_FileRef_Impl() {
}

// static
const PPB_FileRef_Dev* PPB_FileRef_Impl::GetInterface() {
  return &ppb_fileref;
}

PPB_FileRef_Impl* PPB_FileRef_Impl::AsPPB_FileRef_Impl() {
  return this;
}

std::string PPB_FileRef_Impl::GetName() const {
  if (GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL) {
    FilePath::StringType path = system_path_.value();
    size_t pos = path.rfind(FilePath::kSeparators[0]);
    DCHECK(pos != FilePath::StringType::npos);
#if defined(OS_WIN)
    return WideToUTF8(path.substr(pos + 1));
#elif defined(OS_POSIX)
    return path.substr(pos + 1);
#else
#error "Unsupported platform."
#endif
  }

  if (virtual_path_.size() == 1 && virtual_path_[0] == '/')
    return virtual_path_;

  // There should always be a leading slash at least!
  size_t pos = virtual_path_.rfind('/');
  DCHECK(pos != std::string::npos);

  return virtual_path_.substr(pos + 1);
}

scoped_refptr<PPB_FileRef_Impl> PPB_FileRef_Impl::GetParent() {
  if (GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL)
    return new PPB_FileRef_Impl();

  // There should always be a leading slash at least!
  size_t pos = virtual_path_.rfind('/');
  DCHECK(pos != std::string::npos);

  // If the path is "/foo", then we want to include the slash.
  if (pos == 0)
    pos++;
  std::string parent_path = virtual_path_.substr(0, pos);

  PPB_FileRef_Impl* parent_ref = new PPB_FileRef_Impl(instance(), file_system_,
                                                      parent_path);
  return parent_ref;
}

scoped_refptr<PPB_FileSystem_Impl> PPB_FileRef_Impl::GetFileSystem() const {
  return file_system_;
}

PP_FileSystemType_Dev PPB_FileRef_Impl::GetFileSystemType() const {
  if (!file_system_)
    return PP_FILESYSTEMTYPE_EXTERNAL;

  return file_system_->type();
}

std::string PPB_FileRef_Impl::GetPath() const {
  return virtual_path_;
}

FilePath PPB_FileRef_Impl::GetSystemPath() const {
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

}  // namespace ppapi
}  // namespace webkit

