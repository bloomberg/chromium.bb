// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/web_io_operators.h"

#if defined(WCHAR_T_IS_UTF32)
#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

namespace WebKit {

std::ostream& operator<<(std::ostream& out, const WebString& s) {
  return out << static_cast<string16>(s);
}

}  // namespace WebKit

#endif  // defined(WCHAR_T_IS_UTF32)
