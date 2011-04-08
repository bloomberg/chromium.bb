// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FILE_REF_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FILE_REF_IMPL_H_

#include <string>

#include "base/file_path.h"
#include "ppapi/c/dev/ppb_file_ref_dev.h"
#include "webkit/plugins/ppapi/resource.h"

namespace webkit {
namespace ppapi {

class PPB_FileSystem_Impl;
class PluginInstance;
class PluginModule;

class PPB_FileRef_Impl : public Resource {
 public:
  PPB_FileRef_Impl();
  PPB_FileRef_Impl(PluginInstance* instance,
                   scoped_refptr<PPB_FileSystem_Impl> file_system,
                   const std::string& validated_path);
  PPB_FileRef_Impl(PluginInstance* instance,
                   const FilePath& external_file_path);
  virtual ~PPB_FileRef_Impl();

  // Returns a pointer to the interface implementing PPB_FileRef that is
  // exposed to the plugin.
  static const PPB_FileRef_Dev* GetInterface();

  // Resource overrides.
  virtual PPB_FileRef_Impl* AsPPB_FileRef_Impl();

  // PPB_FileRef implementation.
  std::string GetName() const;
  scoped_refptr<PPB_FileRef_Impl> GetParent();

  // Returns the file system to which this PPB_FileRef_Impl belongs.
  scoped_refptr<PPB_FileSystem_Impl> GetFileSystem() const;

  // Returns the type of the file system to which this PPB_FileRef_Impl belongs.
  PP_FileSystemType_Dev GetFileSystemType() const;

  // Returns the virtual path (i.e., the path that the pepper plugin sees)
  // corresponding to this file.
  std::string GetPath() const;

  // Returns the system path corresponding to this file.
  FilePath GetSystemPath() const;

 private:
  scoped_refptr<PPB_FileSystem_Impl> file_system_;
  std::string virtual_path_;  // UTF-8 encoded
  FilePath system_path_;

  DISALLOW_COPY_AND_ASSIGN(PPB_FileRef_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FILE_REF_IMPL_H_
