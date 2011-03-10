// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/test/test_views_delegate.h"

TestViewsDelegate::TestViewsDelegate() {}
TestViewsDelegate::~TestViewsDelegate() {}

ui::Clipboard* TestViewsDelegate::GetClipboard() const {
  if (!clipboard_.get()) {
    // Note that we need a MessageLoop for the next call to work.
    clipboard_.reset(new ui::Clipboard);
  }
  return clipboard_.get();
}


bool TestViewsDelegate::GetSavedWindowBounds(views::Window* window,
                                             const std::wstring& window_name,
                                             gfx::Rect* bounds) const {
  return false;
}

bool TestViewsDelegate::GetSavedMaximizedState(views::Window* window,
                                               const std::wstring& window_name,
                                               bool* maximized) const {
  return false;
}
