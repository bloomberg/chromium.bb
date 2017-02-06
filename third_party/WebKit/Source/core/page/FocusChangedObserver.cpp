// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/FocusChangedObserver.h"

#include "core/page/FocusController.h"
#include "core/page/Page.h"

namespace blink {

FocusChangedObserver::FocusChangedObserver(Page* page) {
  DCHECK(page);
  page->focusController().registerFocusChangedObserver(this);
}

bool FocusChangedObserver::isFrameFocused(LocalFrame* frame) {
  if (!frame)
    return false;
  Page* page = frame->page();
  if (!page)
    return false;
  const FocusController& controller = page->focusController();
  return controller.isFocused() && (controller.focusedFrame() == frame);
}

}  // namespace blink
