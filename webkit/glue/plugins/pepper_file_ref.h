// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_FILE_REF_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_FILE_REF_H_

#include <string>

#include "base/file_path.h"
#include "ppapi/c/dev/ppb_file_ref_dev.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

class FileSystem;
class PluginInstance;
class PluginModule;

class FileRef : public Resource {
 public:
  FileRef();
  FileRef(PluginModule* module,
          scoped_refptr<FileSystem> file_system,
          const std::string& validated_path);
  FileRef(PluginModule* module,
          const FilePath& external_file_path);
  virtual ~FileRef();

  // Returns a pointer to the interface implementing PPB_FileRef that is
  // exposed to the plugin.
  static const PPB_FileRef_Dev* GetInterface();

  // Resource overrides.
  FileRef* AsFileRef() { return this; }

  // PPB_FileRef implementation.
  std::string GetName() const;
  scoped_refptr<FileRef> GetParent();

  // Returns the file system to which this FileRef belongs.
  scoped_refptr<FileSystem> GetFileSystem() const;

  // Returns the type of the file system to which this FileRef belongs.
  PP_FileSystemType_Dev GetFileSystemType() const;

  // Returns the virtual path (i.e., the path that the pepper plugin sees)
  // corresponding to this file.
  std::string GetPath() const;

  // Returns the system path corresponding to this file.
  FilePath GetSystemPath() const;

 private:
  scoped_refptr<FileSystem> file_system_;
  std::string virtual_path_;  // UTF-8 encoded
  FilePath system_path_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_FILE_REF_H_
