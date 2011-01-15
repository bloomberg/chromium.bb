// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FILE_CHOOSER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FILE_CHOOSER_IMPL_H_

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "ppapi/c/dev/ppb_file_chooser_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "webkit/plugins/ppapi/resource.h"

namespace webkit {
namespace ppapi {

class PluginDelegate;
class PluginInstance;
class PPB_FileRef_Impl;

class PPB_FileChooser_Impl : public Resource {
 public:
  PPB_FileChooser_Impl(PluginInstance* instance,
                       const PP_FileChooserOptions_Dev* options);
  virtual ~PPB_FileChooser_Impl();

  // Returns a pointer to the interface implementing PPB_FileChooser that is
  // exposed to the plugin.
  static const PPB_FileChooser_Dev* GetInterface();

  // Resource overrides.
  virtual PPB_FileChooser_Impl* AsPPB_FileChooser_Impl();

  // Stores the list of selected files.
  void StoreChosenFiles(const std::vector<std::string>& files);

  // PPB_FileChooser implementation.
  int32_t Show(PP_CompletionCallback callback);
  scoped_refptr<PPB_FileRef_Impl> GetNextChosenFile();

 private:
  PP_FileChooserMode_Dev mode_;
  std::string accept_mime_types_;
  PP_CompletionCallback completion_callback_;
  std::vector< scoped_refptr<PPB_FileRef_Impl> > chosen_files_;
  size_t next_chosen_file_index_;
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FILE_CHOOSER_IMPL_H_
