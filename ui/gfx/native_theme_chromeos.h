// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_THEME_CHROMEOS_H_
#define UI_GFX_NATIVE_THEME_CHROMEOS_H_

#include <map>
#include "base/compiler_specific.h"
#include "ui/gfx/native_theme_linux.h"

class SkBitmap;

class NativeThemeChromeos : public gfx::NativeThemeLinux {
 private:
  friend class NativeThemeLinux;
  NativeThemeChromeos();
  virtual ~NativeThemeChromeos();

  // NativeTheme overrides
  virtual gfx::Size GetPartSize(Part part,
                                State state,
                                const ExtraParams& extra) const OVERRIDE;

  // NativeThemeLinux overrides
  virtual void PaintScrollbarTrack(SkCanvas* canvas,
      Part part, State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect) const OVERRIDE;
  virtual void PaintArrowButton(SkCanvas* canvas,
      const gfx::Rect& rect, Part direction, State state) const OVERRIDE;
  virtual void PaintScrollbarThumb(SkCanvas* canvas,
      Part part, State state, const gfx::Rect& rect) const OVERRIDE;

  // Draw the checkbox.
  virtual void PaintCheckbox(SkCanvas* canvas,
      State state, const gfx::Rect& rect,
      const ButtonExtraParams& button) const OVERRIDE;

  // Draw the radio.
  virtual void PaintRadio(SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const OVERRIDE;

  // Draw the push button.
  virtual void PaintButton(SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) const OVERRIDE;

  // Draw the text field.
  virtual void PaintTextField(SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const TextFieldExtraParams& text) const OVERRIDE;

  // Draw the slider track.
  virtual void PaintSliderTrack(SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider) const OVERRIDE;

  // Draw the slider thumb.
  virtual void PaintSliderThumb(SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider) const OVERRIDE;

  // Draw the inner spin button.
  virtual void PaintInnerSpinButton(SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const InnerSpinButtonExtraParams& spin_button) const OVERRIDE;

  // Draw the progress bar.
  virtual void PaintProgressBar(SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ProgressBarExtraParams& progress_bar) const OVERRIDE;

  SkBitmap* GetHorizontalBitmapNamed(int resource_id) const;

  // Paint a button like rounded rect with gradient background and stroke.
  void PaintButtonLike(SkCanvas* canvas,
      State state, const gfx::Rect& rect, bool stroke_border) const;

  // Cached images. Resource loader caches all retrieved bitmaps and keeps
  // ownership of the pointers.
  typedef std::map<int, SkBitmap*> SkImageMap;
  mutable SkImageMap horizontal_bitmaps_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeChromeos);
};

#endif  // UI_GFX_NATIVE_THEME_CHROMEOS_H_
