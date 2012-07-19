// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_profile.h"

namespace gfx {

#if defined(OS_WIN) || defined(OS_MAC)
void ReadColorProfile(std::vector<char>* profile);
#else
void ReadColorProfile(std::vector<char>* profile) { }
#endif

void GetColorProfile(std::vector<char>* profile) {
  // Called from the FILE thread on Windows and IO thread elsewhere.

  // TODO: support multiple monitors.
  CR_DEFINE_STATIC_LOCAL(std::vector<char>, screen_profile, ());
  static bool initialized = false;

  profile->clear();
  if (!initialized) {
    initialized = true;
    ReadColorProfile(&screen_profile);
  }

  profile->assign(screen_profile.begin(), screen_profile.end());
}

}  // namespace gfx
