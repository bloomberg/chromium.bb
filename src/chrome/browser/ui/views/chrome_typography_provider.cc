// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_typography_provider.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/platform_font.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "ui/native_theme/native_theme_win.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
// gn check complains on Linux Ozone.
#include "ash/public/cpp/ash_typography.h"  // nogncheck
#endif

ui::ResourceBundle::FontDetails ChromeTypographyProvider::GetFontDetails(
    int context,
    int style) const {
  // "Target" font size constants.
  constexpr int kHeadlineSize = 20;
  constexpr int kTitleSize = 15;
  constexpr int kTouchableLabelSize = 14;
  constexpr int kBodyTextLargeSize = 13;
  constexpr int kDefaultSize = 12;
  constexpr int kStatusSize = 10;

  ui::ResourceBundle::FontDetails details;
  details.size_delta = kDefaultSize - gfx::PlatformFont::kDefaultBaseFontSize;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ApplyAshFontStyles(context, style, details);
#endif

  ApplyCommonFontStyles(context, style, details);

  switch (context) {
    case views::style::CONTEXT_BUTTON_MD:
      details.weight = MediumWeightForUI();
      break;
    case views::style::CONTEXT_DIALOG_TITLE:
      details.size_delta = kTitleSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case views::style::CONTEXT_TOUCH_MENU:
      details.size_delta =
          kTouchableLabelSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case views::style::CONTEXT_DIALOG_BODY_TEXT:
    case CONTEXT_TAB_HOVER_CARD_TITLE:
    case CONTEXT_DOWNLOAD_SHELF:
      details.size_delta =
          kBodyTextLargeSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case CONTEXT_HEADLINE:
      details.size_delta =
          kHeadlineSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case CONTEXT_DOWNLOAD_SHELF_STATUS:
      details.size_delta =
          kStatusSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    default:
      break;
  }

  if (context == CONTEXT_TAB_HOVER_CARD_TITLE) {
    DCHECK_EQ(views::style::STYLE_PRIMARY, style);
    details.weight = gfx::Font::Weight::SEMIBOLD;
  }

  if (context == CONTEXT_TAB_COUNTER &&
      style == views::style::STYLE_SECONDARY) {
    // Secondary font is for double-digit counts. Because we have control over
    // system fonts on ChromeOS, we can just choose a condensed font. For other
    // platforms we adjust size.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    details.typeface = "Roboto Condensed";
#else
    details.size_delta -= 2;
#endif
  }

  if (style == STYLE_EMPHASIZED || style == STYLE_EMPHASIZED_SECONDARY) {
    // Limit emphasizing text to contexts where it's obviously correct. If you
    // hit this DCHECK, ensure it's sane and UX-approved to extend it to your
    // new case (e.g. don't add CONTEXT_BUTTON_MD).
    DCHECK(context == views::style::CONTEXT_LABEL ||
           context == views::style::CONTEXT_DIALOG_BODY_TEXT ||
           context == CONTEXT_DIALOG_BODY_TEXT_SMALL ||
           context == CONTEXT_DOWNLOAD_SHELF);
    details.weight = gfx::Font::Weight::SEMIBOLD;
  }

  if (style == STYLE_PRIMARY_MONOSPACED ||
      style == STYLE_SECONDARY_MONOSPACED) {
#if BUILDFLAG(IS_MAC)
    details.typeface = "Menlo";
#elif BUILDFLAG(IS_WIN)
    details.typeface = "Consolas";
#else
    details.typeface = "DejaVu Sans Mono";
#endif
  }

  return details;
}

SkColor ChromeTypographyProvider::GetColor(const views::View& view,
                                           int context,
                                           int style) const {
  // Body text styles are the same as for labels.
  if (context == views::style::CONTEXT_DIALOG_BODY_TEXT ||
      context == CONTEXT_DIALOG_BODY_TEXT_SMALL)
    context = views::style::CONTEXT_LABEL;

  if (context == CONTEXT_DOWNLOAD_SHELF ||
      (context == CONTEXT_DOWNLOAD_SHELF_STATUS &&
       style == views::style::STYLE_DISABLED)) {
    const auto* theme_provider = view.GetThemeProvider();
    if (!theme_provider)
      return gfx::kPlaceholderColor;
    const SkColor base_color =
        theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR_TEXT);
    // TODO(pkasting): Should use some way of dimming text that's as analogous
    // as possible to e.g. enabled vs. disabled labels.
    const SkColor dimmed_color = SkColorSetA(base_color, 0xC7);
    if (style == views::style::STYLE_DISABLED)
      return dimmed_color;
    if (context == CONTEXT_DOWNLOAD_SHELF)
      return base_color;
  }

  // Monospaced styles have the same colors as their normal counterparts.
  if (style == STYLE_PRIMARY_MONOSPACED) {
    style = views::style::STYLE_PRIMARY;
  } else if (style == STYLE_SECONDARY_MONOSPACED) {
    style = views::style::STYLE_SECONDARY;
  }

  ui::ColorId color_id;
  switch (style) {
    case STYLE_RED:
      color_id = ui::kColorAlertHighSeverity;
      break;
    case STYLE_GREEN:
      color_id = ui::kColorAlertLowSeverity;
      break;
    default:
      return TypographyProvider::GetColor(view, context, style);
  }
  return view.GetColorProvider()->GetColor(color_id);
}

int ChromeTypographyProvider::GetLineHeight(int context, int style) const {
  // "Target" line height constants from the Harmony spec. A default OS
  // configuration should use these heights. However, if the user overrides OS
  // defaults, then GetLineHeight() should return the height that would add the
  // same extra space between lines as the default configuration would have.
  constexpr int kHeadlineHeight = 32;
  constexpr int kTitleHeight = 22;
  constexpr int kBodyHeight = 20;  // For both large and small.
  constexpr int kControlHeight = 16;

// The platform-specific heights (i.e. gfx::Font::GetHeight()) that result when
// asking for the target size constants in ChromeTypographyProvider::GetFont()
// in a default OS configuration.
#if BUILDFLAG(IS_MAC)
  constexpr int kHeadlinePlatformHeight = 25;
  constexpr int kTitlePlatformHeight = 19;
  constexpr int kBodyTextLargePlatformHeight = 16;
  constexpr int kBodyTextSmallPlatformHeight = 15;
#elif BUILDFLAG(IS_WIN)
  constexpr int kHeadlinePlatformHeight = 27;
  constexpr int kTitlePlatformHeight = 20;
  constexpr int kBodyTextLargePlatformHeight = 18;
  constexpr int kBodyTextSmallPlatformHeight = 16;
#else
  constexpr int kHeadlinePlatformHeight = 24;
  constexpr int kTitlePlatformHeight = 18;
  constexpr int kBodyTextLargePlatformHeight = 17;
  constexpr int kBodyTextSmallPlatformHeight = 15;
#endif

  // The style of the system font used to determine line heights.
  constexpr int kTemplateStyle = views::style::STYLE_PRIMARY;

  // TODO(tapted): These statics should be cleared out when something invokes
  // ui::ResourceBundle::ReloadFonts(). Currently that only happens on ChromeOS.
  // See http://crbug.com/708943.
  static const int headline_height =
      GetFont(CONTEXT_HEADLINE, kTemplateStyle).GetHeight() -
      kHeadlinePlatformHeight + kHeadlineHeight;
  static const int title_height =
      GetFont(views::style::CONTEXT_DIALOG_TITLE, kTemplateStyle).GetHeight() -
      kTitlePlatformHeight + kTitleHeight;
  static const int body_large_height =
      GetFont(views::style::CONTEXT_DIALOG_BODY_TEXT, kTemplateStyle)
          .GetHeight() -
      kBodyTextLargePlatformHeight + kBodyHeight;
  static const int control_height =
      GetFont(CONTEXT_DIALOG_BODY_TEXT_SMALL, kTemplateStyle).GetHeight() -
      kBodyTextSmallPlatformHeight + kControlHeight;
  static const int default_height =
      GetFont(CONTEXT_DIALOG_BODY_TEXT_SMALL, kTemplateStyle).GetHeight() -
      kBodyTextSmallPlatformHeight + kBodyHeight;

  switch (context) {
    case views::style::CONTEXT_BUTTON:
    case views::style::CONTEXT_BUTTON_MD:
    case views::style::CONTEXT_TEXTFIELD:
    case CONTEXT_TOOLBAR_BUTTON:
      return control_height;
    case views::style::CONTEXT_DIALOG_TITLE:
      return title_height;
    case views::style::CONTEXT_DIALOG_BODY_TEXT:
    case views::style::CONTEXT_TABLE_ROW:
    case CONTEXT_TAB_HOVER_CARD_TITLE:
    case CONTEXT_DOWNLOAD_SHELF:
      return body_large_height;
    case CONTEXT_HEADLINE:
      return headline_height;
    default:
      return default_height;
  }
}
