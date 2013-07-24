// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBKIT_GLUE_H_
#define WEBKIT_GLUE_WEBKIT_GLUE_H_

#include <string>

#include "base/platform_file.h"
#include "webkit/glue/webkit_glue_export.h"

namespace WebKit {
struct WebFileInfo;
}

namespace webkit_glue {

WEBKIT_GLUE_EXPORT void SetJavaScriptFlags(const std::string& flags);

// File info conversion
WEBKIT_GLUE_EXPORT void PlatformFileInfoToWebFileInfo(
    const base::PlatformFileInfo& file_info,
    WebKit::WebFileInfo* web_file_info);

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_WEBKIT_GLUE_H_
