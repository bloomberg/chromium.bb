// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/FilePathConversion.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebString.h"

namespace blink {

base::FilePath WebStringToFilePath(const WebString& web_string) {
  if (web_string.IsEmpty())
    return base::FilePath();

  String str = web_string;
  if (!str.Is8Bit()) {
    return base::FilePath::FromUTF16Unsafe(
        base::StringPiece16(str.Characters16(), str.length()));
  }

#if defined(OS_POSIX)
  StringUTF8Adaptor utf8(str);
  return base::FilePath::FromUTF8Unsafe(utf8.AsStringPiece());
#else
  const LChar* data8 = str.Characters8();
  return base::FilePath::FromUTF16Unsafe(
      base::string16(data8, data8 + str.length()));
#endif
}

WebString FilePathToWebString(const base::FilePath& path) {
  if (path.empty())
    return WebString();

#if defined(OS_POSIX)
  return WebString::FromUTF8(path.value());
#else
  return WebString::FromUTF16(path.AsUTF16Unsafe());
#endif
}

}  // namespace blink
