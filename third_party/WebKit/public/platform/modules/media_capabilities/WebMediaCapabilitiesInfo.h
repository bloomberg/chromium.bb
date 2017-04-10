// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaCapabilitiesInfo_h
#define WebMediaCapabilitiesInfo_h

#include "public/platform/WebCallbacks.h"

namespace blink {

// Represents a MediaCapabilitiesInfo dictionary to be used outside of Blink.
// This is set by consumers and sent back to Blink.
struct WebMediaCapabilitiesInfo {
  bool supported = false;
  bool smooth = false;
  bool power_efficient = false;
};

using WebMediaCapabilitiesQueryCallbacks =
    WebCallbacks<std::unique_ptr<WebMediaCapabilitiesInfo>, void>;

}  // namespace blink

#endif  // WebMediaCapabilitiesInfo_h
