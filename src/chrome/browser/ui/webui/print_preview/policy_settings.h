// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_POLICY_SETTINGS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_POLICY_SETTINGS_H_

#include "base/macros.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace printing {

// Registers the enterprise policy prefs that should be enforced in print
// preview. These settings override other settings, and cannot be controlled by
// the user.
class PolicySettings {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  DISALLOW_IMPLICIT_CONSTRUCTORS(PolicySettings);
};

}  // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_POLICY_SETTINGS_H_
