// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_selection_controller.h"

namespace views {

#if !defined(TOUCH_UI)
TouchSelectionController* TouchSelectionController::create(
    TouchSelectionClientView* client_view) {
  return NULL;
}
#endif

}  // namespace views
