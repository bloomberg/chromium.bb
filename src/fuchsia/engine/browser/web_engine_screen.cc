// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_screen.h"

#include "ui/display/display.h"

WebEngineScreen::WebEngineScreen() {
  const int64_t kDefaultDisplayId = 1;
  display::Display display(kDefaultDisplayId);
  ProcessDisplayChanged(display, /*is_primary=*/true);
}

WebEngineScreen::~WebEngineScreen() = default;
