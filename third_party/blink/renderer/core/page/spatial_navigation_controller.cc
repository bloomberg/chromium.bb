// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"

#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

namespace {

WebFocusType FocusDirectionForKey(KeyboardEvent* event) {
  if (event->ctrlKey() || event->metaKey() || event->shiftKey())
    return kWebFocusTypeNone;

  WebFocusType ret_val = kWebFocusTypeNone;
  if (event->key() == "ArrowDown")
    ret_val = kWebFocusTypeDown;
  else if (event->key() == "ArrowUp")
    ret_val = kWebFocusTypeUp;
  else if (event->key() == "ArrowLeft")
    ret_val = kWebFocusTypeLeft;
  else if (event->key() == "ArrowRight")
    ret_val = kWebFocusTypeRight;

  // TODO(bokan): We should probably assert that we don't get anything else but
  // currently KeyboardEventManager sends non-arrow keys here.

  return ret_val;
}

}  // namespace

// static
SpatialNavigationController* SpatialNavigationController::Create(Page& page) {
  return MakeGarbageCollected<SpatialNavigationController>(page);
}

SpatialNavigationController::SpatialNavigationController(Page& page)
    : page_(&page) {}

bool SpatialNavigationController::HandleArrowKeyboardEvent(
    KeyboardEvent* event) {
  DCHECK(page_->GetSettings().GetSpatialNavigationEnabled());

  WebFocusType type = FocusDirectionForKey(event);
  if (type == kWebFocusTypeNone)
    return false;

  return page_->GetFocusController().AdvanceFocus(type);
}

void SpatialNavigationController::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
}

}  // namespace blink
