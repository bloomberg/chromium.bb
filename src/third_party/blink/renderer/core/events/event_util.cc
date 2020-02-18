// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/event_util.h"

#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

namespace event_util {

bool IsPointerEventType(const AtomicString& event_type) {
  return event_type == event_type_names::kGotpointercapture ||
         event_type == event_type_names::kLostpointercapture ||
         event_type == event_type_names::kPointercancel ||
         event_type == event_type_names::kPointerdown ||
         event_type == event_type_names::kPointerenter ||
         event_type == event_type_names::kPointerleave ||
         event_type == event_type_names::kPointermove ||
         event_type == event_type_names::kPointerout ||
         event_type == event_type_names::kPointerover ||
         event_type == event_type_names::kPointerup;
}

bool IsDOMMutationEventType(const AtomicString& event_type) {
  return event_type == event_type_names::kDOMCharacterDataModified ||
         event_type == event_type_names::kDOMNodeInserted ||
         event_type == event_type_names::kDOMNodeInsertedIntoDocument ||
         event_type == event_type_names::kDOMNodeRemoved ||
         event_type == event_type_names::kDOMNodeRemovedFromDocument ||
         event_type == event_type_names::kDOMSubtreeModified;
}

}  // namespace event_util

}  // namespace blink
