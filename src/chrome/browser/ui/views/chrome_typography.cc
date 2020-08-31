// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_typography.h"

#include "build/build_config.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/base/default_style.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/platform_font.h"

int GetFontSizeDeltaBoundedByAvailableHeight(int available_height,
                                             int desired_font_size) {
  int size_delta =
      GetFontSizeDeltaIgnoringUserOrLocaleSettings(desired_font_size);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  gfx::FontList base_font = bundle.GetFontListWithDelta(size_delta);

  // Shrink large fonts to ensure they fit. Default fonts should fit already.
  // TODO(tapted): Move DeriveWithHeightUpperBound() to ui::ResourceBundle to
  // take advantage of the font cache.
  int user_or_locale_delta =
      size_delta + gfx::PlatformFont::kDefaultBaseFontSize - desired_font_size;
  base_font = base_font.DeriveWithHeightUpperBound(available_height);

  return base_font.GetFontSize() - gfx::PlatformFont::kDefaultBaseFontSize +
         user_or_locale_delta;
}

int GetFontSizeDeltaIgnoringUserOrLocaleSettings(int desired_font_size) {
  int size_delta = desired_font_size - gfx::PlatformFont::kDefaultBaseFontSize;
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  gfx::FontList base_font = bundle.GetFontListWithDelta(size_delta);

  // The ResourceBundle's default font may not actually be kDefaultBaseFontSize
  // if, for example, the user has changed their system font sizes or the
  // current locale has been overridden to use a different default font size.
  // Adjust for the difference in default font sizes.
  int user_or_locale_delta = 0;
  if (base_font.GetFontSize() != desired_font_size) {
    user_or_locale_delta = desired_font_size - base_font.GetFontSize();
    base_font = bundle.GetFontListWithDelta(size_delta + user_or_locale_delta);
  }
  DCHECK_EQ(desired_font_size, base_font.GetFontSize());

  // To ensure a subsequent request from the ResourceBundle ignores the delta
  // due to user or locale settings, include it here.
  return base_font.GetFontSize() - gfx::PlatformFont::kDefaultBaseFontSize +
         user_or_locale_delta;
}

void ApplyCommonFontStyles(int context,
                           int style,
                           int* size_delta,
                           gfx::Font::Weight* weight) {
  switch (context) {
    case CONTEXT_TOOLBAR_BUTTON: {
      int height = ui::TouchUiController::Get()->touch_ui() ? 22 : 17;
      // We only want the font size to be constrained by available height, and
      // don't actually have a target font size, so we just need to supply any
      // sufficiently-large value for the second argument here. |height| will
      // always be sufficiently large, since dips are smaller than pts.
      *size_delta = GetFontSizeDeltaBoundedByAvailableHeight(height, height);
      break;
    }
    case CONTEXT_WEB_UI_TAB_COUNTER: {
      *size_delta = GetFontSizeDeltaIgnoringUserOrLocaleSettings(14);
      *weight = gfx::Font::Weight::BOLD;
      break;
    }
    case CONTEXT_OMNIBOX_PRIMARY:
    case CONTEXT_OMNIBOX_DEEMPHASIZED: {
      const int omnibox_primary_delta =
          GetFontSizeDeltaBoundedByAvailableHeight(
              LocationBarView::GetAvailableTextHeight(),
              ui::TouchUiController::Get()->touch_ui() ? 15 : 14);
      *size_delta = omnibox_primary_delta;
      if (context == CONTEXT_OMNIBOX_DEEMPHASIZED) {
        (*size_delta)--;
      }
      break;
    }
    case CONTEXT_OMNIBOX_DECORATION: {
      // Use 11 for both touchable and non-touchable. The touchable spec
      // specifies 11 explicitly. Historically, non-touchable would take the
      // primary omnibox font and incrementally reduce its size until it fit.
      // In default configurations, it would obtain 11. Deriving fonts is slow,
      // so don't bother starting at 14.
      static const int omnibox_decoration_delta =
          GetFontSizeDeltaBoundedByAvailableHeight(
              LocationBarView::GetAvailableDecorationTextHeight(), 11);
      *size_delta = omnibox_decoration_delta;
      break;
    }
#if defined(OS_WIN)
    case CONTEXT_WINDOWS10_NATIVE:
      // Adjusts default font size up to match Win10 modern UI.
      *size_delta = 15 - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
#endif
  }
}
