// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/android/android_focus_rules.h"

#include "ui/aura/window.h"

namespace views {

AndroidFocusRules::AndroidFocusRules() {}

AndroidFocusRules::~AndroidFocusRules() {}

bool AndroidFocusRules::SupportsChildActivation(aura::Window* window) const {
  return window->IsRootWindow();
}

}  // namespace views
