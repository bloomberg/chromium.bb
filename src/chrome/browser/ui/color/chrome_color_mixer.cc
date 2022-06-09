// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixer.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace {

ui::ColorTransform AdjustHighlightColorForContrast(ui::ColorTransform fg,
                                                   ui::ColorTransform bg) {
  auto candidate_fg =
      ui::PickGoogleColor(fg, bg, color_utils::kMinimumReadableContrastRatio);
  // Setting highlight color will set the text to the highlight color, and the
  // background to the same color with an alpha of
  // kToolbarInkDropHighlightVisibleOpacity. This means that our target contrast
  // is between the text (the highlight color) and a blend of the highlight
  // color and the toolbar color.
  auto candidate_bg = ui::AlphaBlend(candidate_fg, bg, SkAlpha{0x14});
  // Add a fudge factor to the minimum contrast ratio since we'll actually be
  // blending with the adjusted color.
  return ui::PickGoogleColor(
      candidate_fg, candidate_bg,
      color_utils::kMinimumReadableContrastRatio * 1.05f);
}

ui::ColorTransform IncreaseLightness(ui::ColorTransform input_transform,
                                     double percent) {
  const auto generator = [](ui::ColorTransform input_transform, double percent,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor color = input_transform.Run(input_color, mixer);
    color_utils::HSL result;
    color_utils::SkColorToHSL(color, &result);
    result.l += (1 - result.l) * percent;
    const SkColor result_color =
        color_utils::HSLToSkColor(result, SkColorGetA(color));
    DVLOG(2) << "ColorTransform IncreaseLightness:"
             << " Percent: " << percent
             << " Transform Color: " << ui::SkColorName(color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(input_transform), percent);
}

// This differs from ui::SelectColorBasedOnInput in that we're checking if the
// input transform is *not* dark under the assumption that the background color
// *is* dark from a potential custom theme. Additionally, if the mode is
// explicitly dark just select the correct color for that mode.
ui::ColorTransform SelectColorBasedOnDarkInputOrMode(
    bool dark_mode,
    ui::ColorTransform input_transform,
    ui::ColorTransform dark_mode_color_transform,
    ui::ColorTransform light_mode_color_transform) {
  const auto generator = [](bool dark_mode, ui::ColorTransform input_transform,
                            ui::ColorTransform dark_mode_color_transform,
                            ui::ColorTransform light_mode_color_transform,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor transform_color = input_transform.Run(input_color, mixer);
    const SkColor dark_mode_color =
        dark_mode_color_transform.Run(input_color, mixer);
    const SkColor light_mode_color =
        light_mode_color_transform.Run(input_color, mixer);
    const SkColor result_color =
        dark_mode || !color_utils::IsDark(transform_color) ? dark_mode_color
                                                           : light_mode_color;
    DVLOG(2) << "ColorTransform SelectColorBasedOnDarkColorOrMode:"
             << " Dark Mode: " << dark_mode
             << " Transform Color: " << ui::SkColorName(transform_color)
             << " Dark Mode Color: " << ui::SkColorName(dark_mode_color)
             << " Light Mode Color: " << ui::SkColorName(light_mode_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, dark_mode, std::move(input_transform),
                             std::move(dark_mode_color_transform),
                             std::move(light_mode_color_transform));
}

ui::ColorTransform GetToolbarTopSeparatorColorTransform(
    ui::ColorTransform toolbar_color_transform,
    ui::ColorTransform frame_color_transform) {
  const auto generator = [](ui::ColorTransform toolbar_color_transform,
                            ui::ColorTransform frame_color_transform,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor toolbar_color =
        toolbar_color_transform.Run(input_color, mixer);
    const SkColor frame_color = frame_color_transform.Run(input_color, mixer);
    const SkColor result_color =
        GetToolbarTopSeparatorColor(toolbar_color, frame_color);
    DVLOG(2) << "ColorTransform GetToolbarTopSeparatorColor:"
             << " Input Color: " << ui::SkColorName(input_color)
             << " Toolbar Transform Color: " << ui::SkColorName(toolbar_color)
             << " Frame Transform Color: " << ui::SkColorName(frame_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(toolbar_color_transform),
                             std::move(frame_color_transform));
}

ui::ColorTransform PickGoogleColorTwoBackgrounds(
    ui::ColorTransform fg_transform,
    ui::ColorTransform bg_a_transform,
    ui::ColorTransform bg_b_transform,
    float contrast_threshold) {
  const auto generator =
      [](ui::ColorTransform fg_transform, ui::ColorTransform bg_a_transform,
         ui::ColorTransform bg_b_transform, float contrast_threshold,
         SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor fg_color = fg_transform.Run(input_color, mixer);
    const SkColor bg_a_color = bg_a_transform.Run(input_color, mixer);
    const SkColor bg_b_color = bg_b_transform.Run(input_color, mixer);
    const SkColor result_color = color_utils::PickGoogleColor(
        fg_color, bg_a_color, bg_b_color, contrast_threshold);
    DVLOG(2) << "ColorTransform PickGoogleColorTwoBackgrounds:"
             << " Foreground Color: " << ui::SkColorName(fg_color)
             << " Background Color A: " << ui::SkColorName(bg_a_color)
             << " Background Color B: " << ui::SkColorName(bg_b_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(fg_transform),
                             std::move(bg_a_transform),
                             std::move(bg_b_transform), contrast_threshold);
}

// Flat version of dark mode colors used in bookmarks bar to fill
// the buttons.
constexpr SkColor kFlatGrey = SkColorSetRGB(0x5D, 0x5E, 0x62);
constexpr SkColor kFlatBlue = SkColorSetRGB(0x49, 0x54, 0x68);
constexpr SkColor kFlatRed = SkColorSetRGB(0x62, 0x4A, 0x4B);
constexpr SkColor kFlatGreen = SkColorSetRGB(0x47, 0x59, 0x50);
constexpr SkColor kFlatYellow = SkColorSetRGB(0x65, 0x5C, 0x44);
constexpr SkColor kFlatCyan = SkColorSetRGB(0x45, 0x5D, 0x65);
constexpr SkColor kFlatPurple = SkColorSetRGB(0x58, 0x4A, 0x68);
constexpr SkColor kFlatPink = SkColorSetRGB(0x65, 0x4A, 0x5D);

// Default toolbar colors.
constexpr SkColor kDarkToolbarColor = SkColorSetRGB(0x35, 0x36, 0x3A);
constexpr SkColor kLightToolbarColor = SK_ColorWHITE;

// Alpha of 61 = 24%. From GetTabGroupColors() in theme_helper.cc.
constexpr SkAlpha tab_group_chip_alpha = 61;

}  // namespace

void AddChromeColorMixer(ui::ColorProvider* provider,
                         const ui::ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorAppMenuHighlightSeverityLow] = AdjustHighlightColorForContrast(
      ui::kColorAlertLowSeverity, kColorToolbar);
  mixer[kColorAppMenuHighlightSeverityHigh] = {
      kColorAvatarButtonHighlightSyncError};
  mixer[kColorAppMenuHighlightSeverityMedium] = AdjustHighlightColorForContrast(
      ui::kColorAlertMediumSeverity, kColorToolbar);
  mixer[kColorAvatarButtonHighlightNormal] =
      AdjustHighlightColorForContrast(ui::kColorAccent, kColorToolbar);
  mixer[kColorAvatarButtonHighlightSyncError] = AdjustHighlightColorForContrast(
      ui::kColorAlertHighSeverity, kColorToolbar);
  mixer[kColorAvatarButtonHighlightSyncPaused] = {
      kColorAvatarButtonHighlightNormal};
  mixer[kColorAvatarStrokeLight] = {SK_ColorWHITE};
  mixer[kColorBookmarkBarBackground] = {kColorToolbar};
  mixer[kColorBookmarkBarForeground] = {kColorToolbarText};
  mixer[kColorBookmarkButtonIcon] = {kColorToolbarButtonIcon};
  // If the custom theme supplies a specific color for the bookmark text, use
  // that color to derive folder icon color. We don't actually use the color
  // returned, rather we use the color provider color transform corresponding to
  // that color.
  SkColor color;
  const bool custom_icon_color =
      key.custom_theme &&
      key.custom_theme->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON,
                                 &color);
  mixer[kColorBookmarkFavicon] =
      custom_icon_color ? ui::ColorTransform(kColorToolbarButtonIcon)
                        : ui::ColorTransform(SK_ColorTRANSPARENT);
  const bool custom_text_color =
      key.custom_theme &&
      key.custom_theme->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT, &color);
  mixer[kColorBookmarkFolderIcon] =
      custom_text_color
          ? ui::DeriveDefaultIconColor(kColorBookmarkBarForeground)
          : ui::ColorTransform(ui::kColorIcon);
  mixer[kColorBookmarkBarSeparator] = {kColorToolbarSeparator};
  mixer[kColorBookmarkDragImageBackground] = {ui::kColorAccent};
  mixer[kColorBookmarkDragImageCountBackground] = {ui::kColorAlertHighSeverity};
  mixer[kColorBookmarkDragImageCountForeground] =
      ui::GetColorWithMaxContrast(kColorBookmarkDragImageCountBackground);
  mixer[kColorBookmarkDragImageForeground] =
      ui::GetColorWithMaxContrast(kColorBookmarkDragImageBackground);
  mixer[kColorBookmarkDragImageIconBackground] = {
      kColorBookmarkDragImageForeground};
  mixer[kColorCaptionButtonBackground] = {SK_ColorTRANSPARENT};
  mixer[kColorCapturedTabContentsBorder] = {ui::kColorAccent};
  mixer[kColorDesktopMediaTabListBorder] = {ui::kColorMidground};
  mixer[kColorDesktopMediaTabListPreviewBackground] = {ui::kColorMidground};
  mixer[kColorDownloadItemForeground] = {kColorDownloadShelfForeground};
  mixer[kColorDownloadItemForegroundDangerous] =
      ui::PickGoogleColor(ui::kColorAlertHighSeverity, kColorToolbar,
                          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadItemForegroundDisabled] = BlendForMinContrast(
      ui::AlphaBlend(kColorDownloadItemForeground, kColorToolbar,
                     gfx::kGoogleGreyAlpha600),
      kColorToolbar, kColorDownloadItemForeground);
  mixer[kColorDownloadItemForegroundSafe] =
      ui::PickGoogleColor(ui::kColorAlertLowSeverity, kColorToolbar,
                          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadItemProgressRingBackground] = ui::SetAlpha(
      kColorDownloadItemProgressRingForeground, gfx::kGoogleGreyAlpha400);
  mixer[kColorDownloadItemProgressRingForeground] = {ui::kColorThrobber};
  mixer[kColorDownloadShelfBackground] = {kColorToolbar};
  mixer[kColorDownloadShelfButtonBackground] = {kColorDownloadShelfBackground};
  mixer[kColorDownloadShelfButtonIcon] = {kColorToolbarButtonIcon};
  mixer[kColorDownloadShelfButtonIconDisabled] = {
      kColorToolbarButtonIconDisabled};
  mixer[kColorDownloadShelfButtonText] =
      ui::PickGoogleColor(ui::kColorAccent, kColorDownloadShelfBackground,
                          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadShelfContentAreaSeparator] = ui::AlphaBlend(
      kColorToolbarButtonIcon, kColorDownloadShelfBackground, 0x3A);
  mixer[kColorDownloadShelfForeground] = {kColorToolbarText};
  mixer[kColorDownloadStartedAnimationForeground] = {ui::kColorAccent};
  mixer[kColorDownloadToolbarButtonActive] = {ui::kColorThrobber};
  mixer[kColorDownloadToolbarButtonInactive] = {kColorToolbarButtonIcon};
  mixer[kColorDownloadToolbarButtonRingBackground] = {
      SkColorSetA(kColorDownloadToolbarButtonInactive, 0x33)};
  mixer[kColorExtensionDialogBackground] = {SK_ColorWHITE};
  mixer[kColorExtensionIconBadgeBackgroundDefault] = {ui::kColorAccent};
  mixer[kColorExtensionIconDecorationAmbientShadow] =
      ui::SetAlpha(ui::kColorShadowBase, 0x26);
  mixer[kColorExtensionIconDecorationBackground] = {SK_ColorWHITE};
  mixer[kColorExtensionIconDecorationKeyShadow] =
      ui::SetAlpha(ui::kColorShadowBase, 0x4D);
  mixer[kColorExtensionMenuIcon] = {ui::kColorIcon};
  mixer[kColorExtensionMenuIconDisabled] = {ui::kColorIconDisabled};
  mixer[kColorExtensionMenuPinButtonIcon] = {ui::kColorAccent};
  mixer[kColorExtensionMenuPinButtonIconDisabled] = ui::SetAlpha(
      kColorExtensionMenuPinButtonIcon, gfx::kDisabledControlAlpha);
  mixer[kColorEyedropperBoundary] = {SK_ColorDKGRAY};
  mixer[kColorEyedropperCentralPixelInnerRing] = {SK_ColorBLACK};
  mixer[kColorEyedropperCentralPixelOuterRing] = {SK_ColorWHITE};
  mixer[kColorEyedropperGrid] = {SK_ColorGRAY};
  mixer[kColorFeaturePromoBubbleBackground] = ui::PickGoogleColor(
      ui::kColorAccent, kColorFeaturePromoBubbleForeground, 5.3f);
  mixer[kColorFeaturePromoBubbleButtonBorder] = {gfx::kGoogleGrey300};
  mixer[kColorFeaturePromoBubbleCloseButtonInkDrop] = ui::PickGoogleColor(
      ui::kColorAccent, kColorFeaturePromoBubbleBackground, 2.5f);
  mixer[kColorFeaturePromoBubbleDefaultButtonBackground] = {
      kColorFeaturePromoBubbleForeground};
  mixer[kColorFeaturePromoBubbleDefaultButtonForeground] = {
      kColorFeaturePromoBubbleBackground};
  mixer[kColorFeaturePromoBubbleForeground] = {SK_ColorWHITE};
  mixer[kColorFindBarBackground] = {ui::kColorTextfieldBackground};
  mixer[kColorFindBarButtonIcon] =
      ui::DeriveDefaultIconColor(ui::kColorTextfieldForeground);
  mixer[kColorFindBarButtonIconDisabled] =
      ui::DeriveDefaultIconColor(ui::kColorTextfieldForegroundDisabled);
  mixer[kColorFindBarForeground] = {ui::kColorTextfieldForeground};
  mixer[kColorFindBarMatchCount] = {ui::kColorSecondaryForeground};
  mixer[kColorFindBarSeparator] = {ui::kColorSeparator};
  mixer[kColorFlyingIndicatorBackground] = {kColorToolbar};
  mixer[kColorFlyingIndicatorForeground] = {kColorToolbarButtonIcon};
  mixer[kColorFocusHighlightDefault] = {SkColorSetRGB(0x10, 0x10, 0x10)};
  mixer[kColorFrameCaptionActive] =
      ui::GetColorWithMaxContrast({ui::kColorFrameActive});
  mixer[kColorFrameCaptionInactive] =
      ui::GetColorWithMaxContrast({ui::kColorFrameInactive});
  mixer[kColorInfoBarBackground] = {kColorToolbar};
  mixer[kColorInfoBarButtonIcon] = {kColorToolbarButtonIcon};
  mixer[kColorInfoBarButtonIconDisabled] = {kColorToolbarButtonIconDisabled};
  mixer[kColorInfoBarContentAreaSeparator] =
      ui::AlphaBlend(kColorToolbarButtonIcon, kColorInfoBarBackground, 0x3A);
  mixer[kColorInfoBarForeground] = {kColorToolbarText};
  mixer[kColorLocationBarBorder] = {SkColorSetA(SK_ColorBLACK, 0x4D)};
  mixer[kColorLocationBarBorderOpaque] =
      ui::GetResultingPaintColor(kColorLocationBarBorder, kColorToolbar);
  mixer[kColorMediaRouterIconActive] = {ui::kColorAccent};
  mixer[kColorMediaRouterIconError] = {ui::kColorAlertHighSeverity};
  mixer[kColorMediaRouterIconWarning] = {ui::kColorAlertMediumSeverity};
  {
    int result = 0;
    if (!key.custom_theme ||
        !key.custom_theme->GetDisplayProperty(
            ThemeProperties::SHOULD_FILL_BACKGROUND_TAB_COLOR, &result) ||
        result) {
      mixer[kColorNewTabButtonBackgroundFrameActive] = {
          kColorTabBackgroundInactiveFrameActive};
      mixer[kColorNewTabButtonBackgroundFrameInactive] = {
          kColorTabBackgroundInactiveFrameInactive};
    } else {
      mixer[kColorNewTabButtonBackgroundFrameActive] = {SK_ColorTRANSPARENT};
      mixer[kColorNewTabButtonBackgroundFrameInactive] = {SK_ColorTRANSPARENT};
    }
  }
  mixer[kColorNewTabButtonFocusRing] = PickGoogleColorTwoBackgrounds(
      ui::kColorFocusableBorderFocused,
      ui::GetResultingPaintColor(kColorNewTabButtonBackgroundFrameActive,
                                 ui::kColorFrameActive),
      ui::kColorFrameActive, color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorNewTabButtonInkDropFrameActive] =
      ui::GetColorWithMaxContrast(kColorNewTabButtonBackgroundFrameActive);
  mixer[kColorNewTabButtonInkDropFrameInactive] =
      ui::GetColorWithMaxContrast(kColorNewTabButtonBackgroundFrameInactive);
  mixer[kColorNewTabPageBackground] = {kColorToolbar};
  mixer[kColorNewTabPageHeader] = {SkColorSetRGB(0x96, 0x96, 0x96)};
  mixer[kColorNewTabPageLink] = {dark_mode ? gfx::kGoogleBlue300
                                           : SkColorSetRGB(0x06, 0x37, 0x74)};
  mixer[kColorNewTabPageLogo] = {kColorNewTabPageLogoUnthemedLight};
  mixer[kColorNewTabPageLogoUnthemedDark] = {gfx::kGoogleGrey700};
  mixer[kColorNewTabPageLogoUnthemedLight] = {SkColorSetRGB(0xEE, 0xEE, 0xEE)};
  if (dark_mode) {
    mixer[kColorNewTabPageMostVisitedTileBackground] = {gfx::kGoogleGrey900};
  } else {
    mixer[kColorNewTabPageMostVisitedTileBackground] = {
        kColorNewTabPageMostVisitedTileBackgroundUnthemed};
  }
  mixer[kColorNewTabPageMostVisitedTileBackgroundUnthemed] = {
      gfx::kGoogleGrey100};
  mixer[kColorNewTabPageSectionBorder] =
      ui::SetAlpha(kColorNewTabPageHeader, 0x50);
  mixer[kColorNewTabPageText] = {dark_mode ? gfx::kGoogleGrey200
                                           : SK_ColorBLACK};
  mixer[kColorNewTabPageTextUnthemed] = {gfx::kGoogleGrey050};
  mixer[kColorNewTabPageTextLight] =
      IncreaseLightness(kColorNewTabPageText, 0.40);
  mixer[kColorOmniboxAnswerIconBackground] = {
      ui::kColorButtonBackgroundProminent};
  mixer[kColorOmniboxAnswerIconForeground] = {
      ui::kColorButtonForegroundProminent};
  mixer[kColorOmniboxChipBackgroundLowVisibility] = {
      kColorTabBackgroundActiveFrameActive};
  mixer[kColorOmniboxChipBackgroundNormalVisibility] = {
      ui::kColorButtonBackground};
  mixer[kColorOmniboxChipForegroundLowVisibility] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorOmniboxChipForegroundNormalVisibility] = {
      ui::kColorButtonForeground};
  mixer[kColorPageInfoChosenObjectDeleteButtonIcon] = {ui::kColorIcon};
  mixer[kColorPageInfoChosenObjectDeleteButtonIconDisabled] = {
      ui::kColorIconDisabled};
  mixer[kColorPaymentsFeedbackTipBackground] = {
      ui::kColorSubtleEmphasisBackground};
  mixer[kColorPaymentsFeedbackTipBorder] = {ui::kColorBubbleFooterBorder};
  mixer[kColorPaymentsFeedbackTipForeground] = {
      ui::kColorLabelForegroundSecondary};
  mixer[kColorPaymentsFeedbackTipIcon] = {ui::kColorAlertMediumSeverity};
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  mixer[kColorPaymentsGooglePayLogo] = {dark_mode ? SK_ColorWHITE
                                                  : gfx::kGoogleGrey700};
#endif
  mixer[kColorPaymentsPromoCodeBackground] = {
      dark_mode ? SkColorSetA(gfx::kGoogleGreen300, 0x1F)
                : gfx::kGoogleGreen050};
  mixer[kColorPaymentsPromoCodeForeground] = {dark_mode ? gfx::kGoogleGreen300
                                                        : gfx::kGoogleGreen800};
  mixer[kColorPaymentsPromoCodeForegroundHovered] = {
      dark_mode ? gfx::kGoogleGreen200 : gfx::kGoogleGreen900};
  mixer[kColorPaymentsPromoCodeForegroundPressed] = {
      kColorPaymentsPromoCodeForegroundHovered};
  mixer[kColorPaymentsPromoCodeInkDrop] = {dark_mode ? gfx::kGoogleGreen300
                                                     : gfx::kGoogleGreen600};
  mixer[kColorPaymentsRequestBackArrowButtonIcon] = {ui::kColorIcon};
  mixer[kColorPaymentsRequestBackArrowButtonIconDisabled] = {
      ui::kColorIconDisabled};
  mixer[kColorPaymentsRequestRowBackgroundHighlighted] = {
      SkColorSetA(SK_ColorBLACK, 0x0D)};
  mixer[kColorPipWindowBackToTabButtonBackground] = {
      SkColorSetA(SK_ColorBLACK, 0x60)};
  mixer[kColorPipWindowBackground] = {SK_ColorBLACK};
  mixer[kColorPipWindowControlsBackground] = {
      SkColorSetA(gfx::kGoogleGrey900, 0xC1)};
  mixer[kColorPipWindowForeground] =
      ui::GetColorWithMaxContrast(kColorPipWindowBackground);
  mixer[kColorPipWindowHangUpButtonForeground] = {gfx::kGoogleRed300};
  mixer[kColorPipWindowSkipAdButtonBackground] = {gfx::kGoogleGrey700};
  mixer[kColorPipWindowSkipAdButtonBorder] = {kColorPipWindowForeground};
  // TODO(https://crbug.com/1315194): stop forcing the light theme once the
  // reauth dialog supports the dark mode.
  mixer[kColorProfilesReauthDialogBorder] = {SK_ColorWHITE};
  mixer[kColorQrCodeBackground] = {SK_ColorWHITE};
  mixer[kColorQrCodeBorder] = {ui::kColorMidground};
  mixer[kColorQuickAnswersReportQueryButtonBackground] =
      ui::SetAlpha(ui::kColorAccent, 0x0A);
  mixer[kColorQuickAnswersReportQueryButtonForeground] = {ui::kColorAccent};
  mixer[kColorScreenshotCapturedImageBackground] = {ui::kColorBubbleBackground};
  mixer[kColorScreenshotCapturedImageBorder] = {ui::kColorMidground};
  mixer[kColorSidePanelContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorStatusBubbleBackgroundFrameActive] = {
      kColorTabBackgroundInactiveFrameActive};
  mixer[kColorStatusBubbleBackgroundFrameInactive] = {
      kColorTabBackgroundInactiveFrameInactive};
  mixer[kColorStatusBubbleForegroundFrameActive] = {
      kColorTabForegroundInactiveFrameActive};
  mixer[kColorStatusBubbleForegroundFrameInactive] = {
      kColorTabForegroundInactiveFrameInactive};
  mixer[kColorStatusBubbleShadow] = {SkColorSetA(SK_ColorBLACK, 0x1E)};
  mixer[kColorTabAlertAudioPlayingActiveFrameActive] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorTabAlertAudioPlayingActiveFrameInactive] = {
      kColorTabForegroundActiveFrameInactive};
  mixer[kColorTabAlertAudioPlayingInactiveFrameActive] = {
      kColorTabForegroundInactiveFrameActive};
  mixer[kColorTabAlertAudioPlayingInactiveFrameInactive] = {
      kColorTabForegroundInactiveFrameInactive};
  mixer[kColorTabAlertMediaRecordingActiveFrameActive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundActiveFrameActive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertMediaRecordingActiveFrameInactive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundActiveFrameInactive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertMediaRecordingInactiveFrameActive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundInactiveFrameActive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertMediaRecordingInactiveFrameInactive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundInactiveFrameInactive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertPipPlayingActiveFrameActive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundActiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabAlertPipPlayingActiveFrameInactive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundActiveFrameInactive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabAlertPipPlayingInactiveFrameActive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundInactiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabAlertPipPlayingInactiveFrameInactive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundInactiveFrameInactive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabCloseButtonFocusRingActive] = ui::PickGoogleColor(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundActiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabCloseButtonFocusRingInactive] = ui::PickGoogleColor(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundInactiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabFocusRingActive] = PickGoogleColorTwoBackgrounds(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundActiveFrameActive,
      ui::kColorFrameActive, color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabFocusRingInactive] = PickGoogleColorTwoBackgrounds(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundInactiveFrameActive,
      ui::kColorFrameActive, color_utils::kMinimumVisibleContrastRatio);

  mixer[kColorTabGroupTabStripFrameActiveBlue] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorTabGroupTabStripFrameActiveCyan] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorTabGroupTabStripFrameActiveGreen] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorTabGroupTabStripFrameActiveGrey] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorTabGroupTabStripFrameActiveOrange] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorTabGroupTabStripFrameActivePink] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorTabGroupTabStripFrameActivePurple] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorTabGroupTabStripFrameActiveRed] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorTabGroupTabStripFrameActiveYellow] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);

  mixer[kColorTabGroupTabStripFrameInactiveBlue] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorTabGroupTabStripFrameInactiveCyan] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorTabGroupTabStripFrameInactiveGreen] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorTabGroupTabStripFrameInactiveGrey] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorTabGroupTabStripFrameInactiveOrange] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorTabGroupTabStripFrameInactivePink] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorTabGroupTabStripFrameInactivePurple] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorTabGroupTabStripFrameInactiveRed] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorTabGroupTabStripFrameInactiveYellow] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);

  mixer[kColorTabGroupDialogBlue] = {kColorTabGroupContextMenuBlue};
  mixer[kColorTabGroupDialogCyan] = {kColorTabGroupContextMenuCyan};
  mixer[kColorTabGroupDialogGreen] = {kColorTabGroupContextMenuGreen};
  mixer[kColorTabGroupDialogGrey] = {kColorTabGroupContextMenuGrey};
  mixer[kColorTabGroupDialogOrange] = {kColorTabGroupContextMenuOrange};
  mixer[kColorTabGroupDialogPink] = {kColorTabGroupContextMenuPink};
  mixer[kColorTabGroupDialogPurple] = {kColorTabGroupContextMenuPurple};
  mixer[kColorTabGroupDialogRed] = {kColorTabGroupContextMenuRed};
  mixer[kColorTabGroupDialogYellow] = {kColorTabGroupContextMenuYellow};

  mixer[kColorTabGroupContextMenuBlue] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleBlue300,
      gfx::kGoogleBlue600);
  mixer[kColorTabGroupContextMenuCyan] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleCyan300,
      gfx::kGoogleCyan900);
  mixer[kColorTabGroupContextMenuGreen] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleGreen300,
      gfx::kGoogleGreen700);
  mixer[kColorTabGroupContextMenuGrey] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleGrey300,
      gfx::kGoogleGrey700);
  mixer[kColorTabGroupContextMenuOrange] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleOrange300,
      gfx::kGoogleOrange400);
  mixer[kColorTabGroupContextMenuPink] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGooglePink300,
      gfx::kGooglePink700);
  mixer[kColorTabGroupContextMenuPurple] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGooglePurple300,
      gfx::kGooglePurple500);
  mixer[kColorTabGroupContextMenuRed] =
      SelectColorBasedOnDarkInputOrMode(dark_mode, kColorBookmarkBarForeground,
                                        gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorTabGroupContextMenuYellow] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleYellow300,
      gfx::kGoogleYellow600);

  mixer[kColorTabGroupBookmarkBarBlue] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, kFlatBlue, gfx::kGoogleBlue050);
  mixer[kColorTabGroupBookmarkBarCyan] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, kFlatCyan, gfx::kGoogleCyan050);
  mixer[kColorTabGroupBookmarkBarGreen] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, kFlatGreen, gfx::kGoogleGreen050);
  mixer[kColorTabGroupBookmarkBarGrey] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, kFlatGrey, gfx::kGoogleGrey100);
  mixer[kColorTabGroupBookmarkBarPink] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, kFlatPink, gfx::kGooglePink050);
  mixer[kColorTabGroupBookmarkBarPurple] =
      SelectColorBasedOnDarkInputOrMode(dark_mode, kColorBookmarkBarForeground,
                                        kFlatPurple, gfx::kGooglePurple050);
  mixer[kColorTabGroupBookmarkBarRed] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, kFlatRed, gfx::kGoogleRed050);
  mixer[kColorTabGroupBookmarkBarYellow] =
      SelectColorBasedOnDarkInputOrMode(dark_mode, kColorBookmarkBarForeground,
                                        kFlatYellow, gfx::kGoogleYellow050);
  auto flat_orange = ui::AlphaBlend(gfx::kGoogleOrange300, kDarkToolbarColor,
                                    tab_group_chip_alpha);
  mixer[kColorTabGroupBookmarkBarOrange] =
      SelectColorBasedOnDarkInputOrMode(dark_mode, kColorBookmarkBarForeground,
                                        flat_orange, gfx::kGoogleOrange050);

  mixer[kColorTabHoverCardBackground] = {dark_mode ? gfx::kGoogleGrey900
                                                   : gfx::kGoogleGrey050};
  mixer[kColorTabHoverCardForeground] = {dark_mode ? gfx::kGoogleGrey700
                                                   : gfx::kGoogleGrey300};
  mixer[kColorTabStrokeFrameActive] = {kColorToolbarTopSeparatorFrameActive};
  mixer[kColorTabStrokeFrameInactive] = {
      kColorToolbarTopSeparatorFrameInactive};
  mixer[kColorTabstripLoadingProgressBackground] =
      ui::AlphaBlend(ui::kColorAccent, kColorToolbar, 0x32);
  mixer[kColorTabstripLoadingProgressForeground] = {ui::kColorAccent};
  mixer[kColorTabstripScrollContainerShadow] =
      ui::SetAlpha(ui::kColorShadowBase, 0x4D);
  mixer[kColorTabThrobber] = {ui::kColorThrobber};
  mixer[kColorTabThrobberPreconnect] = {ui::kColorThrobberPreconnect};
  mixer[kColorThumbnailTabBackground] = ui::BlendForMinContrast(
      ui::kColorAccent, ui::kColorFrameActive, absl::nullopt,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorThumbnailTabForeground] =
      ui::GetColorWithMaxContrast(kColorThumbnailTabBackground);
  mixer[kColorThumbnailTabStripBackgroundActive] = {ui::kColorFrameActive};
  mixer[kColorThumbnailTabStripBackgroundInactive] = {ui::kColorFrameInactive};
  mixer[kColorThumbnailTabStripTabGroupFrameActiveBlue] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveCyan] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveGreen] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveGrey] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveOrange] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorThumbnailTabStripTabGroupFrameActivePink] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorThumbnailTabStripTabGroupFrameActivePurple] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveRed] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveYellow] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveBlue] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveCyan] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveGreen] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveGrey] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveOrange] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorThumbnailTabStripTabGroupFrameInactivePink] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorThumbnailTabStripTabGroupFrameInactivePurple] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveRed] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveYellow] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);

  mixer[kColorToolbar] = {dark_mode ? kDarkToolbarColor : kLightToolbarColor};
  mixer[kColorToolbarButtonBackgroundHighlightedDefault] =
      ui::SetAlpha(ui::GetColorWithMaxContrast(kColorToolbarButtonText), 0xCC);
  mixer[kColorToolbarButtonBorder] = ui::SetAlpha(kColorToolbarInkDrop, 0x20);
  mixer[kColorToolbarButtonIcon] = {kColorToolbarButtonIconDefault};
  mixer[kColorToolbarButtonIconDefault] = ui::HSLShift(
      gfx::kGoogleGrey700, GetThemeTint(ThemeProperties::TINT_BUTTONS, key));
  mixer[kColorToolbarButtonIconDisabled] =
      ui::SetAlpha(kColorToolbarButtonIcon, gfx::kDisabledControlAlpha);
  mixer[kColorToolbarButtonIconHovered] = {kColorToolbarButtonIcon};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, gfx::kGoogleGreyAlpha500)};
  mixer[kColorToolbarButtonIconPressed] = {kColorToolbarButtonIconHovered};
  mixer[kColorToolbarButtonText] = ui::GetColorWithMaxContrast(kColorToolbar);
  mixer[kColorToolbarContentAreaSeparator] =
      ui::AlphaBlend(kColorToolbarButtonIcon, kColorToolbar, 0x3A);
  mixer[kColorToolbarFeaturePromoHighlight] =
      ui::PickGoogleColor(ui::kColorAccent, kColorToolbar,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorToolbarInkDrop] = ui::GetColorWithMaxContrast(kColorToolbar);
  mixer[kColorToolbarSeparator] = {kColorToolbarSeparatorDefault};
  mixer[kColorToolbarSeparatorDefault] =
      ui::SetAlpha(kColorToolbarButtonIcon, 0x4D);
  mixer[kColorToolbarText] = {kColorToolbarTextDefault};
  mixer[kColorToolbarTextDefault] = {dark_mode ? SK_ColorWHITE
                                               : gfx::kGoogleGrey800};
  mixer[kColorToolbarTextDisabled] = {kColorToolbarTextDisabledDefault};
  mixer[kColorToolbarTextDisabledDefault] =
      ui::SetAlpha(kColorToolbarText, gfx::kDisabledControlAlpha);
  mixer[kColorToolbarTopSeparatorFrameActive] =
      GetToolbarTopSeparatorColorTransform(kColorToolbar,
                                           ui::kColorFrameActive);
  mixer[kColorToolbarTopSeparatorFrameInactive] =
      GetToolbarTopSeparatorColorTransform(kColorToolbar,
                                           ui::kColorFrameInactive);
  mixer[kColorWebAuthnBackArrowButtonIcon] = {ui::kColorIcon};
  mixer[kColorWebAuthnBackArrowButtonIconDisabled] = {ui::kColorIconDisabled};
  mixer[kColorWebAuthnPinTextfieldBottomBorder] = {ui::kColorAccent};
  mixer[kColorWebAuthnProgressRingBackground] = ui::SetAlpha(
      kColorWebAuthnProgressRingForeground, gfx::kGoogleGreyAlpha400);
  mixer[kColorWebAuthnProgressRingForeground] = {ui::kColorThrobber};
  mixer[kColorWebContentsBackground] = {kColorNewTabPageBackground};
  mixer[kColorWebContentsBackgroundLetterboxing] =
      ui::AlphaBlend(kColorWebContentsBackground, SK_ColorBLACK, 0x33);
  mixer[kColorWindowControlButtonBackgroundActive] = {ui::kColorFrameActive};
  mixer[kColorWindowControlButtonBackgroundInactive] = {
      ui::kColorFrameInactive};

  // Apply high contrast recipes if necessary.
  if (!ShouldApplyHighContrastColors(key))
    return;
  mixer[kColorDownloadShelfContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorInfoBarContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorLocationBarBorder] = {kColorToolbarText};
  mixer[kColorToolbar] = {dark_mode ? SK_ColorBLACK : SK_ColorWHITE};
  mixer[kColorToolbarContentAreaSeparator] = {kColorToolbarText};
  mixer[kColorToolbarText] = {dark_mode ? SK_ColorWHITE : SK_ColorBLACK};
  mixer[kColorToolbarTopSeparatorFrameActive] = {dark_mode ? SK_ColorDKGRAY
                                                           : SK_ColorLTGRAY};
  mixer[ui::kColorFrameActive] = {SK_ColorDKGRAY};
  mixer[ui::kColorFrameInactive] = {SK_ColorGRAY};
}
