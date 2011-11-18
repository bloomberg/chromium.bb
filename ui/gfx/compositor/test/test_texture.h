// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_TEST_TEST_TEXTURE_H_
#define UI_GFX_COMPOSITOR_TEST_TEST_TEXTURE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/rect.h"

namespace ui {

// Texture implementation that TextCompositor creates. Doesn't actually draw
// anything.
class TestTexture : public ui::Texture {
 public:
  TestTexture();
  virtual ~TestTexture();

  // Number of textures that have been created.
  static void reset_live_count() { live_count_ = 0; }
  static int live_count() { return live_count_; }

  // Bounds of the last bitmap passed to SetCanvas.
  const gfx::Rect& bounds_of_last_paint() const {
    return bounds_of_last_paint_;
  }

  // ui::Texture
  virtual void SetCanvas(const SkCanvas& canvas,
                         const gfx::Point& origin,
                         const gfx::Size& overall_size) OVERRIDE;
  virtual void Draw(const ui::TextureDrawParams& params,
                    const gfx::Rect& clip_bounds) OVERRIDE;

 private:
  // Number of live instances.
  static int live_count_;

  gfx::Rect bounds_of_last_paint_;

  DISALLOW_COPY_AND_ASSIGN(TestTexture);
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_TEST_TEST_TEXTURE_H_
