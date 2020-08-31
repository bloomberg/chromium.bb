// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_

#include "ui/color/color_id.h"

// TODO(pkasting): Add the rest of the colors.

// clang-format off
#define CHROME_COLOR_IDS \
  /* Omnibox output colors. */ \
  E(kColorOmniboxBackground, \
    ThemeProperties::COLOR_OMNIBOX_BACKGROUND, kChromeColorsStart) \
  E(kColorOmniboxBackgroundHovered, \
    ThemeProperties::COLOR_OMNIBOX_BACKGROUND_HOVERED) \
  E(kColorOmniboxBubbleOutline, \
    ThemeProperties::COLOR_OMNIBOX_BUBBLE_OUTLINE) \
  E(kColorOmniboxBubbleOutlineExperimentalKeywordMode, \
    ThemeProperties::COLOR_OMNIBOX_BUBBLE_OUTLINE_EXPERIMENTAL_KEYWORD_MODE) \
  E(kColorOmniboxResultsBackground, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BG) \
  E(kColorOmniboxResultsBackgroundHovered, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_HOVERED) \
  E(kColorOmniboxResultsBackgroundSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_BG_SELECTED) \
  E(kColorOmniboxResultsIcon, ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON) \
  E(kColorOmniboxResultsIconSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_ICON_SELECTED) \
  E(kColorOmniboxResultsTextDimmed, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED) \
  E(kColorOmniboxResultsTextDimmedSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_DIMMED_SELECTED) \
  E(kColorOmniboxResultsTextSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_TEXT_SELECTED) \
  E(kColorOmniboxResultsUrl, ThemeProperties::COLOR_OMNIBOX_RESULTS_URL) \
  E(kColorOmniboxResultsUrlSelected, \
    ThemeProperties::COLOR_OMNIBOX_RESULTS_URL_SELECTED) \
  E(kColorOmniboxSecurityChipDangerous, \
    ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_DANGEROUS) \
  E(kColorOmniboxSecurityChipDefault, \
    ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_DEFAULT) \
  E(kColorOmniboxSecurityChipSecure, \
    ThemeProperties::COLOR_OMNIBOX_SECURITY_CHIP_SECURE) \
  E(kColorOmniboxSelectedKeyword, \
    ThemeProperties::COLOR_OMNIBOX_SELECTED_KEYWORD) \
  E(kColorOmniboxText, ThemeProperties::COLOR_OMNIBOX_TEXT) \
  E(kColorOmniboxTextDimmed, ThemeProperties::COLOR_OMNIBOX_TEXT_DIMMED) \
  \
  E(kColorToolbar, ThemeProperties::COLOR_TOOLBAR)

#include "ui/color/color_id_macros.inc"

enum ChromeColorIds : ui::ColorId {
  kChromeColorsStart = ui::kUiColorsEnd,

  CHROME_COLOR_IDS

  kChromeColorsEnd,
};

#include "ui/color/color_id_macros.inc"

// clang-format on

static_assert(ui::ColorId{kChromeColorsEnd} <= ui::ColorId{ui::kUiColorsLast},
              "Embedder colors must not exceed allowed space");

enum ChromeColorSetIds : ui::ColorSetId {
  kColorSetCustomTheme = ui::kUiColorSetsEnd,

  kChromeColorSetsEnd,
};

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_ID_H_
