// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHADOW_H_
#define UI_AURA_SHELL_SHADOW_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/rect.h"

namespace ui {
class Layer;
}  // namespace ui

namespace aura_shell {
namespace internal {

class ImageGrid;

// Simple class that draws a drop shadow around content at given bounds.
class Shadow {
 public:
  Shadow();
  ~Shadow();

  // Returns |image_grid_|'s ui::Layer.  This is exposed so it can be added to
  // the same layer as the content and stacked below it.  SetContentBounds()
  // should be used to adjust the shadow's size and position (rather than
  // applying transformations to this layer).
  ui::Layer* layer() const;

  const gfx::Rect& content_bounds() const { return content_bounds_; }

  void Init();

  // Moves and resizes |image_grid_| to frame |content_bounds|.
  void SetContentBounds(const gfx::Rect& content_bounds);

 private:
  scoped_ptr<ImageGrid> image_grid_;

  // Bounds of the content that the shadow encloses.
  gfx::Rect content_bounds_;

  DISALLOW_COPY_AND_ASSIGN(Shadow);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SHADOW_H_
