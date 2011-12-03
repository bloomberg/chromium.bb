// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/web_io_operators.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

#if defined(WCHAR_T_IS_UTF32)
#include "base/string16.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#endif  // defined(WCHAR_T_IS_UTF32)

namespace WebKit {

#if defined(WCHAR_T_IS_UTF32)
std::ostream& operator<<(std::ostream& out, const WebString& s) {
  return out << static_cast<string16>(s);
}
#endif  // defined(WCHAR_T_IS_UTF32)

std::ostream& operator<<(std::ostream& out, const WebPoint& p) {
  return out << static_cast<gfx::Point>(p).ToString();
}

std::ostream& operator<<(std::ostream& out, const WebRect& p) {
  return out << static_cast<gfx::Rect>(p).ToString();
}

}  // namespace WebKit
