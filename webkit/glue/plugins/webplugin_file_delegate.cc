// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/webplugin_file_delegate.h"

namespace webkit_glue {

bool WebPluginFileDelegate::ChooseFile(const char* mime_types,
                                       int mode,
                                       NPChooseFileCallback callback,
                                       void* user_data) {
  return false;
}

}  // namespace webkit_glue

