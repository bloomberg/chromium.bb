// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/ScrollCustomizationCallbacks.h"

#include "core/page/scrolling/ScrollStateCallback.h"

namespace blink {

void ScrollCustomizationCallbacks::SetDistributeScroll(
    Element* element,
    ScrollStateCallback* scroll_state_callback) {
  distribute_scroll_callbacks_.Set(element, scroll_state_callback);
}

ScrollStateCallback* ScrollCustomizationCallbacks::GetDistributeScroll(
    Element* element) {
  auto it = distribute_scroll_callbacks_.find(element);
  if (it == distribute_scroll_callbacks_.end())
    return nullptr;
  return it->value.Get();
}

void ScrollCustomizationCallbacks::SetApplyScroll(
    Element* element,
    ScrollStateCallback* scroll_state_callback) {
  apply_scroll_callbacks_.Set(element, scroll_state_callback);
}

void ScrollCustomizationCallbacks::RemoveApplyScroll(Element* element) {
  apply_scroll_callbacks_.erase(element);
}

ScrollStateCallback* ScrollCustomizationCallbacks::GetApplyScroll(
    Element* element) {
  auto it = apply_scroll_callbacks_.find(element);
  if (it == apply_scroll_callbacks_.end())
    return nullptr;
  return it->value.Get();
}

}  // namespace blink
