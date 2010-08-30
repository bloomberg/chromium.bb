// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_FILE_CHOOSER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_FILE_CHOOSER_H_

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "third_party/ppapi/c/dev/ppb_file_chooser_dev.h"
#include "third_party/ppapi/c/pp_completion_callback.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

class PluginDelegate;
class PluginInstance;

class FileChooser : public Resource {
 public:
  FileChooser(PluginInstance* instance,
              const PP_FileChooserOptions_Dev* options);
  virtual ~FileChooser();

  // Returns a pointer to the interface implementing PPB_FileChooser that is
  // exposed to the plugin.
  static const PPB_FileChooser_Dev* GetInterface();

  // Resource overrides.
  FileChooser* AsFileChooser() { return this; }

  // Stores the list of selected files.
  void StoreChosenFiles(const std::vector<std::string>& files);

  // PPB_FileChooser implementation.
  int32_t Show(PP_CompletionCallback callback);
  scoped_refptr<FileRef> GetNextChosenFile();

 private:
  PluginDelegate* delegate_;
  PP_FileChooserMode_Dev mode_;
  std::string accept_mime_types_;
  PP_CompletionCallback completion_callback_;
  std::vector< scoped_refptr<FileRef> > chosen_files_;
  size_t next_chosen_file_index_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_FILE_CHOOSER_H_
