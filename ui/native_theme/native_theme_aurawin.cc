// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_aurawin.h"

#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/nine_image_painter.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/win/dpi.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_win.h"

using gfx::NineImagePainter;

#define EMPTY_IMAGE_GRID { 0, 0, 0, 0, 0, 0, 0, 0, 0 }

namespace ui {

namespace {

const int kScrollbarThumbImages[NativeTheme::kMaxState][9] = {
  EMPTY_IMAGE_GRID,
  IMAGE_GRID(IDR_SCROLLBAR_THUMB_BASE_HOVER),
  IMAGE_GRID(IDR_SCROLLBAR_THUMB_BASE_NORMAL),
  IMAGE_GRID(IDR_SCROLLBAR_THUMB_BASE_PRESSED)
};

const int kScrollbarArrowButtonImages[NativeTheme::kMaxState][9] = {
  EMPTY_IMAGE_GRID,
  IMAGE_GRID(IDR_SCROLLBAR_ARROW_BUTTON_BASE_HOVER),
  IMAGE_GRID(IDR_SCROLLBAR_ARROW_BUTTON_BASE_NORMAL),
  IMAGE_GRID(IDR_SCROLLBAR_ARROW_BUTTON_BASE_PRESSED)
};

const int kScrollbarTrackImages[9] = IMAGE_GRID(IDR_SCROLLBAR_BASE);

}  // namespace

// static
NativeTheme* NativeTheme::instance() {
  return NativeThemeAuraWin::instance();
}

// static
NativeThemeAura* NativeThemeAura::instance() {
  return NativeThemeAuraWin::instance();
}

// static
NativeThemeAuraWin* NativeThemeAuraWin::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeAuraWin, s_native_theme, ());
  return &s_native_theme;
}

NativeThemeAuraWin::NativeThemeAuraWin() {
  // Image declarations assume the following order.
  COMPILE_ASSERT(kDisabled == 0, states_unexepctedly_changed);
  COMPILE_ASSERT(kHovered == 1, states_unexepctedly_changed);
  COMPILE_ASSERT(kNormal == 2, states_unexepctedly_changed);
  COMPILE_ASSERT(kPressed == 3, states_unexepctedly_changed);
  COMPILE_ASSERT(kMaxState == 4, states_unexepctedly_changed);
}

NativeThemeAuraWin::~NativeThemeAuraWin() {
}

gfx::Size NativeThemeAuraWin::GetPartSize(Part part,
                                          State state,
                                          const ExtraParams& extra) const {
  gfx::Size part_size = CommonThemeGetPartSize(part, state, extra);
  if (!part_size.IsEmpty())
    return part_size;

  // We want aura on windows to use the same size for scrollbars as we would in
  // the native theme.
  switch (part) {
    case kScrollbarDownArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
    case kScrollbarUpArrow:
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      return NativeThemeWin::instance()->GetPartSize(part, state, extra);
    default:
      break;
  }

  return NativeThemeAura::GetPartSize(part, state, extra);
}

void NativeThemeAuraWin::PaintArrowButton(
      SkCanvas* gc,
      const gfx::Rect& rect,
      Part direction,
      State state) const {
  if (direction == kInnerSpinButton) {
    NativeThemeAura::PaintArrowButton(gc, rect, direction, state);
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

void NativeThemeAuraWin::PaintScrollbarTrack(
    SkCanvas* sk_canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect) const {
  if (!scrollbar_track_painter_)
    scrollbar_track_painter_ = CreateNineImagePainter(kScrollbarTrackImages);
  PaintPainter(scrollbar_track_painter_.get(), sk_canvas, rect);
}

void NativeThemeAuraWin::PaintScrollbarThumb(SkCanvas* sk_canvas,
                                             Part part,
                                             State state,
                                             const gfx::Rect& rect) const {
  gfx::Rect thumb_rect(rect);
  if (part == NativeTheme::kScrollbarVerticalThumb)
    thumb_rect.Inset(2, 0, 2, 0);
  else
    thumb_rect.Inset(0, 2, 0, 2);
  PaintPainter(GetOrCreatePainter(
                   kScrollbarThumbImages, state, scrollbar_thumb_painters_),
               sk_canvas, thumb_rect);
}

void NativeThemeAuraWin::PaintScrollbarCorner(SkCanvas* canvas,
                                              State state,
                                              const gfx::Rect& rect) const {
  canvas->drawColor(SkColorSetRGB(0xF1, 0xF1, 0xF1), SkXfermode::kSrc_Mode);
}

NineImagePainter* NativeThemeAuraWin::GetOrCreatePainter(
    const int images[kMaxState][9],
    State state,
    scoped_ptr<NineImagePainter> painters[kMaxState]) const {
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

void NativeThemeAuraWin::PaintPainter(NineImagePainter* painter,
                                      SkCanvas* sk_canvas,
                                      const gfx::Rect& rect) const {
  DCHECK(painter);
  scoped_ptr<gfx::Canvas> canvas(CreateCanvas(sk_canvas));
  painter->Paint(canvas.get(), rect);
}

}  // namespace ui
