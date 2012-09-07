// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_delegate.h"

namespace aura {

ui::EventResult WindowDelegate::OnScrollEvent(ui::ScrollEvent* event) {
  return ui::ER_UNHANDLED;
}

}  // namespace aura
