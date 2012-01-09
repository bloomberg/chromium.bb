// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_THEME_AURA_H_
#define UI_GFX_NATIVE_THEME_AURA_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "ui/gfx/native_theme_base.h"
#include "ui/gfx/rect.h"

class SkBitmap;

namespace gfx {

// Aura implementation of native theme support.
class NativeThemeAura : public NativeThemeBase {
 public:
  static const NativeThemeAura* instance();

 private:
  NativeThemeAura();
  virtual ~NativeThemeAura();

  // NativeThemeBase overrides
  virtual void PaintMenuPopupBackground(
      SkCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuListExtraParams& menu_list) const OVERRIDE;

  virtual void PaintScrollbarTrack(
      SkCanvas* canvas,
      Part part,
      State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect) const OVERRIDE;

  virtual void PaintArrowButton(
      SkCanvas* canvas,
      const gfx::Rect& rect,
      Part direction,
      State state) const OVERRIDE;

  virtual void PaintScrollbarThumb(
      SkCanvas* canvas,
      Part part,
      State state,
      const gfx::Rect& rect) const OVERRIDE;

  SkBitmap* GetHorizontalBitmapNamed(int resource_id) const;

  // Cached images. Resource loader caches all retrieved bitmaps and keeps
  // ownership of the pointers.
  typedef std::map<int, SkBitmap*> SkImageMap;
  mutable SkImageMap horizontal_bitmaps_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeAura);
};

}  // namespace gfx

#endif  // UI_GFX_NATIVE_THEME_AURA_H_
