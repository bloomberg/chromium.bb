// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BASE_FILE_PATH_STRING_CONVERSIONS_H_
#define WEBKIT_BASE_FILE_PATH_STRING_CONVERSIONS_H_

#include "base/files/file_path.h"
#include "webkit/base/webkit_base_export.h"

namespace WebKit {
class WebString;
}

namespace webkit_base {

WEBKIT_BASE_EXPORT base::FilePath::StringType WebStringToFilePathString(
    const WebKit::WebString& str);
WEBKIT_BASE_EXPORT WebKit::WebString FilePathStringToWebString(
    const base::FilePath::StringType& str);
WEBKIT_BASE_EXPORT base::FilePath WebStringToFilePath(const WebKit::WebString& str);
WEBKIT_BASE_EXPORT WebKit::WebString FilePathToWebString(
    const base::FilePath& file_path);

}  // namespace webkit_base

#endif  // WEBKIT_BASE_FILE_PATH_STRING_CONVERSIONS_H_
