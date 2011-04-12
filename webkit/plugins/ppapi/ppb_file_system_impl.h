// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FILE_SYSTEM_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FILE_SYSTEM_IMPL_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "webkit/plugins/ppapi/resource.h"

struct PPB_FileSystem_Dev;

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_FileSystem_Impl : public Resource {
 public:
  // Returns a pointer to the interface implementing PPB_FileSystem that is
  // exposed to the plugin.
  static const PPB_FileSystem_Dev* GetInterface();

  PPB_FileSystem_Impl(PluginInstance* instance, PP_FileSystemType_Dev type);
  virtual PPB_FileSystem_Impl* AsPPB_FileSystem_Impl();

  PluginInstance* instance() { return instance_; }
  PP_FileSystemType_Dev type() { return type_; }
  const FilePath& root_path() const { return root_path_; }
  void set_root_path(const FilePath& root_path) { root_path_ = root_path; }
  bool opened() const { return opened_; }
  void set_opened(bool opened) { opened_ = opened; }
  bool called_open() const { return called_open_; }
  void set_called_open() { called_open_ = true; }

 private:
  PluginInstance* instance_;
  PP_FileSystemType_Dev type_;
  FilePath root_path_;
  bool opened_;
  bool called_open_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileSystem_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FILE_SYSTEM_IMPL_H_
