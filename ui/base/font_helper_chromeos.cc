// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/font_helper_chromeos.h"

#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"

namespace ui {

void ReplaceNotoSansWithRobotoIfEnabled(std::string* font_family) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRobotoFontUI)) {
    static const char kNotoSansUI[] = "Noto Sans UI";
    static const size_t kNotoSansUILen = arraysize(kNotoSansUI) - 1;
    std::string::size_type noto_position = font_family->find(kNotoSansUI);
    if (noto_position != std::string::npos)
      font_family->replace(noto_position, kNotoSansUILen, "Roboto");
  }
}

}  // namespace ui
