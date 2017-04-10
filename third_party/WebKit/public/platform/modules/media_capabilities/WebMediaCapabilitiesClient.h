// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaCapabilitiesClient_h
#define WebMediaCapabilitiesClient_h

#include <memory>

#include "public/platform/modules/media_capabilities/WebMediaCapabilitiesInfo.h"

namespace blink {

struct WebMediaConfiguration;

// Interface between Blink and the Media layer.
class WebMediaCapabilitiesClient {
 public:
  virtual ~WebMediaCapabilitiesClient() = default;

  virtual void DecodingInfo(
      const WebMediaConfiguration&,
      std::unique_ptr<WebMediaCapabilitiesQueryCallbacks>) = 0;
};

}  // namespace blink

#endif  // WebMediaCapabilitiesClient_h
