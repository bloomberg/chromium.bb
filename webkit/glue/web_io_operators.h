// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEB_IO_OPERATORS_H_
#define WEBKIT_GLUE_WEB_IO_OPERATORS_H_

#include <iosfwd>

#include "build/build_config.h"

namespace WebKit {

#if defined(WCHAR_T_IS_UTF32)
class WebString;
std::ostream& operator<<(std::ostream& out, const WebString& s);
#endif  // defined(WCHAR_T_IS_UTF32)

struct WebPoint;
std::ostream& operator<<(std::ostream& out, const WebPoint& p);

struct WebRect;
std::ostream& operator<<(std::ostream& out, const WebRect& p);

}  // namespace WebKit

#endif  // WEBKIT_GLUE_WEB_IO_OPERATORS_H_
