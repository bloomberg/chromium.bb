// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebClientHintsType_h
#define WebClientHintsType_h

#include "public/platform/web_client_hints_types.mojom-shared.h"

namespace blink {

// WebEnabledClientHints stores all the client hints along with whether the hint
// is enabled or not.
struct WebEnabledClientHints {
  WebEnabledClientHints() {}

  bool IsEnabled(mojom::WebClientHintsType type) const {
    return enabled_types_[static_cast<int>(type)];
  }
  void SetIsEnabled(mojom::WebClientHintsType type, bool should_send) {
    enabled_types_[static_cast<int>(type)] = should_send;
  }

  bool enabled_types_[static_cast<int>(mojom::WebClientHintsType::kLast) + 1] =
      {};
};

}  // namespace blink

#endif  // WebClientHintsType_h
