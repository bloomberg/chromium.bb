// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/test/test_views_delegate.h"

#include "base/logging.h"
#include "ui/base/clipboard/clipboard.h"

namespace views {

TestViewsDelegate::TestViewsDelegate()
    : default_parent_view_(NULL) {
  DCHECK(!ViewsDelegate::views_delegate);
  ViewsDelegate::views_delegate = this;
}

TestViewsDelegate::~TestViewsDelegate() {
  ViewsDelegate::views_delegate = NULL;
}

ui::Clipboard* TestViewsDelegate::GetClipboard() const {
  if (!clipboard_.get()) {
    // Note that we need a MessageLoop for the next call to work.
    clipboard_.reset(new ui::Clipboard);
  }
  return clipboard_.get();
}

View* TestViewsDelegate::GetDefaultParentView() {
  return default_parent_view_;
}

bool TestViewsDelegate::GetSavedWindowBounds(const std::wstring& window_name,
                                             gfx::Rect* bounds) const {
  return false;
}

bool TestViewsDelegate::GetSavedMaximizedState(const std::wstring& window_name,
                                               bool* maximized) const {
  return false;
}

int TestViewsDelegate::GetDispositionForEvent(int event_flags) {
  return 0;
}

}  // namespace views
