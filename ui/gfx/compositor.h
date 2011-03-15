// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_H_
#define UI_GFX_COMPOSITOR_H_
#pragma once

#include "base/ref_counted.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
class Transform;

typedef unsigned int TextureID;

// Compositor object to take care of GPU painting.
// A Browser compositor object is responsible for generating the final
// displayable form of pixels comprising a single widget's contents. It draws an
// appropriately transformed texture for each transformed view in the widget's
// view hierarchy. The initial implementation uses GL for this purpose.
// Future CLs will adapt this to ongoing Skia development.
class Compositor : public base::RefCounted<Compositor> {
 public:
  // Create a compositor from the provided handle.
  static Compositor* Create(gfx::AcceleratedWidget widget);

  // Notifies the compositor that compositing is about to start.
  virtual void NotifyStart() = 0;

  // Notifies the compositor that compositing is complete.
  virtual void NotifyEnd() = 0;

  // Draws the given texture with the given transform.
  virtual void DrawTextureWithTransform(TextureID txt,
                                        const ui::Transform& transform) = 0;

  // Save the current transformation that can be restored with RestoreTransform.
  virtual void SaveTransform() = 0;

  // Restore a previously saved transformation using SaveTransform.
  virtual void RestoreTransform() = 0;

 protected:
  virtual ~Compositor() {}

 private:
  friend class base::RefCounted<Compositor>;
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_H_
