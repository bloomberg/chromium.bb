// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_FILE_REF_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_FILE_REF_H_

#include <string>

#include "base/file_path.h"
#include "third_party/ppapi/c/ppb_file_ref.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

class PluginModule;

class FileRef : public Resource {
 public:
  FileRef(PluginModule* module,
          PP_FileSystemType file_system_type,
          const std::string& validated_path,
          const std::string& origin);
  virtual ~FileRef();

  // Returns a pointer to the interface implementing PPB_FileRef that is
  // exposed to the plugin.
  static const PPB_FileRef* GetInterface();

  // Resource overrides.
  FileRef* AsFileRef() { return this; }

  // PPB_FileRef implementation.
  std::string GetName() const;
  scoped_refptr<FileRef> GetParent();

  PP_FileSystemType file_system_type() const { return fs_type_; }

  // Returns the virtual path (i.e., the path that the pepper plugin sees)
  // corresponding to this file.
  const std::string& path() const { return path_; }

  // Returns the system path corresponding to this file.
  const FilePath& system_path() const { return system_path_; }

 private:
  FilePath system_path_;
  PP_FileSystemType fs_type_;
  std::string path_;  // UTF-8 encoded.
  std::string origin_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_FILE_REF_H_
