// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_THEME_LINUX_H_
#define UI_GFX_NATIVE_THEME_LINUX_H_

#include "base/basictypes.h"
#include "skia/ext/platform_canvas.h"

namespace skia {
class PlatformCanvas;
}

namespace gfx {
class Rect;
class Size;

// Linux theming API.
class NativeThemeLinux {
 public:
  // The part to be painted / sized.
  enum Part {
    kScrollbarDownArrow,
    kScrollbarLeftArrow,
    kScrollbarRightArrow,
    kScrollbarUpArrow,
    kScrollbarHorizontalThumb,
    kScrollbarVerticalThumb,
    kScrollbarHorizontalTrack,
    kScrollbarVerticalTrack,
    kCheckbox,
    kRadio,
    kPushButton,
    kTextField,
    kMenuList,
    kSliderTrack,
    kSliderThumb,
    kInnerSpinButton,
    kProgressBar,
  };

  // The state of the part.
  enum State {
    kDisabled,
    kHovered,
    kNormal,
    kPressed,
  };

  // Extra data needed to draw scrollbar track correctly.
  struct ScrollbarTrackExtraParams {
    int track_x;
    int track_y;
    int track_width;
    int track_height;
  };

  struct ButtonExtraParams {
    bool checked;
    bool indeterminate;  // Whether the button state is indeterminate.
    bool is_default;     // Whether the button is default button.
    bool has_border;
    SkColor background_color;
  };

  struct TextFieldExtraParams {
    bool is_text_area;
    bool is_listbox;
    SkColor background_color;
  };

  struct MenuListExtraParams {
    bool has_border;
    bool has_border_radius;
    int arrow_x;
    int arrow_y;
    SkColor background_color;
  };

  struct SliderExtraParams {
    bool vertical;
    bool in_drag;
  };

  struct InnerSpinButtonExtraParams {
    bool spin_up;
    bool read_only;
  };

  struct ProgressBarExtraParams {
    bool determinate;
    int value_rect_x;
    int value_rect_y;
    int value_rect_width;
    int value_rect_height;
  };

  union ExtraParams {
    ScrollbarTrackExtraParams scrollbar_track;
    ButtonExtraParams button;
    MenuListExtraParams menu_list;
    SliderExtraParams slider;
    TextFieldExtraParams text_field;
    InnerSpinButtonExtraParams inner_spin;
    ProgressBarExtraParams progress_bar;
  };

  // Gets our singleton instance.
  static NativeThemeLinux* instance();

  // Return the size of the part.
  virtual gfx::Size GetPartSize(Part part) const;
  // Paint the part to the canvas.
  virtual void Paint(skia::PlatformCanvas* canvas,
                     Part part,
                     State state,
                     const gfx::Rect& rect,
                     const ExtraParams& extra);
  // Supports theme specific colors.
  void SetScrollbarColors(unsigned inactive_color,
                          unsigned active_color,
                          unsigned track_color) const;

 protected:
  NativeThemeLinux();
  virtual ~NativeThemeLinux();

  // Draw the arrow. Used by scrollbar and inner spin button.
  virtual void PaintArrowButton(
      skia::PlatformCanvas* gc,
      const gfx::Rect& rect,
      Part direction,
      State state);
  // Paint the scrollbar track. Done before the thumb so that it can contain
  // alpha.
  virtual void PaintScrollbarTrack(skia::PlatformCanvas* canvas,
      Part part,
      State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect);
  // Draw the scrollbar thumb over the track.
  virtual void PaintScrollbarThumb(skia::PlatformCanvas* canvas,
      Part part,
      State state,
      const gfx::Rect& rect);
  // Draw the checkbox.
  virtual void PaintCheckbox(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button);
  // Draw the radio.
  virtual void PaintRadio(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button);
  // Draw the push button.
  virtual void PaintButton(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button);
  // Draw the text field.
  virtual void PaintTextField(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const TextFieldExtraParams& text);
  // Draw the menu list.
  virtual void PaintMenuList(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuListExtraParams& menu_list);
  // Draw the slider track.
  virtual void PaintSliderTrack(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider);
  // Draw the slider thumb.
  virtual void PaintSliderThumb(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider);
  // Draw the inner spin button.
  virtual void PaintInnerSpinButton(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const InnerSpinButtonExtraParams& spin_button);
  // Draw the progress bar.
  virtual void PaintProgressBar(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ProgressBarExtraParams& progress_bar);

 protected:
  bool IntersectsClipRectInt(skia::PlatformCanvas* canvas,
                             int x, int y, int w, int h);

  void DrawBitmapInt(skia::PlatformCanvas* canvas, const SkBitmap& bitmap,
                     int src_x, int src_y, int src_w, int src_h,
                     int dest_x, int dest_y, int dest_w, int dest_h);

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
  static unsigned int thumb_inactive_color_;
  static unsigned int thumb_active_color_;
  static unsigned int track_color_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeLinux);
};

}  // namespace gfx

#endif  // UI_GFX_NATIVE_THEME_LINUX_H_
