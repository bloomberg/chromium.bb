// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_POPUP_CONSTANTS_H_
#define CHROME_BROWSER_UI_AUTOFILL_POPUP_CONSTANTS_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/insets.h"

namespace autofill {

#if defined(OS_MACOSX) || defined(OS_ANDROID)
// TODO(crbug.com/676221): Change this to pixels
const int kPopupBorderThickness = 1;
#else
// In views, the implementation takes care of the border itself.
const int kPopupBorderThickness = 0;
#endif

constexpr int kMigrationDialogMainContainerChildSpacing = 24;
constexpr gfx::Insets kMigrationDialogInsets = gfx::Insets(0, 24, 48, 24);

// The time span a card bubble should be visible even if the document
// navigates away meanwhile. This is to ensure that the user can see
// the bubble.
constexpr base::TimeDelta kCardBubbleSurviveNavigationTime =
    base::TimeDelta::FromSeconds(5);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_POPUP_CONSTANTS_H_
