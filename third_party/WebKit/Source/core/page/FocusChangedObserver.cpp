// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/FocusChangedObserver.h"

#include "core/page/FocusController.h"
#include "core/page/Page.h"

namespace blink {

FocusChangedObserver::FocusChangedObserver(Page* page) {
  DCHECK(page);
  page->GetFocusController().RegisterFocusChangedObserver(this);
}

bool FocusChangedObserver::IsFrameFocused(LocalFrame* frame) {
  if (!frame)
    return false;
  Page* page = frame->GetPage();
  if (!page)
    return false;
  const FocusController& controller = page->GetFocusController();
  return controller.IsFocused() && (controller.FocusedFrame() == frame);
}

}  // namespace blink
