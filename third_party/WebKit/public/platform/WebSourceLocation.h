// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSourceLocation_h
#define WebSourceLocation_h

#include "public/platform/WebString.h"

namespace blink {

// PlzNavigate
// This struct is passed to the browser when navigating, so that console error
// messages due to the navigation do not lose the source location information.
struct WebSourceLocation {
  WebString url;
  unsigned line_number = 0;
  unsigned column_number = 0;
};

}  // namespace blink

#endif
