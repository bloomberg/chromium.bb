// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_constants.h"

namespace app_list {

const SkColor kContentsBackgroundColor = SkColorSetRGB(0xF5, 0xF5, 0xF5);
const SkColor kHoverAndPushedColor = SkColorSetARGB(0x19, 0, 0, 0);

// Duration in milliseconds for page transition.
const int kPageTransitionDurationInMs = 180;

// Duration in milliseconds for over scroll page transition.
const int kOverscrollPageTransitionDurationMs = 50;

// Preferred number of columns and rows in apps grid.
const int kPreferredCols = 4;
const int kPreferredRows = 4;

// Font style for app item labels.
const ui::ResourceBundle::FontStyle kItemTextFontStyle =
    ui::ResourceBundle::SmallBoldFont;

}  // namespace app_list
