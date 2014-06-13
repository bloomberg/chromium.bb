// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_native_widget.h"

#include "base/logging.h"
#import "ui/views/cocoa/bridged_content_view.h"

namespace views {

BridgedNativeWidget::BridgedNativeWidget() {
}

BridgedNativeWidget::~BridgedNativeWidget() {
  SetRootView(NULL);
}

void BridgedNativeWidget::Init(base::scoped_nsobject<NSWindow> window) {
  DCHECK(!window_);
  window_.swap(window);
}

void BridgedNativeWidget::SetRootView(views::View* view) {
  if (view == [bridged_view_ hostedView])
    return;

  [bridged_view_ clearView];
  bridged_view_.reset();
  // Note that there can still be references to the old |bridged_view_|
  // floating around in Cocoa libraries at this point. However, references to
  // the old views::View will be gone, so any method calls will become no-ops.

  if (view) {
    bridged_view_.reset([[BridgedContentView alloc] initWithView:view]);
    // Objective C initializers can return nil. However, if |view| is non-NULL
    // this should be treated as an error and caught early.
    CHECK(bridged_view_);
  }
  [window_ setContentView:bridged_view_];
}

}  // namespace views
