// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_WEBPLUGIN_FILE_DELEGATE_H_
#define WEBKIT_GLUE_PLUGINS_WEBPLUGIN_FILE_DELEGATE_H_

#include "base/basictypes.h"
#include "third_party/npapi/bindings/npapi_extensions.h"

namespace webkit_glue {

// Interface for the NPAPI file extensions. This class implements "NOP"
// versions of all these functions so it can be used seamlessly by the
// "regular" plugin delegate while being overridden by the "pepper" one.
class WebPluginFileDelegate {
 public:
  // See NPChooseFilePtr in npapi_extensions.h. Returns true on success, on
  // cancel, returns true but *filename will be filled with an empty FilePath
  // and *handle will be 0.
  virtual bool ChooseFile(const char* mime_types,
                          int mode,
                          NPChooseFileCallback callback,
                          void* user_data) {
    return false;
  }

 protected:
  WebPluginFileDelegate() {}
  virtual ~WebPluginFileDelegate() {}
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_PLUGINS_WEBPLUGIN_FILE_DELEGATE_H_
