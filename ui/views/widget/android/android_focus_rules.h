// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_ANDROID_ANDROID_FOCUS_RULES_H_
#define UI_VIEWS_WIDGET_ANDROID_ANDROID_FOCUS_RULES_H_

#include "base/macros.h"
#include "ui/wm/core/base_focus_rules.h"

namespace views {

// A set of focus rules for Android using aura.
class AndroidFocusRules : public wm::BaseFocusRules {
 public:
  AndroidFocusRules();
  ~AndroidFocusRules() override;

 private:
  // wm::BaseFocusRules:
  bool SupportsChildActivation(aura::Window* window) const override;

  DISALLOW_COPY_AND_ASSIGN(AndroidFocusRules);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_ANDROID_ANDROID_FOCUS_RULES_H_
