// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_FILE_CHOOSER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_FILE_CHOOSER_H_

#include <string>

#include "third_party/ppapi/c/ppb_file_chooser.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

class PluginInstance;

class FileChooser : public Resource {
 public:
  FileChooser(PluginInstance* instance, const PP_FileChooserOptions* options);
  virtual ~FileChooser();

  // Returns a pointer to the interface implementing PPB_FileChooser that is
  // exposed to the plugin.
  static const PPB_FileChooser* GetInterface();

  // Resource overrides.
  FileChooser* AsFileChooser() { return this; }

  // PPB_FileChooser implementation.
  int32_t Show(PP_CompletionCallback callback);
  scoped_refptr<FileRef> GetNextChosenFile();

 private:
  PP_FileChooserMode mode_;
  std::string accept_mime_types_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_FILE_CHOOSER_H_
