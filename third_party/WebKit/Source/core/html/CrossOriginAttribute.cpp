// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/CrossOriginAttribute.h"

namespace blink {

CrossOriginAttributeValue GetCrossOriginAttributeValue(const String& value) {
  if (value.IsNull())
    return kCrossOriginAttributeNotSet;
  if (DeprecatedEqualIgnoringCase(value, "use-credentials"))
    return kCrossOriginAttributeUseCredentials;
  return kCrossOriginAttributeAnonymous;
}

}  // namespace blink
