// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_THEME_ANDROID_H_
#define UI_GFX_NATIVE_THEME_ANDROID_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/native_theme.h"

namespace gfx {
class Rect;
class Size;

// Android theming API.
class NativeThemeAndroid : public NativeTheme {
 public:
  // Gets our singleton instance.
  static const NativeThemeAndroid* instance();

  // Return the size of the part.
  virtual Size GetPartSize(Part part,
                           State state,
                           const ExtraParams& extra) const OVERRIDE;

  // Paint the part to the canvas.
  virtual void Paint(SkCanvas* canvas,
                     Part part,
                     State state,
                     const gfx::Rect& rect,
                     const ExtraParams& extra) const OVERRIDE;

  virtual SkColor GetSystemColor(ColorId color_id) const OVERRIDE;

 private:
  NativeThemeAndroid();
  virtual ~NativeThemeAndroid();

  // Draw the arrow. Used by scrollbar and inner spin button.
  void PaintArrowButton(SkCanvas* gc,
                        const gfx::Rect& rect,
                        Part direction,
                        State state) const;

  // Draw the checkbox.
  void PaintCheckbox(SkCanvas* canvas,
                     State state,
                     const gfx::Rect& rect,
                     const ButtonExtraParams& button) const;

  // Draw the radio.
  void PaintRadio(SkCanvas* canvas,
                  State state,
                  const gfx::Rect& rect,
                  const ButtonExtraParams& button) const;

  // Draw the push button.
  void PaintButton(SkCanvas* canvas,
                   State state,
                   const gfx::Rect& rect,
                   const ButtonExtraParams& button) const;

  // Draw the text field.
  void PaintTextField(SkCanvas* canvas,
                      State state,
                      const gfx::Rect& rect,
                      const TextFieldExtraParams& text) const;

  // Draw the menu list.
  void PaintMenuList(SkCanvas* canvas,
                     State state,
                     const gfx::Rect& rect,
                     const MenuListExtraParams& menu_list) const;

  // Draw the slider track.
  void PaintSliderTrack(SkCanvas* canvas,
                        State state,
                        const gfx::Rect& rect,
                        const SliderExtraParams& slider) const;

  // Draw the slider thumb.
  void PaintSliderThumb(SkCanvas* canvas,
                        State state,
                        const gfx::Rect& rect,
                        const SliderExtraParams& slider) const;

  // Draw the inner spin button.
  void PaintInnerSpinButton(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const InnerSpinButtonExtraParams& spin_button) const;

  // Draw the progress bar.
  void PaintProgressBar(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ProgressBarExtraParams& progress_bar) const;

  // Return true if there is intersection between the |canvas| and the given
  // rectangle.
  bool IntersectsClipRectInt(SkCanvas* canvas,
                             int x,
                             int y,
                             int w,
                             int h) const;

  // Draw the dest rectangle with the given bitmap which might be scaled if its
  // size is not same as target rectangle.
  void DrawBitmapInt(SkCanvas* canvas,
                     const SkBitmap& bitmap,
                     int src_x,
                     int src_y,
                     int src_w,
                     int src_h,
                     int dest_x,
                     int dest_y,
                     int dest_w,
                     int dest_h) const;

  // Draw the target rectangle with the |bitmap| accroding the given
  // |tile_scale_x| and |tile_scale_y|
  void DrawTiledImage(SkCanvas* canvas,
                      const SkBitmap& bitmap,
                      int src_x,
                      int src_y,
                      double tile_scale_x,
                      double tile_scale_y,
                      int dest_x,
                      int dest_y,
                      int w,
                      int h) const;

  // Return a new color which comes from the |hsv| by adjusting saturate and
  // brighten according |saturate_amount| and |brighten_amount|
  SkColor SaturateAndBrighten(SkScalar* hsv,
                              SkScalar saturate_amount,
                              SkScalar brighten_amount) const;

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

  // Return the |value| if it is between |min| and |max|, otherwise the |min|
  // or |max| is returned dependent on which is mostly near the |value|.
  SkScalar Clamp(SkScalar value, SkScalar min, SkScalar max) const;

  // Used to return the color of scrollbar based on the color of thumb and
  // track.
  SkColor OutlineColor(SkScalar* hsv1, SkScalar* hsv2) const;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeAndroid);
};

}  // namespace gfx

#endif  // UI_GFX_NATIVE_THEME_ANDROID_H_
