// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_file_system_impl.h"

#include "base/ref_counted.h"
#include "ppapi/c/dev/ppb_file_system_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/plugins/ppapi/file_callbacks.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_directory_reader_impl.h"
#include "webkit/plugins/ppapi/resource.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

namespace {

PP_Resource Create(PP_Instance instance, PP_FileSystemType_Dev type) {
  PluginInstance* plugin_instance =
      ResourceTracker::Get()->GetInstance(instance);
  if (!plugin_instance)
    return 0;

  PPB_FileSystem_Impl* file_system =
      new PPB_FileSystem_Impl(plugin_instance, type);
  return file_system->GetReference();
}

int32_t Open(PP_Resource file_system_id,
             int64 expected_size,
             PP_CompletionCallback callback) {
  scoped_refptr<PPB_FileSystem_Impl> file_system(
      Resource::GetAs<PPB_FileSystem_Impl>(file_system_id));
  if (!file_system)
    return PP_ERROR_BADRESOURCE;

  if (file_system->opened())
    return PP_OK;

  if ((file_system->type() != PP_FILESYSTEMTYPE_LOCALPERSISTENT) &&
      (file_system->type() != PP_FILESYSTEMTYPE_LOCALTEMPORARY))
    return PP_ERROR_FAILED;

  PluginInstance* instance = file_system->instance();
  fileapi::FileSystemType file_system_type =
      (file_system->type() == PP_FILESYSTEMTYPE_LOCALTEMPORARY ?
       fileapi::kFileSystemTypeTemporary :
       fileapi::kFileSystemTypePersistent);
  if (!instance->delegate()->OpenFileSystem(
          instance->container()->element().document().frame()->url(),
          file_system_type, expected_size,
          new FileCallbacks(instance->module()->AsWeakPtr(), file_system_id,
                            callback, NULL, file_system, NULL)))
    return PP_ERROR_FAILED;

  return PP_ERROR_WOULDBLOCK;
}

const PPB_FileSystem_Dev ppb_filesystem = {
  &Create,
  &Open
};

}  // namespace

PPB_FileSystem_Impl::PPB_FileSystem_Impl(PluginInstance* instance,
                                         PP_FileSystemType_Dev type)
    : Resource(instance->module()),
      instance_(instance),
      type_(type),
      opened_(false) {
}

PPB_FileSystem_Impl* PPB_FileSystem_Impl::AsPPB_FileSystem_Impl() {
  return this;
}

const PPB_FileSystem_Dev* PPB_FileSystem_Impl::GetInterface() {
  return &ppb_filesystem;
}

}  // namespace ppapi
}  // namespace webkit

