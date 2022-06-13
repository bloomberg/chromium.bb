// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/about_ui/credit_utils.h"

#include <stdint.h>

#include "base/strings/string_piece.h"
#include "build/chromeos_buildflags.h"
#include "components/grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace about_ui {

std::string GetCredits(bool include_scripts) {
  std::string response =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_ABOUT_UI_CREDITS_HTML);
  if (include_scripts) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    response +=
        "<script src=\"chrome://credits/keyboard_utils.js\"></script>\n";
#endif
    response +=
        "<script src=\"chrome://credits/credits.js\"></script>\n";
  }
  response += "</body>\n</html>";
  return response;
}

}  // namespace about_ui
