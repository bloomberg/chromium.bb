// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/scrollbar/native_scroll_bar.h"
#include "views/controls/scrollbar/native_scroll_bar_views.h"
#include "views/controls/scrollbar/native_scroll_bar_wrapper.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativewScrollBarWrapper, public:

// static
NativeScrollBarWrapper* NativeScrollBarWrapper::CreateWrapper(
    NativeScrollBar* scroll_bar) {
  return new NativeScrollBarViews(scroll_bar);
}

// static
int NativeScrollBarWrapper::GetHorizontalScrollBarHeight() {
  return 20;
}

// static
int NativeScrollBarWrapper::GetVerticalScrollBarWidth() {
  return 20;
}

}  // namespace views
