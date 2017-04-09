// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaDecodingAbility_h
#define WebMediaDecodingAbility_h

#include "public/platform/WebCallbacks.h"

namespace blink {

// Represents a MediaDecodingAbility dictionary to be used outside of Blink.
// This is set by consumers and send back to Blink.
struct WebMediaDecodingAbility {
  bool supported = false;
  bool smooth = false;
  bool power_efficient = false;
};

using WebMediaCapabilitiesQueryCallbacks =
    WebCallbacks<std::unique_ptr<WebMediaDecodingAbility>, void>;

}  // namespace blink

#endif  // WebMediaDecodingAbility_h
