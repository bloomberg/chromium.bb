// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/base/file_path_string_conversions.h"

#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"

namespace webkit_base {

base::FilePath::StringType WebStringToFilePathString(
    const WebKit::WebString& str) {
#if defined(OS_POSIX)
  return base::SysWideToNativeMB(UTF16ToWideHack(str));
#elif defined(OS_WIN)
  return UTF16ToWideHack(str);
#endif
}

WebKit::WebString FilePathStringToWebString(
    const base::FilePath::StringType& str) {
#if defined(OS_POSIX)
  return WideToUTF16Hack(base::SysNativeMBToWide(str));
#elif defined(OS_WIN)
  return WideToUTF16Hack(str);
#endif
}

base::FilePath WebStringToFilePath(const WebKit::WebString& str) {
  return base::FilePath(WebStringToFilePathString(str));
}

WebKit::WebString FilePathToWebString(const base::FilePath& file_path) {
  return FilePathStringToWebString(file_path.value());
}

}  // namespace webkit_base
