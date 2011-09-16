// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_file_system_impl.h"

#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_file_system.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/file_callbacks.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_directory_reader_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::thunk::PPB_FileSystem_API;

namespace webkit {
namespace ppapi {

PPB_FileSystem_Impl::PPB_FileSystem_Impl(PP_Instance instance,
                                         PP_FileSystemType type)
    : Resource(instance),
      type_(type),
      opened_(false),
      called_open_(false) {
  DCHECK(type_ != PP_FILESYSTEMTYPE_INVALID);
}

PPB_FileSystem_Impl::~PPB_FileSystem_Impl() {
}

// static
PP_Resource PPB_FileSystem_Impl::Create(PP_Instance instance,
                                        PP_FileSystemType type) {
  if (type != PP_FILESYSTEMTYPE_EXTERNAL &&
      type != PP_FILESYSTEMTYPE_LOCALPERSISTENT &&
      type != PP_FILESYSTEMTYPE_LOCALTEMPORARY)
    return 0;
  return (new PPB_FileSystem_Impl(instance, type))->GetReference();
}

PPB_FileSystem_API* PPB_FileSystem_Impl::AsPPB_FileSystem_API() {
  return this;
}

int32_t PPB_FileSystem_Impl::Open(int64_t expected_size,
                                  PP_CompletionCallback callback) {
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  // Should not allow multiple opens.
  if (called_open_)
    return PP_ERROR_INPROGRESS;
  called_open_ = true;

  if (type_ != PP_FILESYSTEMTYPE_LOCALPERSISTENT &&
      type_ != PP_FILESYSTEMTYPE_LOCALTEMPORARY)
    return PP_ERROR_FAILED;

  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(this);
  if (!plugin_instance)
    return PP_ERROR_FAILED;

  fileapi::FileSystemType file_system_type =
      (type_ == PP_FILESYSTEMTYPE_LOCALTEMPORARY ?
       fileapi::kFileSystemTypeTemporary :
       fileapi::kFileSystemTypePersistent);
  if (!plugin_instance->delegate()->OpenFileSystem(
          plugin_instance->container()->element().document().url(),
          file_system_type, expected_size,
          new FileCallbacks(plugin_instance->module()->AsWeakPtr(),
                            pp_resource(), callback, NULL,
                            scoped_refptr<PPB_FileSystem_Impl>(this), NULL)))
    return PP_ERROR_FAILED;
  return PP_OK_COMPLETIONPENDING;
}

PP_FileSystemType PPB_FileSystem_Impl::GetType() {
  return type_;
}

}  // namespace ppapi
}  // namespace webkit
