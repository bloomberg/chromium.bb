// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/web_io_operators.h"

#include "gfx/point.h"
#include "gfx/rect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"

#if defined(WCHAR_T_IS_UTF32)
#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#endif  // defined(WCHAR_T_IS_UTF32)

namespace WebKit {

#if defined(WCHAR_T_IS_UTF32)
std::ostream& operator<<(std::ostream& out, const WebString& s) {
  return out << static_cast<string16>(s);
}
#endif  // defined(WCHAR_T_IS_UTF32)

std::ostream& operator<<(std::ostream& out, const WebPoint& p) {
  return out << static_cast<gfx::Point>(p);
}

std::ostream& operator<<(std::ostream& out, const WebRect& p) {
  return out << static_cast<gfx::Rect>(p);
}

}  // namespace WebKit
