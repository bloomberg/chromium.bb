// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_METRICS_H_

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Record explicitly set MatchType option with UMA.
void RecordMatchType(const String& matchType);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_COOKIE_STORE_METRICS_H_
