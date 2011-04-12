// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FILE_SYSTEM_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FILE_SYSTEM_IMPL_H_

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
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
  const GURL& root_url() const { return root_url_; }
  void set_root_url(const GURL& root_url) { root_url_ = root_url; }
  bool opened() const { return opened_; }
  void set_opened(bool opened) { opened_ = opened; }
  bool called_open() const { return called_open_; }
  void set_called_open() { called_open_ = true; }

 private:
  PluginInstance* instance_;
  PP_FileSystemType_Dev type_;
  GURL root_url_;
  bool opened_;
  bool called_open_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileSystem_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FILE_SYSTEM_IMPL_H_
