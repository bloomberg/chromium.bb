// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/MediaKeysPolicy.h"

namespace blink {

MediaKeysPolicy::MediaKeysPolicy(const MediaKeysPolicyInit& initializer) {
  if (initializer.minHdcpVersion())
    min_hdcp_version_ = initializer.minHdcpVersion();
}

void MediaKeysPolicy::Trace(blink::Visitor* visitor) {}

}  // namespace blink
