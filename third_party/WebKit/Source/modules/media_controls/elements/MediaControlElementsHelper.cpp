// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/elements/MediaControlElementsHelper.h"

#include "core/events/Event.h"

namespace blink {

// static
bool MediaControlElementsHelper::IsUserInteractionEvent(Event* event) {
  const AtomicString& type = event->type();
  return type == EventTypeNames::mousedown || type == EventTypeNames::mouseup ||
         type == EventTypeNames::click || type == EventTypeNames::dblclick ||
         event->IsKeyboardEvent() || event->IsTouchEvent();
}

}  // namespace blink
