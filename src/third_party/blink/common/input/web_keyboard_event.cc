// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_keyboard_event.h"

namespace blink {

std::unique_ptr<WebInputEvent> WebKeyboardEvent::Clone() const {
  return std::make_unique<WebKeyboardEvent>(*this);
}

bool WebKeyboardEvent::CanCoalesce(const WebInputEvent& event) const {
  return false;
}

void WebKeyboardEvent::Coalesce(const WebInputEvent& event) {
  NOTREACHED();
}

}  // namespace blink
