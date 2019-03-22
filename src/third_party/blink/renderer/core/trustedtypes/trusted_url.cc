// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TrustedURL::TrustedURL(const KURL& url) : url_(url) {}

String TrustedURL::toString() const {
  return url_.GetString();
}

}  // namespace blink
