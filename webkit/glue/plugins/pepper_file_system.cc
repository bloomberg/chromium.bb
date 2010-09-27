// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_file_system.h"

#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/weak_ptr.h"
#include "third_party/ppapi/c/dev/ppb_file_system_dev.h"
#include "third_party/ppapi/c/pp_completion_callback.h"
#include "third_party/ppapi/c/pp_time.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/glue/plugins/pepper_resource.h"
#include "webkit/glue/plugins/pepper_error_util.h"
#include "webkit/glue/plugins/pepper_file_ref.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

namespace {

// Instances of this class are deleted when RunCallback() is called.
class StatusCallback : public fileapi::FileSystemCallbackDispatcher {
 public:
  StatusCallback(base::WeakPtr<pepper::PluginModule> module,
                 PP_CompletionCallback callback)
      : module_(module),
        callback_(callback) {
  }

  // FileSystemCallbackDispatcher implementation.
  virtual void DidSucceed() {
    RunCallback(base::PLATFORM_FILE_OK);
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo&) {
    NOTREACHED();
  }

  virtual void DidReadDirectory(
      const std::vector<base::file_util_proxy::Entry>&, bool) {
    NOTREACHED();
  }

  virtual void DidOpenFileSystem(const std::string&, const FilePath&) {
    NOTREACHED();
  }

  virtual void DidFail(base::PlatformFileError error_code) {
    RunCallback(error_code);
  }

 private:
  void RunCallback(base::PlatformFileError error_code) {
    if (!module_.get() || !callback_.func)
      return;

    PP_RunCompletionCallback(
        &callback_, pepper::PlatformFileErrorToPepperError(error_code));

    delete this;
  }

  base::WeakPtr<pepper::PluginModule> module_;
  PP_CompletionCallback callback_;
};

// Instances of this class are deleted when RunCallback() is called.
class QueryInfoCallback : public fileapi::FileSystemCallbackDispatcher {
 public:
  QueryInfoCallback(base::WeakPtr<pepper::PluginModule> module,
                    PP_CompletionCallback callback,
                    PP_FileInfo_Dev* info,
                    PP_FileSystemType_Dev file_system_type)
      : module_(module),
        callback_(callback),
        info_(info),
        file_system_type_(file_system_type) {
    DCHECK(info_);
  }

  // FileSystemCallbackDispatcher implementation.
  virtual void DidSucceed() {
    NOTREACHED();
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& file_info) {
    RunCallback(base::PLATFORM_FILE_OK, file_info);
  }

  virtual void DidReadDirectory(
      const std::vector<base::file_util_proxy::Entry>&, bool) {
    NOTREACHED();
  }

  virtual void DidOpenFileSystem(const std::string&, const FilePath&) {
    NOTREACHED();
  }

  virtual void DidFail(base::PlatformFileError error_code) {
    RunCallback(error_code, base::PlatformFileInfo());
  }

 private:
  void RunCallback(base::PlatformFileError error_code,
                   const base::PlatformFileInfo& file_info) {
    if (!module_.get() || !callback_.func)
      return;

    if (error_code == base::PLATFORM_FILE_OK) {
      info_->size = file_info.size;
      info_->creation_time = file_info.creation_time.ToDoubleT();
      info_->last_access_time = file_info.last_accessed.ToDoubleT();
      info_->last_modified_time = file_info.last_modified.ToDoubleT();
      info_->system_type = file_system_type_;
      if (file_info.is_directory)
        info_->type = PP_FILETYPE_DIRECTORY;
      else
        info_->type = PP_FILETYPE_REGULAR;
    }
    PP_RunCompletionCallback(
        &callback_, pepper::PlatformFileErrorToPepperError(error_code));

    delete this;
  }

  base::WeakPtr<pepper::PluginModule> module_;
  PP_CompletionCallback callback_;
  PP_FileInfo_Dev* info_;
  PP_FileSystemType_Dev file_system_type_;
};

int32_t MakeDirectory(PP_Resource directory_ref_id,
                      bool make_ancestors,
                      PP_CompletionCallback callback) {
  scoped_refptr<FileRef> directory_ref(
      Resource::GetAs<FileRef>(directory_ref_id));
  if (!directory_ref)
    return PP_ERROR_BADRESOURCE;

  if (directory_ref->file_system_type() == PP_FILESYSTEMTYPE_EXTERNAL)
    return PP_ERROR_FAILED;

  PluginModule* module = directory_ref->module();
  if (!module->GetSomeInstance()->delegate()->MakeDirectory(
          directory_ref->system_path(), make_ancestors,
          new StatusCallback(module->AsWeakPtr(), callback)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t Query(PP_Resource file_ref_id,
              PP_FileInfo_Dev* info,
              PP_CompletionCallback callback) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  PluginModule* module = file_ref->module();
  if (!module->GetSomeInstance()->delegate()->Query(
          file_ref->system_path(),
          new QueryInfoCallback(module->AsWeakPtr(), callback,
                                info, file_ref->file_system_type())))
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

  PluginModule* module = file_ref->module();
  if (!module->GetSomeInstance()->delegate()->Touch(
          file_ref->system_path(), base::Time::FromDoubleT(last_access_time),
          base::Time::FromDoubleT(last_modified_time),
          new StatusCallback(module->AsWeakPtr(), callback)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

int32_t Delete(PP_Resource file_ref_id,
               PP_CompletionCallback callback) {
  scoped_refptr<FileRef> file_ref(Resource::GetAs<FileRef>(file_ref_id));
  if (!file_ref)
    return PP_ERROR_BADRESOURCE;

  if (file_ref->file_system_type() == PP_FILESYSTEMTYPE_EXTERNAL)
    return PP_ERROR_FAILED;

  PluginModule* module = file_ref->module();
  if (!module->GetSomeInstance()->delegate()->Delete(
          file_ref->system_path(),
          new StatusCallback(module->AsWeakPtr(), callback)))
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

  if ((file_ref->file_system_type() == PP_FILESYSTEMTYPE_EXTERNAL) ||
      (file_ref->file_system_type() != new_file_ref->file_system_type()))
    return PP_ERROR_FAILED;

  PluginModule* module = file_ref->module();
  if (!module->GetSomeInstance()->delegate()->Rename(
          file_ref->system_path(), new_file_ref->system_path(),
          new StatusCallback(module->AsWeakPtr(), callback)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

const PPB_FileSystem_Dev ppb_filesystem = {
  &MakeDirectory,
  &Query,
  &Touch,
  &Delete,
  &Rename
};

}  // namespace

const PPB_FileSystem_Dev* FileSystem::GetInterface() {
  return &ppb_filesystem;
}

}  // namespace pepper
