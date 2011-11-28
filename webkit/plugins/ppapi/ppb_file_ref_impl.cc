// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_system_api.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/var.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/file_callbacks.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_directory_reader_impl.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::HostResource;
using ppapi::PPB_FileRef_CreateInfo;
using ppapi::PPTimeToTime;
using ppapi::StringVar;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileRef_API;
using ppapi::thunk::PPB_FileSystem_API;

namespace webkit {
namespace ppapi {

namespace {

bool IsValidLocalPath(const std::string& path) {
  // The path must start with '/'
  if (path.empty() || path[0] != '/' || path.find("..") != std::string::npos)
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

std::string GetNameForExternalFilePath(const FilePath& in_path) {
  const FilePath::StringType& path = in_path.value();
  size_t pos = path.rfind(FilePath::kSeparators[0]);
  CHECK(pos != FilePath::StringType::npos);
#if defined(OS_WIN)
  return WideToUTF8(path.substr(pos + 1));
#elif defined(OS_POSIX)
  return path.substr(pos + 1);
#else
#error "Unsupported platform."
#endif
}

std::string GetNameForVirtualFilePath(const std::string& path) {
  if (path.size() == 1 && path[0] == '/')
    return path;

  // There should always be a leading slash at least!
  size_t pos = path.rfind('/');
  CHECK(pos != std::string::npos);
  return path.substr(pos + 1);
}

}  // namespace

PPB_FileRef_Impl::PPB_FileRef_Impl(const PPB_FileRef_CreateInfo& info,
                                   PPB_FileSystem_Impl* file_system)
    : FileRefImpl(FileRefImpl::InitAsImpl(), info),
      file_system_(file_system),
      external_file_system_path_() {
}

PPB_FileRef_Impl::PPB_FileRef_Impl(const PPB_FileRef_CreateInfo& info,
                                   const FilePath& external_file_path)
    : FileRefImpl(FileRefImpl::InitAsImpl(), info),
      file_system_(),
      external_file_system_path_(external_file_path) {
}

PPB_FileRef_Impl::~PPB_FileRef_Impl() {
}

// static
PPB_FileRef_Impl* PPB_FileRef_Impl::CreateInternal(PP_Resource pp_file_system,
                                                   const std::string& path) {
  EnterResourceNoLock<PPB_FileSystem_API> enter(pp_file_system, true);
  if (enter.failed())
    return 0;

  PPB_FileSystem_Impl* file_system =
      static_cast<PPB_FileSystem_Impl*>(enter.object());
  if (!file_system->pp_instance())
    return 0;

  if (file_system->type() != PP_FILESYSTEMTYPE_LOCALPERSISTENT &&
      file_system->type() != PP_FILESYSTEMTYPE_LOCALTEMPORARY &&
      file_system->type() != PP_FILESYSTEMTYPE_EXTERNAL)
    return 0;

  PPB_FileRef_CreateInfo info;
  info.resource = HostResource::MakeInstanceOnly(file_system->pp_instance());
  info.file_system_type = file_system->type();

  // Validate the path.
  info.path = path;
  if (!IsValidLocalPath(info.path))
    return 0;
  TrimTrailingSlash(&info.path);

  info.name = GetNameForVirtualFilePath(info.path);

  return new PPB_FileRef_Impl(info, file_system);
}

// static
PPB_FileRef_Impl* PPB_FileRef_Impl::CreateExternal(
    PP_Instance instance,
    const FilePath& external_file_path) {
  PPB_FileRef_CreateInfo info;
  info.resource = HostResource::MakeInstanceOnly(instance);
  info.file_system_type = PP_FILESYSTEMTYPE_EXTERNAL;
  info.name = GetNameForExternalFilePath(external_file_path);

  return new PPB_FileRef_Impl(info, external_file_path);
}

PP_Resource PPB_FileRef_Impl::GetParent() {
  if (GetFileSystemType() == PP_FILESYSTEMTYPE_EXTERNAL)
    return 0;

  const std::string& virtual_path = GetCreateInfo().path;

  // There should always be a leading slash at least!
  size_t pos = virtual_path.rfind('/');
  CHECK(pos != std::string::npos);

  // If the path is "/foo", then we want to include the slash.
  if (pos == 0)
    pos++;
  std::string parent_path = virtual_path.substr(0, pos);

  scoped_refptr<PPB_FileRef_Impl> parent_ref(
      CreateInternal(file_system_->pp_resource(), parent_path));
  if (!parent_ref.get())
    return 0;
  return parent_ref->GetReference();
}

int32_t PPB_FileRef_Impl::MakeDirectory(PP_Bool make_ancestors,
                                        PP_CompletionCallback callback) {
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;
  if (!IsValidNonExternalFileSystem())
    return PP_ERROR_NOACCESS;

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return PP_ERROR_FAILED;
  if (!plugin_instance->delegate()->MakeDirectory(
          GetFileSystemURL(), PP_ToBool(make_ancestors),
          new FileCallbacks(plugin_instance->module()->AsWeakPtr(),
                            pp_resource(), callback, NULL, NULL, NULL)))
    return PP_ERROR_FAILED;
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileRef_Impl::Touch(PP_Time last_access_time,
                                PP_Time last_modified_time,
                                PP_CompletionCallback callback) {
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;
  if (!IsValidNonExternalFileSystem())
    return PP_ERROR_NOACCESS;

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return PP_ERROR_FAILED;
  if (!plugin_instance->delegate()->Touch(
          GetFileSystemURL(),
          PPTimeToTime(last_access_time),
          PPTimeToTime(last_modified_time),
          new FileCallbacks(plugin_instance->module()->AsWeakPtr(),
                            pp_resource(), callback, NULL, NULL, NULL)))
    return PP_ERROR_FAILED;
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileRef_Impl::Delete(PP_CompletionCallback callback) {
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;
  if (!IsValidNonExternalFileSystem())
    return PP_ERROR_NOACCESS;

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return PP_ERROR_FAILED;
  if (!plugin_instance->delegate()->Delete(
          GetFileSystemURL(),
          new FileCallbacks(plugin_instance->module()->AsWeakPtr(),
                            pp_resource(), callback, NULL, NULL, NULL)))
    return PP_ERROR_FAILED;
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileRef_Impl::Rename(PP_Resource new_pp_file_ref,
                                 PP_CompletionCallback callback) {
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;
  EnterResourceNoLock<PPB_FileRef_API> enter(new_pp_file_ref, true);
  if (enter.failed())
    return PP_ERROR_BADRESOURCE;
  PPB_FileRef_Impl* new_file_ref =
      static_cast<PPB_FileRef_Impl*>(enter.object());

  if (!IsValidNonExternalFileSystem() ||
      file_system_.get() != new_file_ref->file_system_.get())
    return PP_ERROR_NOACCESS;

  // TODO(viettrungluu): Also cancel when the new file ref is destroyed?
  // http://crbug.com/67624
  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return PP_ERROR_FAILED;
  if (!plugin_instance->delegate()->Rename(
          GetFileSystemURL(), new_file_ref->GetFileSystemURL(),
          new FileCallbacks(plugin_instance->module()->AsWeakPtr(),
                            pp_resource(), callback, NULL, NULL, NULL)))
    return PP_ERROR_FAILED;
  return PP_OK_COMPLETIONPENDING;
}

PP_Var PPB_FileRef_Impl::GetAbsolutePath() {
  if (GetFileSystemType() != PP_FILESYSTEMTYPE_EXTERNAL)
    return GetPath();
  if (!external_path_var_.get()) {
    PluginModule* plugin_module = ResourceHelper::GetPluginModule(this);
    if (!plugin_module)
      return PP_MakeNull();
    external_path_var_ = new StringVar(
        plugin_module->pp_module(),
        external_file_system_path_.AsUTF8Unsafe());
  }
  return external_path_var_->GetPPVar();
}

FilePath PPB_FileRef_Impl::GetSystemPath() const {
  if (GetFileSystemType() != PP_FILESYSTEMTYPE_EXTERNAL) {
    NOTREACHED();
    return FilePath();
  }
  return external_file_system_path_;
}

GURL PPB_FileRef_Impl::GetFileSystemURL() const {
  if (GetFileSystemType() != PP_FILESYSTEMTYPE_LOCALPERSISTENT &&
      GetFileSystemType() != PP_FILESYSTEMTYPE_LOCALTEMPORARY &&
      GetFileSystemType() != PP_FILESYSTEMTYPE_EXTERNAL) {
    NOTREACHED();
    return GURL();
  }

  const std::string& virtual_path = GetCreateInfo().path;
  CHECK(!virtual_path.empty());  // Should always be at least "/".

  // Since |virtual_path_| starts with a '/', it looks like an absolute path.
  // We need to trim off the '/' before calling Resolve, as FileSystem URLs
  // start with a storage type identifier that looks like a path segment.
  // TODO(ericu): Switch this to use Resolve after fixing GURL to understand
  // FileSystem URLs.
  return GURL(file_system_->root_url().spec() + virtual_path.substr(1));
}

bool PPB_FileRef_Impl::HasValidFileSystem() const {
  return file_system_ && file_system_->opened();
}

bool PPB_FileRef_Impl::IsValidNonExternalFileSystem() const {
  return file_system_ && file_system_->opened() &&
      file_system_->type() != PP_FILESYSTEMTYPE_EXTERNAL;
}

}  // namespace ppapi
}  // namespace webkit
