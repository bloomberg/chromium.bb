// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_FILE_SYSTEM_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_FILE_SYSTEM_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "webkit/glue/plugins/pepper_resource.h"

struct PPB_FileSystem_Dev;

namespace pepper {

class PluginInstance;

class FileSystem : public Resource {
 public:
  // Returns a pointer to the interface implementing PPB_FileSystem that is
  // exposed to the plugin.
  static const PPB_FileSystem_Dev* GetInterface();

  FileSystem(PluginInstance* instance, PP_FileSystemType_Dev type);
  FileSystem* AsFileSystem() { return this; }

  PluginInstance* instance() { return instance_; }
  PP_FileSystemType_Dev type() { return type_; }
  const FilePath& root_path() const { return root_path_; }
  void set_root_path(const FilePath& root_path) { root_path_ = root_path; }
  bool opened() const { return opened_; }
  void set_opened(bool opened) { opened_ = opened; }

 private:
  PluginInstance* instance_;
  PP_FileSystemType_Dev type_;
  FilePath root_path_;
  bool opened_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_FILE_SYSTEM_H_
