// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/metrics.h"

#include <gtk/gtk.h>

namespace views {

int GetDoubleClickInterval() {
  GdkDisplay* display = gdk_display_get_default();
  return display ? display->double_click_time : 500;
}

int GetMenuShowDelay() {
  return kDefaultMenuShowDelay;
}

}  // namespace views
