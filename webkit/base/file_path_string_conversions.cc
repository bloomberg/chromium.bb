// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/base/file_path_string_conversions.h"

#include "third_party/WebKit/public/platform/WebString.h"

namespace webkit_base {

base::FilePath::StringType WebStringToFilePathString(
    const WebKit::WebString& str) {
  return base::FilePath::FromUTF16Unsafe(str).value();
}

WebKit::WebString FilePathStringToWebString(
    const base::FilePath::StringType& str) {
  return base::FilePath(str).AsUTF16Unsafe();
}

base::FilePath WebStringToFilePath(const WebKit::WebString& str) {
  return base::FilePath::FromUTF16Unsafe(str);
}

WebKit::WebString FilePathToWebString(const base::FilePath& file_path) {
  return file_path.AsUTF16Unsafe();
}

}  // namespace webkit_base
