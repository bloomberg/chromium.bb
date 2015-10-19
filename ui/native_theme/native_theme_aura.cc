// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aura.h"

#include <limits>

#include "base/logging.h"
#include "ui/base/layout.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/nine_image_painter.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_switches.h"
#include "ui/resources/grit/ui_resources.h"

using gfx::NineImagePainter;

#define EMPTY_IMAGE_GRID { 0, 0, 0, 0, 0, 0, 0, 0, 0 }

namespace ui {

namespace {

const int kScrollbarThumbImages[NativeTheme::kNumStates][9] = {
  EMPTY_IMAGE_GRID,
  IMAGE_GRID(IDR_SCROLLBAR_THUMB_BASE_HOVER),
  IMAGE_GRID(IDR_SCROLLBAR_THUMB_BASE_NORMAL),
  IMAGE_GRID(IDR_SCROLLBAR_THUMB_BASE_PRESSED)
};

const int kScrollbarArrowButtonImages[NativeTheme::kNumStates][9] = {
  EMPTY_IMAGE_GRID,
  IMAGE_GRID(IDR_SCROLLBAR_ARROW_BUTTON_BASE_HOVER),
  IMAGE_GRID(IDR_SCROLLBAR_ARROW_BUTTON_BASE_NORMAL),
  IMAGE_GRID(IDR_SCROLLBAR_ARROW_BUTTON_BASE_PRESSED)
};

const uint8 kScrollbarOverlayThumbFillAlphas[NativeTheme::kNumStates] = {
  0,    // Does not matter, will not paint for disabled state.
  178,  // Hover state, opacity 70%, alpha would be 0.7 * 255.
  140,  // Normal state, opacity 55%, alpha would be 0.55 * 255.
  178   // Pressed state, opacity 70%, alpha would be 0.7 * 255.
};

const uint8 kScrollbarOverlayThumbStrokeAlphas[NativeTheme::kNumStates] = {
  0,   // Does not matter, will not paint for disabled state.
  51,  // Hover state, opacity 20%, alpha would be 0.2 * 255.
  38,  // Normal state, opacity 15%, alpha would be 0.15 * 255.
  51   // Pressed state, opacity 20%, alpha would be 0.2 * 255.
};

const int kScrollbarOverlayThumbStrokeImages[9] =
    IMAGE_GRID_NO_CENTER(IDR_SCROLLBAR_OVERLAY_THUMB_STROKE);

const int kScrollbarOverlayThumbFillImages[9] =
    IMAGE_GRID(IDR_SCROLLBAR_OVERLAY_THUMB_FILL);

const int kScrollbarTrackImages[9] = IMAGE_GRID(IDR_SCROLLBAR_BASE);

}  // namespace

#if !defined(OS_WIN)
// static
NativeTheme* NativeTheme::instance() {
  return NativeThemeAura::instance();
}

// static
NativeThemeAura* NativeThemeAura::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeAura, s_native_theme, ());
  return &s_native_theme;
}
#endif

NativeThemeAura::NativeThemeAura() {
  // We don't draw scrollbar buttons.
#if defined(OS_CHROMEOS)
  set_scrollbar_button_length(0);
#endif

  // Images and alphas declarations assume the following order.
  static_assert(kDisabled == 0, "states unexpectedly changed");
  static_assert(kHovered == 1, "states unexpectedly changed");
  static_assert(kNormal == 2, "states unexpectedly changed");
  static_assert(kPressed == 3, "states unexpectedly changed");
}

NativeThemeAura::~NativeThemeAura() {
}

// This implementation returns hardcoded colors.
SkColor NativeThemeAura::GetSystemColor(ColorId color_id) const {
  SkColor color;
  if (CommonThemeGetSystemColor(color_id, &color))
    return color;

  // Shared colors.
  static const SkColor kTextfieldDefaultBackground = SK_ColorWHITE;
  static const SkColor kTextfieldSelectionBackgroundFocused =
      SkColorSetARGB(0x54, 0x60, 0xA8, 0xEB);

  // MD colors.
  if (ui::MaterialDesignController::IsModeMaterial()) {
    // Dialogs:
    static const SkColor kDialogBackgroundColorMd = SK_ColorWHITE;
    // Results tables:
    static const SkColor kResultsTableSelectedBackgroundMd =
        SkColorSetARGB(0x15, 0x00, 0x00, 0x00);
    static const SkColor kResultsTableTextMd = SK_ColorBLACK;
    static const SkColor kResultsTableDimmedTextMd =
        SkColorSetRGB(0x64, 0x64, 0x64);
    static const SkColor kResultsTableUrlMd = SkColorSetRGB(0x33, 0x67, 0xD6);
    static const SkColor kResultsTableHoveredBackgroundMd =
        SkColorSetARGB(0x0D, 0x00, 0x00, 0x00);
    static const SkColor kResultsTableDividerMd = color_utils::AlphaBlend(
        kResultsTableTextMd, kTextfieldDefaultBackground, 0x34);
    static const SkColor kResultsTableSelectedDividerMd =
        color_utils::AlphaBlend(kResultsTableTextMd,
                                kTextfieldSelectionBackgroundFocused, 0x34);
    static const SkColor kPositiveTextColorMd = SkColorSetRGB(0x0b, 0x80, 0x43);
    static const SkColor kNegativeTextColorMd = SkColorSetRGB(0xc5, 0x39, 0x29);

    switch (color_id) {
      // Dialogs
      case kColorId_DialogBackground:
      case kColorId_BubbleBackground:
        return kDialogBackgroundColorMd;

      // Results Tables
      case kColorId_ResultsTableHoveredBackground:
        return kResultsTableHoveredBackgroundMd;
      case kColorId_ResultsTableSelectedBackground:
        return kResultsTableSelectedBackgroundMd;
      case kColorId_ResultsTableNormalText:
      case kColorId_ResultsTableHoveredText:
      case kColorId_ResultsTableSelectedText:
        return kResultsTableTextMd;
      case kColorId_ResultsTableNormalDimmedText:
      case kColorId_ResultsTableHoveredDimmedText:
      case kColorId_ResultsTableSelectedDimmedText:
        return kResultsTableDimmedTextMd;
      case kColorId_ResultsTableNormalUrl:
      case kColorId_ResultsTableHoveredUrl:
      case kColorId_ResultsTableSelectedUrl:
        return kResultsTableUrlMd;
      case kColorId_ResultsTableNormalDivider:
      case kColorId_ResultsTableHoveredDivider:
        return kResultsTableDividerMd;
      case kColorId_ResultsTableSelectedDivider:
        return kResultsTableSelectedDividerMd;
      case kColorId_ResultsTablePositiveText:
      case kColorId_ResultsTablePositiveHoveredText:
      case kColorId_ResultsTablePositiveSelectedText:
        return kPositiveTextColorMd;
      case kColorId_ResultsTableNegativeText:
      case kColorId_ResultsTableNegativeHoveredText:
      case kColorId_ResultsTableNegativeSelectedText:
        return kNegativeTextColorMd;

      default:
        break;
    }
  }

  // Pre-MD colors.
  static const SkColor kInvalidColorIdColor = SkColorSetRGB(255, 0, 128);
  // Windows:
  static const SkColor kWindowBackgroundColor = SK_ColorWHITE;
  // Dialogs:
  static const SkColor kDialogBackgroundColor = SkColorSetRGB(251, 251, 251);
  // FocusableBorder:
  static const SkColor kFocusedBorderColor = SkColorSetRGB(0x4D, 0x90, 0xFE);
  static const SkColor kUnfocusedBorderColor = SkColorSetRGB(0xD9, 0xD9, 0xD9);
  // Button:
  static const SkColor kButtonBackgroundColor = SkColorSetRGB(0xDE, 0xDE, 0xDE);
  static const SkColor kButtonEnabledColor = SkColorSetRGB(0x22, 0x22, 0x22);
  static const SkColor kButtonHighlightColor = SkColorSetRGB(0, 0, 0);
  static const SkColor kButtonHoverColor = kButtonEnabledColor;
  // Label:
  static const SkColor kLabelEnabledColor = kButtonEnabledColor;
  static const SkColor kLabelBackgroundColor = SK_ColorWHITE;
  // Link:
  static const SkColor kLinkDisabledColor = SK_ColorBLACK;
  static const SkColor kLinkEnabledColor = SkColorSetRGB(0, 51, 153);
  static const SkColor kLinkPressedColor = SK_ColorRED;
  // Textfield:
  static const SkColor kTextfieldDefaultColor = SK_ColorBLACK;
  static const SkColor kTextfieldReadOnlyColor = SK_ColorDKGRAY;
  static const SkColor kTextfieldReadOnlyBackground = SK_ColorWHITE;
  static const SkColor kTextfieldSelectionColor = color_utils::AlphaBlend(
      SK_ColorBLACK, kTextfieldSelectionBackgroundFocused, 0xdd);
  // Tooltip
  static const SkColor kTooltipBackground = 0xFFFFFFCC;
  static const SkColor kTooltipTextColor = kLabelEnabledColor;
  // Tree
  static const SkColor kTreeBackground = SK_ColorWHITE;
  static const SkColor kTreeTextColor = SK_ColorBLACK;
  static const SkColor kTreeSelectedTextColor = SK_ColorBLACK;
  static const SkColor kTreeSelectionBackgroundColor =
      SkColorSetRGB(0xEE, 0xEE, 0xEE);
  static const SkColor kTreeArrowColor = SkColorSetRGB(0x7A, 0x7A, 0x7A);
  // Table
  static const SkColor kTableBackground = SK_ColorWHITE;
  static const SkColor kTableTextColor = SK_ColorBLACK;
  static const SkColor kTableSelectedTextColor = SK_ColorBLACK;
  static const SkColor kTableSelectionBackgroundColor =
      SkColorSetRGB(0xEE, 0xEE, 0xEE);
  static const SkColor kTableGroupingIndicatorColor =
      SkColorSetRGB(0xCC, 0xCC, 0xCC);
  // Results Tables
  static const SkColor kResultsTableSelectedBackground =
      kTextfieldSelectionBackgroundFocused;
  static const SkColor kResultsTableNormalText =
      color_utils::AlphaBlend(SK_ColorBLACK, kTextfieldDefaultBackground, 0xDD);
  static const SkColor kResultsTableHoveredBackground = color_utils::AlphaBlend(
      kTextfieldSelectionBackgroundFocused, kTextfieldDefaultBackground, 0x40);
  static const SkColor kResultsTableHoveredText = color_utils::AlphaBlend(
      SK_ColorBLACK, kResultsTableHoveredBackground, 0xDD);
  static const SkColor kResultsTableSelectedText = color_utils::AlphaBlend(
      SK_ColorBLACK, kTextfieldSelectionBackgroundFocused, 0xDD);
  static const SkColor kResultsTableNormalDimmedText =
      color_utils::AlphaBlend(SK_ColorBLACK, kTextfieldDefaultBackground, 0xBB);
  static const SkColor kResultsTableHoveredDimmedText = color_utils::AlphaBlend(
      SK_ColorBLACK, kResultsTableHoveredBackground, 0xBB);
  static const SkColor kResultsTableSelectedDimmedText =
      color_utils::AlphaBlend(SK_ColorBLACK,
                              kTextfieldSelectionBackgroundFocused, 0xBB);
  static const SkColor kResultsTableNormalUrl = kTextfieldSelectionColor;
  static const SkColor kResultsTableSelectedOrHoveredUrl =
      SkColorSetARGB(0xff, 0x0b, 0x80, 0x43);
  static const SkColor kResultsTableNormalDivider = color_utils::AlphaBlend(
      kResultsTableNormalText, kTextfieldDefaultBackground, 0x34);
  static const SkColor kResultsTableHoveredDivider = color_utils::AlphaBlend(
      kResultsTableHoveredText, kResultsTableHoveredBackground, 0x34);
  static const SkColor kResultsTableSelectedDivider = color_utils::AlphaBlend(
      kResultsTableSelectedText, kTextfieldSelectionBackgroundFocused, 0x34);
  const SkColor kPositiveTextColor = SkColorSetRGB(0x0b, 0x80, 0x43);
  const SkColor kNegativeTextColor = SkColorSetRGB(0xc5, 0x39, 0x29);
  static const SkColor kResultsTablePositiveText = color_utils::AlphaBlend(
      kPositiveTextColor, kTextfieldDefaultBackground, 0xDD);
  static const SkColor kResultsTablePositiveHoveredText =
      color_utils::AlphaBlend(kPositiveTextColor,
                              kResultsTableHoveredBackground, 0xDD);
  static const SkColor kResultsTablePositiveSelectedText =
      color_utils::AlphaBlend(kPositiveTextColor,
                              kTextfieldSelectionBackgroundFocused, 0xDD);
  static const SkColor kResultsTableNegativeText = color_utils::AlphaBlend(
      kNegativeTextColor, kTextfieldDefaultBackground, 0xDD);
  static const SkColor kResultsTableNegativeHoveredText =
      color_utils::AlphaBlend(kNegativeTextColor,
                              kResultsTableHoveredBackground, 0xDD);
  static const SkColor kResultsTableNegativeSelectedText =
      color_utils::AlphaBlend(kNegativeTextColor,
                              kTextfieldSelectionBackgroundFocused, 0xDD);

  switch (color_id) {
    // Windows
    case kColorId_WindowBackground:
      return kWindowBackgroundColor;

    // Dialogs
    case kColorId_DialogBackground:
    case kColorId_BubbleBackground:
      return kDialogBackgroundColor;

    // FocusableBorder
    case kColorId_FocusedBorderColor:
      return kFocusedBorderColor;
    case kColorId_UnfocusedBorderColor:
      return kUnfocusedBorderColor;

    // Button
    case kColorId_ButtonBackgroundColor:
      return kButtonBackgroundColor;
    case kColorId_ButtonEnabledColor:
      return kButtonEnabledColor;
    case kColorId_ButtonHighlightColor:
      return kButtonHighlightColor;
    case kColorId_ButtonHoverColor:
      return kButtonHoverColor;

    // Label
    case kColorId_LabelEnabledColor:
      return kLabelEnabledColor;
    case kColorId_LabelDisabledColor:
      return GetSystemColor(kColorId_ButtonDisabledColor);
    case kColorId_LabelBackgroundColor:
      return kLabelBackgroundColor;

    // Link
    case kColorId_LinkDisabled:
      return kLinkDisabledColor;
    case kColorId_LinkEnabled:
      return kLinkEnabledColor;
    case kColorId_LinkPressed:
      return kLinkPressedColor;

    // Textfield
    case kColorId_TextfieldDefaultColor:
      return kTextfieldDefaultColor;
    case kColorId_TextfieldDefaultBackground:
      return kTextfieldDefaultBackground;
    case kColorId_TextfieldReadOnlyColor:
      return kTextfieldReadOnlyColor;
    case kColorId_TextfieldReadOnlyBackground:
      return kTextfieldReadOnlyBackground;
    case kColorId_TextfieldSelectionColor:
      return kTextfieldSelectionColor;
    case kColorId_TextfieldSelectionBackgroundFocused:
      return kTextfieldSelectionBackgroundFocused;

    // Tooltip
    case kColorId_TooltipBackground:
      return kTooltipBackground;
    case kColorId_TooltipText:
      return kTooltipTextColor;

    // Tree
    case kColorId_TreeBackground:
      return kTreeBackground;
    case kColorId_TreeText:
      return kTreeTextColor;
    case kColorId_TreeSelectedText:
    case kColorId_TreeSelectedTextUnfocused:
      return kTreeSelectedTextColor;
    case kColorId_TreeSelectionBackgroundFocused:
    case kColorId_TreeSelectionBackgroundUnfocused:
      return kTreeSelectionBackgroundColor;
    case kColorId_TreeArrow:
      return kTreeArrowColor;

    // Table
    case kColorId_TableBackground:
      return kTableBackground;
    case kColorId_TableText:
      return kTableTextColor;
    case kColorId_TableSelectedText:
    case kColorId_TableSelectedTextUnfocused:
      return kTableSelectedTextColor;
    case kColorId_TableSelectionBackgroundFocused:
    case kColorId_TableSelectionBackgroundUnfocused:
      return kTableSelectionBackgroundColor;
    case kColorId_TableGroupingIndicatorColor:
      return kTableGroupingIndicatorColor;

    // Results Tables
    case kColorId_ResultsTableNormalBackground:
      return kTextfieldDefaultBackground;
    case kColorId_ResultsTableHoveredBackground:
      return kResultsTableHoveredBackground;
    case kColorId_ResultsTableSelectedBackground:
      return kResultsTableSelectedBackground;
    case kColorId_ResultsTableNormalText:
      return kResultsTableNormalText;
    case kColorId_ResultsTableHoveredText:
      return kResultsTableHoveredText;
    case kColorId_ResultsTableSelectedText:
      return kResultsTableSelectedText;
    case kColorId_ResultsTableNormalDimmedText:
      return kResultsTableNormalDimmedText;
    case kColorId_ResultsTableHoveredDimmedText:
      return kResultsTableHoveredDimmedText;
    case kColorId_ResultsTableSelectedDimmedText:
      return kResultsTableSelectedDimmedText;
    case kColorId_ResultsTableNormalUrl:
      return kResultsTableNormalUrl;
    case kColorId_ResultsTableHoveredUrl:
    case kColorId_ResultsTableSelectedUrl:
      return kResultsTableSelectedOrHoveredUrl;
    case kColorId_ResultsTableNormalDivider:
      return kResultsTableNormalDivider;
    case kColorId_ResultsTableHoveredDivider:
      return kResultsTableHoveredDivider;
    case kColorId_ResultsTableSelectedDivider:
      return kResultsTableSelectedDivider;
    case kColorId_ResultsTablePositiveText:
      return kResultsTablePositiveText;
    case kColorId_ResultsTablePositiveHoveredText:
      return kResultsTablePositiveHoveredText;
    case kColorId_ResultsTablePositiveSelectedText:
      return kResultsTablePositiveSelectedText;
    case kColorId_ResultsTableNegativeText:
      return kResultsTableNegativeText;
    case kColorId_ResultsTableNegativeHoveredText:
      return kResultsTableNegativeHoveredText;
    case kColorId_ResultsTableNegativeSelectedText:
      return kResultsTableNegativeSelectedText;

    default:
      NOTREACHED();
      break;
  }

  return kInvalidColorIdColor;
}

void NativeThemeAura::PaintMenuPopupBackground(
    SkCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background) const {
  SkColor color = GetSystemColor(NativeTheme::kColorId_MenuBackgroundColor);
  if (menu_background.corner_radius > 0) {
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(color);

    gfx::Path path;
    SkRect rect = SkRect::MakeWH(SkIntToScalar(size.width()),
                                 SkIntToScalar(size.height()));
    SkScalar radius = SkIntToScalar(menu_background.corner_radius);
    SkScalar radii[8] = {radius, radius, radius, radius,
                         radius, radius, radius, radius};
    path.addRoundRect(rect, radii);

    canvas->drawPath(path, paint);
  } else {
    canvas->drawColor(color, SkXfermode::kSrc_Mode);
  }
}

void NativeThemeAura::PaintMenuItemBackground(
    SkCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuListExtraParams& menu_list) const {
  CommonThemePaintMenuItemBackground(canvas, state, rect);
}

void NativeThemeAura::PaintArrowButton(
      SkCanvas* gc,
      const gfx::Rect& rect,
      Part direction,
      State state) const {
  if (direction == kInnerSpinButton) {
    NativeThemeBase::PaintArrowButton(gc, rect, direction, state);
    return;
  }
  PaintPainter(GetOrCreatePainter(
                   kScrollbarArrowButtonImages, state,
                   scrollbar_arrow_button_painters_),
               gc, rect);

  // Aura-win uses slightly different arrow colors.
  SkColor arrow_color = GetArrowColor(state);
  switch (state) {
    case kHovered:
    case kNormal:
      arrow_color = SkColorSetRGB(0x50, 0x50, 0x50);
      break;
    case kPressed:
      arrow_color = SK_ColorWHITE;
    default:
      break;
  }
  PaintArrow(gc, rect, direction, arrow_color);
}

void NativeThemeAura::PaintScrollbarTrack(
    SkCanvas* sk_canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect) const {
  // Overlay Scrollbar should never paint a scrollbar track.
  DCHECK(!IsOverlayScrollbarEnabled());
  if (!scrollbar_track_painter_)
    scrollbar_track_painter_ = CreateNineImagePainter(kScrollbarTrackImages);
  PaintPainter(scrollbar_track_painter_.get(), sk_canvas, rect);
}

void NativeThemeAura::PaintScrollbarThumb(SkCanvas* sk_canvas,
                                          Part part,
                                          State state,
                                          const gfx::Rect& rect) const {
  gfx::Rect thumb_rect(rect);
  if (IsOverlayScrollbarEnabled()) {
    // Overlay scrollbar has no track, just paint thumb directly.
    // Do not paint if state is disabled.
    if (state == kDisabled)
      return;

    if (!scrollbar_overlay_thumb_painter_) {
      scrollbar_overlay_thumb_painter_ =
          CreateDualPainter(kScrollbarOverlayThumbFillImages,
                            kScrollbarOverlayThumbFillAlphas,
                            kScrollbarOverlayThumbStrokeImages,
                            kScrollbarOverlayThumbStrokeAlphas);
    }

    PaintDualPainter(
        scrollbar_overlay_thumb_painter_.get(), sk_canvas, thumb_rect, state);
    return;
  }
  // If there are no scrollbuttons then provide some padding so that thumb
  // doesn't touch the top of the track.
  const int extra_padding = (scrollbar_button_length() == 0) ? 2 : 0;
  if (part == NativeTheme::kScrollbarVerticalThumb)
    thumb_rect.Inset(2, extra_padding, 2, extra_padding);
  else
    thumb_rect.Inset(extra_padding, 2, extra_padding, 2);
  PaintPainter(GetOrCreatePainter(
                   kScrollbarThumbImages, state, scrollbar_thumb_painters_),
               sk_canvas,
               thumb_rect);
}

void NativeThemeAura::PaintScrollbarThumbStateTransition(
    SkCanvas* canvas,
    State startState,
    State endState,
    double progress,
    const gfx::Rect& rect) const {
  // Only Overlay scrollbars should have state transition animation.
  DCHECK(IsOverlayScrollbarEnabled());
  if (!scrollbar_overlay_thumb_painter_) {
    scrollbar_overlay_thumb_painter_ =
        CreateDualPainter(kScrollbarOverlayThumbFillImages,
                          kScrollbarOverlayThumbFillAlphas,
                          kScrollbarOverlayThumbStrokeImages,
                          kScrollbarOverlayThumbStrokeAlphas);
  }

  PaintDualPainterTransition(scrollbar_overlay_thumb_painter_.get(),
                             canvas,
                             rect,
                             startState,
                             endState,
                             progress);
}

void NativeThemeAura::PaintScrollbarCorner(SkCanvas* canvas,
                                           State state,
                                           const gfx::Rect& rect) const {
  // Overlay Scrollbar should never paint a scrollbar corner.
  DCHECK(!IsOverlayScrollbarEnabled());
  SkPaint paint;
  paint.setColor(SkColorSetRGB(0xF1, 0xF1, 0xF1));
  paint.setStyle(SkPaint::kFill_Style);
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  canvas->drawIRect(RectToSkIRect(rect), paint);
}

NineImagePainter* NativeThemeAura::GetOrCreatePainter(
    const int images[kNumStates][9],
    State state,
    scoped_ptr<NineImagePainter> painters[kNumStates]) const {
  if (painters[state])
    return painters[state].get();
  if (images[state][0] == 0) {
    // Must always provide normal state images.
    DCHECK_NE(kNormal, state);
    return GetOrCreatePainter(images, kNormal, painters);
  }
  painters[state] = CreateNineImagePainter(images[state]);
  return painters[state].get();
}

void NativeThemeAura::PaintPainter(NineImagePainter* painter,
                                   SkCanvas* sk_canvas,
                                   const gfx::Rect& rect) const {
  DCHECK(painter);
  scoped_ptr<gfx::Canvas> canvas(CommonThemeCreateCanvas(sk_canvas));
  painter->Paint(canvas.get(), rect);
}

scoped_ptr<NativeThemeAura::DualPainter> NativeThemeAura::CreateDualPainter(
    const int fill_image_ids[9],
    const uint8 fill_alphas[kNumStates],
    const int stroke_image_ids[9],
    const uint8 stroke_alphas[kNumStates]) const {
  scoped_ptr<NativeThemeAura::DualPainter> dual_painter(
      new NativeThemeAura::DualPainter(CreateNineImagePainter(fill_image_ids),
                                       fill_alphas,
                                       CreateNineImagePainter(stroke_image_ids),
                                       stroke_alphas));
  return dual_painter.Pass();
}

void NativeThemeAura::PaintDualPainter(
    NativeThemeAura::DualPainter* dual_painter,
    SkCanvas* sk_canvas,
    const gfx::Rect& rect,
    State state) const {
  DCHECK(dual_painter);
  scoped_ptr<gfx::Canvas> canvas(CommonThemeCreateCanvas(sk_canvas));
  dual_painter->fill_painter->Paint(
      canvas.get(), rect, dual_painter->fill_alphas[state]);
  dual_painter->stroke_painter->Paint(
      canvas.get(), rect, dual_painter->stroke_alphas[state]);
}

void NativeThemeAura::PaintDualPainterTransition(
    NativeThemeAura::DualPainter* dual_painter,
    SkCanvas* sk_canvas,
    const gfx::Rect& rect,
    State startState,
    State endState,
    double progress) const {
  DCHECK(dual_painter);
  scoped_ptr<gfx::Canvas> canvas(CommonThemeCreateCanvas(sk_canvas));
  uint8 fill_alpha = dual_painter->fill_alphas[startState] +
                     (dual_painter->fill_alphas[endState] -
                      dual_painter->fill_alphas[startState]) *
                         progress;
  uint8 stroke_alpha = dual_painter->stroke_alphas[startState] +
                       (dual_painter->stroke_alphas[endState] -
                        dual_painter->stroke_alphas[startState]) *
                           progress;

  dual_painter->fill_painter->Paint(canvas.get(), rect, fill_alpha);
  dual_painter->stroke_painter->Paint(canvas.get(), rect, stroke_alpha);
}

NativeThemeAura::DualPainter::DualPainter(
    scoped_ptr<NineImagePainter> fill_painter,
    const uint8 fill_alphas[kNumStates],
    scoped_ptr<NineImagePainter> stroke_painter,
    const uint8 stroke_alphas[kNumStates])
    : fill_painter(fill_painter.Pass()),
      fill_alphas(fill_alphas),
      stroke_painter(stroke_painter.Pass()),
      stroke_alphas(stroke_alphas) {}

NativeThemeAura::DualPainter::~DualPainter() {}

}  // namespace ui
