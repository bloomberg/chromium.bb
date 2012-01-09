// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_THEME_BASE_H_
#define UI_GFX_NATIVE_THEME_BASE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/native_theme.h"

namespace gfx {
class Rect;
class Size;

// Theme support for non-Windows toolkits.
class NativeThemeBase : public NativeTheme {
 public:
  // NativeTheme implementation:
  virtual gfx::Size GetPartSize(Part part,
                                State state,
                                const ExtraParams& extra) const OVERRIDE;
  virtual void Paint(SkCanvas* canvas,
                     Part part,
                     State state,
                     const gfx::Rect& rect,
                     const ExtraParams& extra) const OVERRIDE;
  virtual SkColor GetSystemColor(ColorId color_id) const OVERRIDE;

 protected:
  NativeThemeBase();
  virtual ~NativeThemeBase();

  // Draw the arrow. Used by scrollbar and inner spin button.
  virtual void PaintArrowButton(
      SkCanvas* gc,
      const gfx::Rect& rect,
      Part direction,
      State state) const;
  // Paint the scrollbar track. Done before the thumb so that it can contain
  // alpha.
  virtual void PaintScrollbarTrack(
      SkCanvas* canvas,
      Part part,
      State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect) const;
  // Draw the scrollbar thumb over the track.
  virtual void PaintScrollbarThumb(
      SkCanvas* canvas,
      Part part,
      State state,
      const gfx::Rect& rect) const;

  virtual void PaintCheckbox(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const;

  virtual void PaintRadio(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const;

  virtual void PaintButton(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const;

  virtual void PaintTextField(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const TextFieldExtraParams& text) const;

  virtual void PaintMenuList(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuListExtraParams& menu_list) const;

  virtual void PaintMenuPopupBackground(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuListExtraParams& menu_list) const;

  virtual void PaintMenuItemBackground(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuListExtraParams& menu_list) const;

  virtual void PaintSliderTrack(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider) const;

  virtual void PaintSliderThumb(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider) const;

  virtual void PaintInnerSpinButton(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const InnerSpinButtonExtraParams& spin_button) const;

  virtual void PaintProgressBar(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ProgressBarExtraParams& progress_bar) const;

 protected:
  bool IntersectsClipRectInt(SkCanvas* canvas,
                             int x, int y, int w, int h) const;

  void DrawBitmapInt(SkCanvas* canvas, const SkBitmap& bitmap,
                     int src_x, int src_y, int src_w, int src_h,
                     int dest_x, int dest_y, int dest_w, int dest_h) const;

  void DrawTiledImage(SkCanvas* canvas,
                      const SkBitmap& bitmap,
                      int src_x, int src_y,
                      double tile_scale_x, double tile_scale_y,
                      int dest_x, int dest_y, int w, int h) const;

  SkColor SaturateAndBrighten(SkScalar* hsv,
                              SkScalar saturate_amount,
                              SkScalar brighten_amount) const;

 private:
  void DrawVertLine(SkCanvas* canvas,
                    int x,
                    int y1,
                    int y2,
                    const SkPaint& paint) const;
  void DrawHorizLine(SkCanvas* canvas,
                     int x1,
                     int x2,
                     int y,
                     const SkPaint& paint) const;
  void DrawBox(SkCanvas* canvas,
               const gfx::Rect& rect,
               const SkPaint& paint) const;
  SkScalar Clamp(SkScalar value,
                 SkScalar min,
                 SkScalar max) const;
  SkColor OutlineColor(SkScalar* hsv1, SkScalar* hsv2) const;

  static unsigned int scrollbar_width_;
  static unsigned int button_length_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeBase);
};

}  // namespace gfx

#endif  // UI_GFX_NATIVE_THEME_BASE_H_
