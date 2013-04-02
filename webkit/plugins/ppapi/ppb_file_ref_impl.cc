// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_file_ref_impl.h"

#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/time_conversion.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_system_api.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/file_callbacks.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_file_system_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::HostResource;
using ppapi::PPB_FileRef_CreateInfo;
using ppapi::PPTimeToTime;
using ppapi::StringVar;
using ppapi::TrackedCallback;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileRef_API;
using ppapi::thunk::PPB_FileSystem_API;

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

#if defined(OS_WIN)
  base::FilePath::StringType path_win(path.begin(), path.end());
  base::FilePath file_path(path_win);
#else
  base::FilePath file_path(path);
#endif
  if (file_path.ReferencesParent())
    return false;

  return true;
}

void TrimTrailingSlash(std::string* path) {
  // If this path ends with a slash, then normalize it away unless path is the
  // root path.
  if (path->size() > 1 && path->at(path->size() - 1) == '/')
    path->erase(path->size() - 1, 1);
}

std::string GetNameForExternalFilePath(const base::FilePath& in_path) {
  const base::FilePath::StringType& path = in_path.value();
  size_t pos = path.rfind(base::FilePath::kSeparators[0]);
  CHECK(pos != base::FilePath::StringType::npos);
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

void IgnoreCloseCallback(base::PlatformFileError error_code) {
}

void GetFileInfoCallback(
    scoped_refptr<base::TaskRunner> task_runner,
    base::PlatformFile file,
    PP_FileInfo* info,
    scoped_refptr<TrackedCallback> callback,
    base::PlatformFileError error_code,
    const base::PlatformFileInfo& file_info) {
  base::FileUtilProxy::Close(
      task_runner, file, base::Bind(&IgnoreCloseCallback));

  if (!TrackedCallback::IsPending(callback))
    return;

  int32_t pp_error = ::ppapi::PlatformFileErrorToPepperError(error_code);
  if (pp_error != PP_OK)
    callback->Run(pp_error);

  info->size = file_info.size;
  if (file_info.is_symbolic_link)
    info->type = PP_FILETYPE_OTHER;
  else if (file_info.is_directory)
    info->type = PP_FILETYPE_DIRECTORY;
  else
    info->type = PP_FILETYPE_REGULAR;

  info->system_type = PP_FILESYSTEMTYPE_EXTERNAL;
  info->creation_time = file_info.creation_time.ToDoubleT();
  info->last_access_time = file_info.last_accessed.ToDoubleT();
  info->last_modified_time = file_info.last_modified.ToDoubleT();
  callback->Run(PP_OK);
}

void QueryCallback(scoped_refptr<base::TaskRunner> task_runner,
                   PP_FileInfo* info,
                   scoped_refptr<TrackedCallback> callback,
                   base::PlatformFileError error_code,
                   base::PassPlatformFile passed_file) {
  if (!TrackedCallback::IsPending(callback))
    return;

  int32_t pp_error = ::ppapi::PlatformFileErrorToPepperError(error_code);
  if (pp_error != PP_OK)
    callback->Run(pp_error);
  base::PlatformFile file = passed_file.ReleaseValue();

  if (!base::FileUtilProxy::GetFileInfoFromPlatformFile(
          task_runner, file,
          base::Bind(&GetFileInfoCallback, task_runner, file, info,
                     callback))) {
    base::FileUtilProxy::Close(
        task_runner, file, base::Bind(&IgnoreCloseCallback));
    callback->Run(PP_ERROR_FAILED);
  }
}

}  // namespace

PPB_FileRef_Impl::PPB_FileRef_Impl(const PPB_FileRef_CreateInfo& info,
                                   PPB_FileSystem_Impl* file_system)
    : PPB_FileRef_Shared(::ppapi::OBJECT_IS_IMPL, info),
      file_system_(file_system),
      external_file_system_path_() {
}

PPB_FileRef_Impl::PPB_FileRef_Impl(const PPB_FileRef_CreateInfo& info,
                                   const base::FilePath& external_file_path)
    : PPB_FileRef_Shared(::ppapi::OBJECT_IS_IMPL, info),
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
    const base::FilePath& external_file_path,
    const std::string& display_name) {
  PPB_FileRef_CreateInfo info;
  info.resource = HostResource::MakeInstanceOnly(instance);
  info.file_system_type = PP_FILESYSTEMTYPE_EXTERNAL;
  if (display_name.empty())
    info.name = GetNameForExternalFilePath(external_file_path);
  else
    info.name = display_name;

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

int32_t PPB_FileRef_Impl::MakeDirectory(
    PP_Bool make_ancestors,
    scoped_refptr<TrackedCallback> callback) {
  if (!IsValidNonExternalFileSystem())
    return PP_ERROR_NOACCESS;

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return PP_ERROR_FAILED;
  if (!plugin_instance->delegate()->MakeDirectory(
          GetFileSystemURL(), PP_ToBool(make_ancestors),
          new FileCallbacks(this, callback, NULL, NULL)))
    return PP_ERROR_FAILED;
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileRef_Impl::Touch(PP_Time last_access_time,
                                PP_Time last_modified_time,
                                scoped_refptr<TrackedCallback> callback) {
  if (!IsValidNonExternalFileSystem())
    return PP_ERROR_NOACCESS;

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return PP_ERROR_FAILED;
  if (!plugin_instance->delegate()->Touch(
          GetFileSystemURL(),
          PPTimeToTime(last_access_time),
          PPTimeToTime(last_modified_time),
          new FileCallbacks(this, callback, NULL, NULL)))
    return PP_ERROR_FAILED;
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileRef_Impl::Delete(scoped_refptr<TrackedCallback> callback) {
  if (!IsValidNonExternalFileSystem())
    return PP_ERROR_NOACCESS;

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return PP_ERROR_FAILED;
  if (!plugin_instance->delegate()->Delete(
          GetFileSystemURL(),
          new FileCallbacks(this, callback, NULL, NULL)))
    return PP_ERROR_FAILED;
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_FileRef_Impl::Rename(PP_Resource new_pp_file_ref,
                                 scoped_refptr<TrackedCallback> callback) {
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
          new FileCallbacks(this, callback, NULL, NULL)))
    return PP_ERROR_FAILED;
  return PP_OK_COMPLETIONPENDING;
}

PP_Var PPB_FileRef_Impl::GetAbsolutePath() {
  if (GetFileSystemType() != PP_FILESYSTEMTYPE_EXTERNAL)
    return GetPath();
  if (!external_path_var_.get()) {
    external_path_var_ = new StringVar(
        external_file_system_path_.AsUTF8Unsafe());
  }
  return external_path_var_->GetPPVar();
}

base::FilePath PPB_FileRef_Impl::GetSystemPath() const {
  if (GetFileSystemType() != PP_FILESYSTEMTYPE_EXTERNAL) {
    NOTREACHED();
    return base::FilePath();
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

  return file_system_->root_url().Resolve(
      net::EscapePath(virtual_path.substr(1)));
}

bool PPB_FileRef_Impl::HasValidFileSystem() const {
  return file_system_ && file_system_->opened();
}

bool PPB_FileRef_Impl::IsValidNonExternalFileSystem() const {
  return file_system_ && file_system_->opened() &&
      file_system_->type() != PP_FILESYSTEMTYPE_EXTERNAL;
}

int32_t PPB_FileRef_Impl::Query(PP_FileInfo* info,
                                scoped_refptr<TrackedCallback> callback) {
  scoped_refptr<PluginInstance> plugin_instance =
      ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance.get())
    return PP_ERROR_FAILED;

  if (!file_system_) {
    // External file system
    // We have to do something totally different for external file systems.

    // TODO(teravest): Use the SequencedWorkerPool instead.
    scoped_refptr<base::TaskRunner> task_runner =
        plugin_instance->delegate()->GetFileThreadMessageLoopProxy();
    if (!plugin_instance->delegate()->AsyncOpenFile(
            GetSystemPath(),
            base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
            base::Bind(&QueryCallback, task_runner, info, callback)))
      return PP_ERROR_FAILED;
  } else {
    // Non-external file system
    if (!HasValidFileSystem())
      return PP_ERROR_NOACCESS;

    if (!plugin_instance->delegate()->Query(
            GetFileSystemURL(),
            new FileCallbacks(this, callback, info, file_system_)))
      return PP_ERROR_FAILED;

  }
  return PP_OK_COMPLETIONPENDING;
}

}  // namespace ppapi
}  // namespace webkit
