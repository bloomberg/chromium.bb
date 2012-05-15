// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NATIVE_THEME_NATIVE_THEME_AURA_H_
#define UI_BASE_NATIVE_THEME_NATIVE_THEME_AURA_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/native_theme/native_theme_base.h"

namespace ui {

// Aura implementation of native theme support.
class NativeThemeAura : public NativeThemeBase {
 public:
  static const NativeThemeAura* instance();

 private:
  NativeThemeAura();
  virtual ~NativeThemeAura();

  // Overridden from NativeThemeBase:
  virtual SkColor GetSystemColor(ColorId color_id) const OVERRIDE;
  virtual void PaintMenuPopupBackground(SkCanvas* canvas,
                                        const gfx::Size& size) const OVERRIDE;
  virtual void PaintScrollbarTrack(
      SkCanvas* canvas,
      Part part,
      State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect) const OVERRIDE;
  virtual void PaintArrowButton(SkCanvas* canvas,
                                const gfx::Rect& rect,
                                Part direction,
                                State state) const OVERRIDE;
  virtual void PaintScrollbarThumb(SkCanvas* canvas,
                                   Part part,
                                   State state,
                                   const gfx::Rect& rect) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeAura);
};

}  // namespace ui

#endif  // UI_BASE_NATIVE_THEME_NATIVE_THEME_AURA_H_
